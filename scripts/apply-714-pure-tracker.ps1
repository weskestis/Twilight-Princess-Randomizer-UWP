$ErrorActionPreference = 'Stop'

function Read-Normalized([string]$Path) {
  return [IO.File]::ReadAllText($Path).Replace("`r`n", "`n")
}
function Write-Utf8([string]$Path, [string]$Text) {
  [IO.File]::WriteAllText($Path, $Text, [Text.UTF8Encoding]::new($false))
}
function Join-Lines([string[]]$Lines) {
  return ($Lines -join [char]10)
}

$src = $env:TPR_SRC
$control = $env:TPR_CONTROL

$sessionPath = "$src\mods\randomizer\src\session.cpp"
$session = Read-Normalized $sessionPath
$includeOld = '#include "../generator/utility/text.hpp"'
$includeNew = Join-Lines @(
  '#include "../generator/utility/text.hpp"',
  '#include "../generator/randomizer.hpp"',
  '#include "paths.hpp"',
  '#include "f_op/f_op_actor_mng.h"',
  '#include "f_op/f_op_camera_mng.h"',
  '#include "f_pc/f_pc_manager.h"',
  '#include "d/d_camera.h"',
  '#include "d/d_s_play.h"'
)
if (-not $session.Contains($includeOld)) { throw 'Pure tracker generator include anchor changed.' }
$session = $session.Replace($includeOld, $includeNew)

$stdIncludeAnchor = '#include <cstdlib>'
if (-not $session.Contains($stdIncludeAnchor)) {
  throw 'Pure tracker standard include anchor changed.'
}
$trackerStdIncludes = @(
  '#include <algorithm>',
  '#include <array>',
  '#include <cstddef>',
  '#include <cstdint>',
  '#include <memory>',
  '#include <sstream>'
)
$missingTrackerStdIncludes = @(
  $trackerStdIncludes | Where-Object { -not $session.Contains($_) }
)
if ($missingTrackerStdIncludes.Count -gt 0) {
  $session = $session.Replace(
    $stdIncludeAnchor,
    (Join-Lines @($missingTrackerStdIncludes + $stdIncludeAnchor)))
}

$bridgeAnchor = 'constexpr const char* kSeedHashBlobName = "seed_hash";'
$bridge = Read-Normalized "$control\payloads\pure-tracker-bridge.inc"
if (-not $session.Contains($bridgeAnchor)) { throw 'Pure tracker bridge anchor changed.' }
$session = $session.Replace($bridgeAnchor, $bridgeAnchor + [char]10 + [char]10 + $bridge)
Write-Utf8 $sessionPath $session

$iconHeaderPath = "$src\src\dusk\ui\icon_provider.hpp"
$iconHeader = Read-Normalized $iconHeaderPath
$headerIncludesOld = Join-Lines @('#include <cstdint>','#include <optional>','#include <string>')
$headerIncludesNew = Join-Lines @('#include <cstdint>','#include <optional>','#include <string>','#include <vector>')
if (-not $iconHeader.Contains($headerIncludesOld)) { throw 'Pure tracker icon header include anchor changed.' }
$iconHeader = $iconHeader.Replace($headerIncludesOld, $headerIncludesNew)
$headerAnchor = 'void register_icon_texture_provider() noexcept;'
$headerNew = Join-Lines @(
  'struct RenderedItemIcon {',
  '    std::vector<uint8_t> rgba8;',
  '    uint32_t width = 0;',
  '    uint32_t height = 0;',
  '};',
  '',
  'std::optional<RenderedItemIcon> render_item_icon_pixels(uint8_t itemNo);',
  '',
  'void register_icon_texture_provider() noexcept;'
)
if (-not $iconHeader.Contains($headerAnchor)) { throw 'Pure tracker icon header declaration anchor changed.' }
$iconHeader = $iconHeader.Replace($headerAnchor, $headerNew)
Write-Utf8 $iconHeaderPath $iconHeader

$iconSourcePath = "$src\src\dusk\ui\icon_provider.cpp"
$iconSource = Read-Normalized $iconSourcePath
$publicAnchor = Join-Lines @('}  // namespace','','void register_icon_texture_provider() noexcept {')
$publicNew = Join-Lines @(
  '}  // namespace',
  '',
  'std::optional<RenderedItemIcon> render_item_icon_pixels(uint8_t itemNo) {',
  '    auto icon = render_item_icon(itemNo);',
  '    if (!icon) {',
  '        return std::nullopt;',
  '    }',
  '    return RenderedItemIcon{',
  '        .rgba8 = std::move(icon->pixels),',
  '        .width = icon->width,',
  '        .height = icon->height,',
  '    };',
  '}',
  '',
  'void register_icon_texture_provider() noexcept {'
)
if (-not $iconSource.Contains($publicAnchor)) { throw 'Pure tracker icon public wrapper anchor changed.' }
$iconSource = $iconSource.Replace($publicAnchor, $publicNew)
$stubAnchor = Join-Lines @('void register_icon_texture_provider() noexcept {}','void unregister_icon_texture_provider() noexcept {}')
$stubNew = Join-Lines @(
  'std::optional<RenderedItemIcon> render_item_icon_pixels(uint8_t) {',
  '    return std::nullopt;',
  '}',
  'void register_icon_texture_provider() noexcept {}',
  'void unregister_icon_texture_provider() noexcept {}'
)
if (-not $iconSource.Contains($stubAnchor)) { throw 'Pure tracker icon stub anchor changed.' }
$iconSource = $iconSource.Replace($stubAnchor, $stubNew)
Write-Utf8 $iconSourcePath $iconSource

$settingsHeaderPath = "$src\src\dusk\settings.h"
$settingsHeader = Read-Normalized $settingsHeaderPath
$settingsHeaderAnchor = Join-Lines @('        ConfigVar<bool> showInputViewer;','        ConfigVar<bool> showInputViewerGyro;')
$settingsHeaderNew = Join-Lines @(
  '        ConfigVar<bool> showInputViewer;',
  '        ConfigVar<bool> showInputViewerGyro;',
  '        ConfigVar<bool> showPureItemTracker;',
  '        ConfigVar<float> pureItemTrackerScale;',
  '        ConfigVar<int> pureItemTrackerIconSize;',
  '        ConfigVar<int> pureItemTrackerColumns;',
  '        ConfigVar<bool> pureItemTrackerLock;',
  '        ConfigVar<bool> pureItemTrackerResetLayoutRequested;'
)
if (-not $settingsHeader.Contains($settingsHeaderAnchor)) { throw 'Pure tracker settings header anchor changed.' }
$settingsHeader = $settingsHeader.Replace($settingsHeaderAnchor, $settingsHeaderNew)
Write-Utf8 $settingsHeaderPath $settingsHeader

$settingsSourcePath = "$src\src\dusk\settings.cpp"
$settingsSource = Read-Normalized $settingsSourcePath
$settingsInitAnchor = Join-Lines @(
  '        .showInputViewer {"game.showInputViewer", false},',
  '        .showInputViewerGyro {"game.showInputViewerGyro", false},'
)
$settingsInitNew = Join-Lines @(
  '        .showInputViewer {"game.showInputViewer", false},',
  '        .showInputViewerGyro {"game.showInputViewerGyro", false},',
  '        .showPureItemTracker {"game.showPureItemTracker", true},',
  '        .pureItemTrackerScale {"game.pureItemTrackerScale", 1.0f},',
  '        .pureItemTrackerIconSize {"game.pureItemTrackerIconSize", 56},',
  '        .pureItemTrackerColumns {"game.pureItemTrackerColumns", 6},',
  '        .pureItemTrackerLock {"game.pureItemTrackerLock", false},',
  '        .pureItemTrackerResetLayoutRequested {"game.pureItemTrackerResetLayoutRequested", false},'
)
if (-not $settingsSource.Contains($settingsInitAnchor)) { throw 'Pure tracker settings init anchor changed.' }
$settingsSource = $settingsSource.Replace($settingsInitAnchor, $settingsInitNew)
$settingsRegisterAnchor = Join-Lines @(
  '    Register(g_userSettings.game.showInputViewer);',
  '    Register(g_userSettings.game.showInputViewerGyro);'
)
$settingsRegisterNew = Join-Lines @(
  '    Register(g_userSettings.game.showInputViewer);',
  '    Register(g_userSettings.game.showInputViewerGyro);',
  '    Register(g_userSettings.game.showPureItemTracker);',
  '    Register(g_userSettings.game.pureItemTrackerScale);',
  '    Register(g_userSettings.game.pureItemTrackerIconSize);',
  '    Register(g_userSettings.game.pureItemTrackerColumns);',
  '    Register(g_userSettings.game.pureItemTrackerLock);',
  '    Register(g_userSettings.game.pureItemTrackerResetLayoutRequested);'
)
if (-not $settingsSource.Contains($settingsRegisterAnchor)) { throw 'Pure tracker settings register anchor changed.' }
$settingsSource = $settingsSource.Replace($settingsRegisterAnchor, $settingsRegisterNew)
Write-Utf8 $settingsSourcePath $settingsSource

$settingsUiPath = "$src\src\dusk\ui\settings.cpp"
$settingsUi = Read-Normalized $settingsUiPath
$settingsUiAnchor = Join-Lines @(
  '        config_bool_select(leftPane, rightPane, getSettings().game.showInputViewerGyro,',
  '            {',
  '                .key = "Show Gyro Input Viewer",',
  '                .helpText = "Show gyro sensor values in the input viewer.",',
  '                .isDisabled = [] { return !getSettings().game.showInputViewer; },',
  '            });',
  '        leftPane.add_section("Game");'
)
$settingsUiNew = Join-Lines @(
  '        config_bool_select(leftPane, rightPane, getSettings().game.showInputViewerGyro,',
  '            {',
  '                .key = "Show Gyro Input Viewer",',
  '                .helpText = "Show gyro sensor values in the input viewer.",',
  '                .isDisabled = [] { return !getSettings().game.showInputViewer; },',
  '            });',
  '',
  '        leftPane.add_section("Pure Item Tracker");',
  '        config_bool_select(leftPane, rightPane, getSettings().game.showPureItemTracker,',
  '            {',
  '                .key = "Show Pure Item Tracker",',
  '                .helpText = "Show the Ship-style transparent Randomizer item tracker while a seed is active.",',
  '            });',
  '        config_percent_select(leftPane, rightPane, getSettings().game.pureItemTrackerScale,',
  '            "Pure Tracker UI Scale",',
  '            "Scale the transparent icon grid, spacing, and count text.",',
  '            75, 150, 5);',
  '        config_int_select(leftPane, rightPane, getSettings().game.pureItemTrackerIconSize,',
  '            "Pure Tracker Icon Size",',
  '            "Set the base size of item icons before the grid fits itself to the window.",',
  '            32, 112, 4);',
  '        config_int_select(leftPane, rightPane, getSettings().game.pureItemTrackerColumns,',
  '            "Pure Tracker Columns",',
  '            "Choose how many item columns the tracker uses.",',
  '            3, 9, 1);',
  '        config_bool_select(leftPane, rightPane, getSettings().game.pureItemTrackerLock,',
  '            {',
  '                .key = "Lock Pure Tracker Position",',
  '                .helpText = "Prevent moving or resizing the tracker after placing it.",',
  '            });',
  '        auto& resetPureTrackerLayout = leftPane.add_button(ControlledButton::Props{',
  '            .text = "Reset Pure Tracker Layout",',
  '        });',
  '        leftPane.register_control(',
  '            resetPureTrackerLayout.on_pressed([] {',
  '                mDoAud_seStartMenu(kSoundItemChange);',
  '                getSettings().game.pureItemTrackerIconSize.setValue(56);',
  '                getSettings().game.pureItemTrackerColumns.setValue(6);',
  '                getSettings().game.pureItemTrackerScale.setValue(1.0f);',
  '                getSettings().game.pureItemTrackerResetLayoutRequested.setValue(true);',
  '                config::save();',
  '            }),',
  '            rightPane, [](Pane& pane) {',
  '                pane.clear();',
  '                pane.add_text("Restore the compact six-column transparent tracker layout and default icon size.");',
  '            });',
  '',
  '        leftPane.add_section("Game");'
)
if (-not $settingsUi.Contains($settingsUiAnchor)) { throw 'Pure tracker settings UI anchor changed.' }
$settingsUi = $settingsUi.Replace($settingsUiAnchor, $settingsUiNew)
Write-Utf8 $settingsUiPath $settingsUi

Copy-Item "$control\payloads\PureItemTracker.cpp" "$src\src\dusk\imgui\PureItemTracker.cpp" -Force
Copy-Item "$control\payloads\PureItemTracker.hpp" "$src\src\dusk\imgui\PureItemTracker.hpp" -Force

$filesPath = "$src\files.cmake"
$files = Read-Normalized $filesPath
$filesAnchor = Join-Lines @('        src/dusk/imgui/ImGuiEngine.cpp','        src/dusk/imgui/ImGuiEngine.hpp')
$filesNew = Join-Lines @(
  '        src/dusk/imgui/ImGuiEngine.cpp',
  '        src/dusk/imgui/ImGuiEngine.hpp',
  '        src/dusk/imgui/PureItemTracker.cpp',
  '        src/dusk/imgui/PureItemTracker.hpp'
)
if (-not $files.Contains($filesAnchor)) { throw 'Pure tracker files.cmake anchor changed.' }
$files = $files.Replace($filesAnchor, $filesNew)
Write-Utf8 $filesPath $files

$consolePath = "$src\src\dusk\imgui\ImGuiConsole.cpp"
$console = Read-Normalized $consolePath
$consoleInclude = '#include "ImGuiEngine.hpp"'
$consoleIncludeNew = Join-Lines @('#include "ImGuiEngine.hpp"','#include "PureItemTracker.hpp"')
if (-not $console.Contains($consoleInclude)) { throw 'Pure tracker ImGuiConsole include anchor changed.' }
$console = $console.Replace($consoleInclude, $consoleIncludeNew)
$consoleDraw = '        m_menuTools.ShowInputViewer();'
$consoleDrawNew = Join-Lines @('        m_menuTools.ShowInputViewer();','        draw_pure_item_tracker();','        draw_xbox_failure_report();')
if (-not $console.Contains($consoleDraw)) { throw 'Pure tracker ImGuiConsole draw anchor changed.' }
$console = $console.Replace($consoleDraw, $consoleDrawNew)
Write-Utf8 $consolePath $console

$all = (Read-Normalized "$src\src\dusk\imgui\PureItemTracker.cpp") +
       (Read-Normalized $sessionPath) +
       (Read-Normalized $settingsSourcePath) +
       (Read-Normalized $settingsUiPath) +
       (Read-Normalized $iconSourcePath) +
       (Read-Normalized $consolePath)
foreach ($marker in @(
  'Pure Item Tracker###PureItemTrackerV3',
  'ImVec4(1.0f, 1.0f, 1.0f, 0.20f)',
  'trackerDesignSize(440.0f, 500.0f)',
  'tpr_pure_tracker_slot_owned',
  'kPureTrackerStageStableFrames',
  'pureTrackerRuntimeReady',
  'dComIfGp_event_runCheck()',
  'needsWorldBuild',
  'ImGui::GetFrameCount()',
  'Xbox Runtime Failure###XboxTransitionFailure',
  'xbox-runtime-failure.txt',
  'tpr_xbox_transition_failure_pending',
  'clearXboxTransitionFades',
  'game.showPureItemTracker',
  'Pure Tracker Icon Size',
  'Pure Tracker Columns',
  'Lock Pure Tracker Position',
  'Reset Pure Tracker Layout',
  'render_item_icon_pixels',
  'draw_pure_item_tracker();'
)) {
  if (-not $all.Contains($marker)) { throw "Pure tracker port marker missing: $marker" }
}

Write-Host 'Applied .714 Ship-style Pure Item Tracker port.'
