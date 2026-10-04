$ErrorActionPreference = 'Stop'

function Read-Normalized([string]$Path) {
  return [IO.File]::ReadAllText($Path).Replace("`r`n", "`n")
}
function Write-Utf8([string]$Path, [string]$Text) {
  [IO.File]::WriteAllText($Path, $Text, [Text.UTF8Encoding]::new($false))
}

$src = $env:TPR_SRC
if (-not $src) { throw 'TPR_SRC is not set.' }

# -----------------------------------------------------------------------------
# Main-menu hard-crash handoff
# -----------------------------------------------------------------------------
$prePath = "$src\src\dusk\ui\prelaunch.cpp"
$pre = Read-Normalized $prePath

if (-not $pre.Contains('#include <filesystem>')) {
  throw '.728 hard-crash report could not find prelaunch filesystem include.'
}
if (-not $pre.Contains('#include <fstream>')) {
  $pre = $pre.Replace('#include <filesystem>', '#include <filesystem>' + "`n" + '#include <fstream>')
}

$stateAnchor = 'PrelaunchState sPrelaunchState;'
if (-not $pre.Contains($stateAnchor)) {
  throw '.728 hard-crash report could not find prelaunch state anchor.'
}

$helpers = @'
#if defined(_UWP)
constexpr std::string_view kXboxSessionMarkerName = "xbox-session-active.txt";
constexpr std::string_view kXboxRuntimeFailureName = "xbox-runtime-failure.txt";
constexpr std::string_view kXboxRuntimeFailureHistoryName = "xbox-runtime-failures.log";

std::filesystem::path xbox_session_marker_path() {
    return ConfigPath / kXboxSessionMarkerName;
}

void xbox_arm_runtime_session() noexcept {
    try {
        std::filesystem::create_directories(ConfigPath);
        std::ofstream marker(xbox_session_marker_path(), std::ios::out | std::ios::trunc);
        marker << "session_state=gameplay_active\n";
        marker << "build=1.4.1.728\n";
    } catch (...) {
    }
}

void xbox_clear_runtime_session() noexcept {
    std::error_code ec;
    std::filesystem::remove(xbox_session_marker_path(), ec);
}

std::string xbox_read_text_file(const std::filesystem::path& path) {
    try {
        std::ifstream input(path, std::ios::in);
        if (!input) {
            return {};
        }
        return std::string(
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>());
    } catch (...) {
        return {};
    }
}

std::string xbox_latest_runtime_stage() {
    std::error_code ec;
    if (!std::filesystem::exists(ConfigPath, ec)) {
        return "unavailable";
    }

    std::filesystem::path newest;
    std::filesystem::file_time_type newestTime{};
    bool found = false;
    for (std::filesystem::directory_iterator it(ConfigPath, ec), end;
         !ec && it != end; it.increment(ec))
    {
        if (!it->is_regular_file(ec)) {
            continue;
        }
        const std::string name = it->path().filename().string();
        if (!name.starts_with("xbox-startup-stage-") || !name.ends_with(".txt")) {
            continue;
        }
        const auto writeTime = it->last_write_time(ec);
        if (ec) {
            ec.clear();
            continue;
        }
        if (!found || writeTime > newestTime) {
            newest = it->path();
            newestTime = writeTime;
            found = true;
        }
    }

    if (!found) {
        return "unavailable";
    }

    std::string stage = xbox_read_text_file(newest);
    while (!stage.empty() &&
           (stage.back() == '\n' || stage.back() == '\r' ||
            stage.back() == ' ' || stage.back() == '\t'))
    {
        stage.pop_back();
    }
    return stage.empty() ? "unavailable" : stage;
}

std::string xbox_rml_safe(std::string value) {
    for (char& ch : value) {
        if (ch == '<' || ch == '>' || ch == '&') {
            ch = ' ';
        }
    }
    return value;
}

std::optional<std::string> xbox_previous_hard_crash_report() {
    std::error_code ec;
    if (!std::filesystem::exists(xbox_session_marker_path(), ec)) {
        return std::nullopt;
    }

    const std::string stage = xbox_latest_runtime_stage();
    std::string report;
    report += "Twilight Princess Randomizer Xbox runtime failure report\n";
    report += "build=1.4.1.728\n";
    report += "failure_class=hard_process_termination\n";
    report += "failure_summary=The previous gameplay session ended without a clean shutdown.\n";
    report += "failure_hint=The last persisted runtime checkpoint is shown below.\n";
    report += "last_runtime_stage=" + stage + "\n";
    report += "source=next_launch_main_menu\n";

    try {
        std::filesystem::create_directories(ConfigPath);
        {
            std::ofstream latest(
                ConfigPath / kXboxRuntimeFailureName,
                std::ios::out | std::ios::trunc);
            latest << report;
        }
        {
            std::ofstream history(
                ConfigPath / kXboxRuntimeFailureHistoryName,
                std::ios::out | std::ios::app);
            history << "\n===== hard process termination =====\n" << report;
        }
    } catch (...) {
    }

    return report;
}
#endif
'@

if ($pre.Contains('kXboxSessionMarkerName')) {
  throw '.728 hard-crash main-menu handoff already present unexpectedly.'
}
$pre = $pre.Replace($stateAnchor, $stateAnchor + "`n`n" + $helpers)

# Arm immediately before Xbox activates the pending game mode.
# Use the actual activation call as the stable boundary instead of surrounding launcher formatting.
$activationCall = '            if (!gamemode::getGameModeManager().setCurrentGameMode(mPendingGameModeId)) {'
if (-not $pre.Contains($activationCall)) {
  throw '.728 hard-crash report could not find Xbox pending-mode activation call.'
}
$pre = $pre.Replace(
  $activationCall,
  '            xbox_arm_runtime_session();' + "`n" + $activationCall)

# Clean activation/callback rejection paths are not hard crashes.
$activationFailedStage =
  '                startup_guard::end_activity("prelaunch.play-activation-failed");'
if (-not $pre.Contains($activationFailedStage)) {
  throw '.728 hard-crash report could not find activation-failed checkpoint.'
}
$pre = $pre.Replace(
  $activationFailedStage,
  $activationFailedStage + "`n" + '                xbox_clear_runtime_session();')

$callbackFailedStage =
  '                startup_guard::end_activity("prelaunch.play-callback-failed");'
if (-not $pre.Contains($callbackFailedStage)) {
  throw '.728 hard-crash report could not find callback-failed checkpoint.'
}
$pre = $pre.Replace(
  $callbackFailedStage,
  $callbackFailedStage + "`n" + '                xbox_clear_runtime_session();')

# Clean return-to-menu is not a crash.
# Patch the reset call site instead of the prelaunch function body; the Xbox launcher
# source is heavily rewritten by earlier layers, while the reset handoff is stable.
$resetPath = "$src\src\m_Do\m_Do_Reset.cpp"
$reset = Read-Normalized $resetPath
$resetAnchor = '        dusk::ui::return_to_prelaunch();'
if (-not $reset.Contains($resetAnchor)) {
  throw '.728 hard-crash report could not find reset-to-prelaunch call.'
}
$reset = $reset.Replace(
  $resetAnchor,
  @'
#if defined(_UWP)
        {
            std::error_code ec;
            std::filesystem::remove(dusk::ConfigPath / "xbox-session-active.txt", ec);
        }
#endif
        dusk::ui::return_to_prelaunch();
'@)

if (-not $reset.Contains('#include "dusk/main.h"')) {
  $includeAnchor = '#include "dusk/ui/prelaunch.hpp"'
  if (-not $reset.Contains($includeAnchor)) {
    throw '.728 hard-crash report could not find reset include anchor.'
  }
  $reset = $reset.Replace(
    $includeAnchor,
    $includeAnchor + "`n" + '#include "dusk/main.h"')
}
if (-not $reset.Contains('#include <filesystem>')) {
  $reset = '#include <filesystem>' + "`n" + $reset
}
Write-Utf8 $resetPath $reset

# Make the existing Xbox main-menu diagnostic surface the hard crash automatically.
# Do this at startup_guard::previous_stage() so no launcher UI function needs patching.
$guardPath = "$src\src\dusk\startup_guard.hpp"
$guard = Read-Normalized $guardPath
$prevStart = $guard.IndexOf('inline std::string previous_stage() noexcept {')
$prevEnd = $guard.IndexOf('inline void stage(', $prevStart)
if ($prevStart -lt 0 -or $prevEnd -lt 0) {
  throw '.728 hard-crash report could not find startup_guard previous_stage boundaries.'
}

$previousStageImpl = @'
inline std::string previous_stage() noexcept {
    std::scoped_lock lock(detail::mutex);
#if defined(_UWP)
    if (!detail::markerPath.empty()) {
        std::error_code ec;
        const auto sessionMarker =
            detail::markerPath.parent_path() / "xbox-session-active.txt";
        if (std::filesystem::exists(sessionMarker, ec)) {
            const std::string lastStage =
                detail::previousStage.empty() ? "unavailable" : detail::previousStage;
            const std::string report =
                "Twilight Princess Randomizer Xbox runtime failure report\n"
                "build=1.4.1.728\n"
                "failure_class=hard_process_termination\n"
                "failure_summary=The previous gameplay session ended unexpectedly or was terminated before a clean shutdown.\n"
                "failure_hint=The last persisted runtime checkpoint is shown below.\n"
                "last_runtime_stage=" + lastStage + "\n"
                "source=next_launch_main_menu\n";
            try {
                {
                    std::ofstream latest(
                        detail::markerPath.parent_path() / "xbox-runtime-failure.txt",
                        std::ios::binary | std::ios::trunc);
                    latest << report;
                }
                {
                    std::ofstream history(
                        detail::markerPath.parent_path() / "xbox-runtime-failures.log",
                        std::ios::binary | std::ios::app);
                    history << "\n===== hard process termination =====\n" << report;
                }
            } catch (...) {
            }
            return "Xbox Runtime Failure - Previous Session | "
                   "hard_process_termination | Last runtime checkpoint: " +
                   lastStage + " | Full report: xbox-runtime-failure.txt";
        }
    }
#endif
    return detail::previousStage;
}

'@

$guard = $guard.Substring(0, $prevStart) + $previousStageImpl +
         $guard.Substring($prevEnd)
Write-Utf8 $guardPath $guard
Write-Utf8 $prePath $pre

# Contract verification.
$preVerify = Read-Normalized $prePath
$resetVerify = Read-Normalized $resetPath
$guardVerify = Read-Normalized $guardPath
foreach ($marker in @(
  'xbox-session-active.txt',
  'hard_process_termination',
  'xbox_previous_hard_crash_report',
  'xbox_arm_runtime_session',
  'setCurrentGameMode(mPendingGameModeId)',
  'xbox_clear_runtime_session',
  'prelaunch.play-activation-failed',
  'prelaunch.play-callback-failed',
  'xbox-runtime-failure.txt',
  'last_runtime_stage=')) {
  if (-not $preVerify.Contains($marker)) {
    throw "Missing .728 main-menu hard-crash marker: $marker"
  }
}
foreach ($marker in @(
  'Xbox Runtime Failure - Previous Session',
  'hard_process_termination',
  'Last runtime checkpoint:',
  'Full report: xbox-runtime-failure.txt',
  'xbox-runtime-failure.txt',
  'xbox-runtime-failures.log',
  'source=next_launch_main_menu')) {
  if (-not $guardVerify.Contains($marker)) {
    throw "Missing .728 startup journal hard-crash marker: $marker"
  }
}

foreach ($marker in @(
  'dusk::ui::return_to_prelaunch();',
  'xbox-session-active.txt')) {
  if (-not $resetVerify.Contains($marker)) {
    throw "Missing .728 reset-to-menu crash-marker cleanup: $marker"
  }
}

git -C $src diff --check
if ($LASTEXITCODE -ne 0) {
  throw '.728 main-menu hard-crash report failed git diff --check.'
}

Write-Host 'Applied .728 hard-crash handoff: failed launches and reset-to-menu clear the session marker; gameplay termination leaves it for next-launch reporting.'
