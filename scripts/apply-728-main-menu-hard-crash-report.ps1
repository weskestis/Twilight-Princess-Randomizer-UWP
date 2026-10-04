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
bool sXboxHardCrashModalPresented = false;

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
$returnAnchor = @'
void return_to_prelaunch() noexcept {
    close_all_documents();
'@
if (-not $pre.Contains($returnAnchor)) {
  throw '.728 hard-crash report could not find return-to-prelaunch anchor.'
}
$pre = $pre.Replace(
  $returnAnchor,
  @'
void return_to_prelaunch() noexcept {
#if defined(_UWP)
    xbox_clear_runtime_session();
#endif
    close_all_documents();
'@)

# Automatically surface a previous unclean gameplay session on the main menu.
$showAnchor = @'
void Prelaunch::show() {
    Document::show();
    mDocument->SetAttribute("open", "");
    mRoot->SetAttribute("open", "");
'@
if (-not $pre.Contains($showAnchor)) {
  throw '.728 hard-crash report could not find Prelaunch::show anchor.'
}
$showReplacement = @'
void Prelaunch::show() {
    Document::show();
    mDocument->SetAttribute("open", "");
    mRoot->SetAttribute("open", "");

#if defined(_UWP)
    if (!sXboxHardCrashModalPresented) {
        if (const auto report = xbox_previous_hard_crash_report(); report.has_value()) {
            sXboxHardCrashModalPresented = true;
            const std::string stage = xbox_rml_safe(xbox_latest_runtime_stage());
            const auto dismiss = [](Modal& modal) {
                xbox_clear_runtime_session();
                modal.pop();
            };
            push(std::make_unique<Modal>(Modal::Props{
                .title = "Xbox Runtime Failure - Previous Session",
                .bodyRml = fmt::format(
                    "The previous gameplay session ended unexpectedly or was terminated before "
                    "Dusklight could shut it down cleanly.<br/><br/>"
                    "<b>Failure class:</b> hard_process_termination<br/>"
                    "<b>Last runtime checkpoint:</b> {}<br/><br/>"
                    "The full report was saved as <b>xbox-runtime-failure.txt</b> in LocalState.",
                    stage),
                .actions = {
                    ModalAction{
                        .label = "Dismiss",
                        .onPressed = dismiss,
                    },
                },
                .onDismiss = dismiss,
            }));
            return;
        }
    }
#endif
'@
$pre = $pre.Replace($showAnchor, $showReplacement)

Write-Utf8 $prePath $pre

# -----------------------------------------------------------------------------
# Clean process exit clears the active gameplay marker.
# A hard process crash never reaches this code, so the marker survives.
# -----------------------------------------------------------------------------
$mainPath = "$src\src\dusk\main.cpp"
$main = Read-Normalized $mainPath

if (-not $main.Contains('#include "dusk/config.hpp"')) {
  $main = $main.Replace(
    '#include "dusk/main.h"',
    '#include "dusk/main.h"' + "`n" + '#include "dusk/config.hpp"')
}

$namespaceAnchor = @'
namespace {

bool RestartProcess(int argc, char* argv[]) {
'@
if (-not $main.Contains($namespaceAnchor)) {
  throw '.728 hard-crash report could not find main anonymous namespace anchor.'
}
$mainHelpers = @'
namespace {

void ClearXboxRuntimeSessionOnCleanExit() noexcept {
#if defined(_UWP)
    std::error_code ec;
    std::filesystem::remove(dusk::ConfigPath / "xbox-session-active.txt", ec);
#endif
}

bool RestartProcess(int argc, char* argv[]) {
'@
$main = $main.Replace($namespaceAnchor, $mainHelpers)

$windowsResultAnchor = @'
    const int result = game_main(argc, argv);
    if constexpr (dusk::SupportsProcessRestart) {
'@
if (-not $main.Contains($windowsResultAnchor)) {
  throw '.728 hard-crash report could not find Windows clean-exit anchor.'
}
$main = $main.Replace(
  $windowsResultAnchor,
  @'
    const int result = game_main(argc, argv);
    if (result == 0) {
        ClearXboxRuntimeSessionOnCleanExit();
    }
    if constexpr (dusk::SupportsProcessRestart) {
'@)

$nonWindowsAnchor = @'
    const int result = game_main(argc, argv);
    if (dusk::RestartRequested && RestartProcess(argc, argv)) {
'@
if ($main.Contains($nonWindowsAnchor)) {
  $main = $main.Replace(
    $nonWindowsAnchor,
    @'
    const int result = game_main(argc, argv);
    if (result == 0) {
        ClearXboxRuntimeSessionOnCleanExit();
    }
    if (dusk::RestartRequested && RestartProcess(argc, argv)) {
'@)
}

Write-Utf8 $mainPath $main

# Contract verification.
$preVerify = Read-Normalized $prePath
$mainVerify = Read-Normalized $mainPath
foreach ($marker in @(
  'xbox-session-active.txt',
  'hard_process_termination',
  'Xbox Runtime Failure - Previous Session',
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
  'ClearXboxRuntimeSessionOnCleanExit',
  'xbox-session-active.txt')) {
  if (-not $mainVerify.Contains($marker)) {
    throw "Missing .728 clean-exit marker: $marker"
  }
}

git -C $src diff --check
if ($LASTEXITCODE -ne 0) {
  throw '.728 main-menu hard-crash report failed git diff --check.'
}

Write-Host 'Applied .728 main-menu hard-crash handoff.'
