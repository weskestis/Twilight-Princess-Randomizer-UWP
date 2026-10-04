$ErrorActionPreference = 'Stop'

function Read-Normalized([string]$Path) {
  return [IO.File]::ReadAllText($Path).Replace("`r`n", "`n")
}
function Write-Utf8([string]$Path, [string]$Text) {
  [IO.File]::WriteAllText($Path, $Text, [Text.UTF8Encoding]::new($false))
}

$src = $env:TPR_SRC
if (-not $src) { throw 'TPR_SRC is not set.' }

# ---- stale overlap force-clear ----
$overlapHeaderPath = "$src\include\f_op\f_op_overlap_mng.h"
$overlapHeader = Read-Normalized $overlapHeaderPath
$headerAnchor = @(
  'int fopOvlpM_ClearOfReq();',
  'overlap_request_class* fopOvlpM_Request(s16 i_procname, u16 i_peektime);',
  'int fopOvlpM_Cancel();'
) -join "`n"
$headerNew = @(
  'int fopOvlpM_ClearOfReq();',
  'overlap_request_class* fopOvlpM_Request(s16 i_procname, u16 i_peektime);',
  'int fopOvlpM_ForceClearStale();',
  'int fopOvlpM_Cancel();'
) -join "`n"
if (-not $overlapHeader.Contains($headerAnchor)) {
  throw 'Overlap manager header anchor changed before .720 force-clear.'
}
$overlapHeader = $overlapHeader.Replace($headerAnchor, $headerNew)
Write-Utf8 $overlapHeaderPath $overlapHeader

$overlapSourcePath = "$src\src\f_op\f_op_overlap_mng.cpp"
$overlapSource = Read-Normalized $overlapSourcePath
$sourceAnchor = @(
  'int fopOvlpM_Cancel() {',
  '    if (l_fopOvlpM_overlap[0] == NULL) {'
) -join "`n"
$sourceNew = @(
  'int fopOvlpM_ForceClearStale() {',
  '    overlap_request_class* req = l_fopOvlpM_overlap[0];',
  '    if (req == NULL || req->overlap_task == NULL || req->field_0x8 == 0 || req->field_0x4 != 1) {',
  '        return 0;',
  '    }',
  '',
  '    // Only the .720 watchdog calls this, after the destination scene/player have',
  '    // remained live for 300 frames with no event or next-stage request. At that point',
  '    // field_0x8 means the request is stuck in WaitOfFadeout. Bypass only the normal',
  '    // guard that would otherwise reject base.flag0/peektime and issue its outbound command.',
  '    req->peektime = 0;',
  '    fopOvlpM_SceneIsStart();',
  '    cReq_Command(&req->base, 2);',
  '    return 1;',
  '}',
  '',
  'int fopOvlpM_Cancel() {',
  '    if (l_fopOvlpM_overlap[0] == NULL) {'
) -join "`n"
if (-not $overlapSource.Contains($sourceAnchor)) {
  throw 'Overlap manager source anchor changed before .720 force-clear.'
}
$overlapSource = $overlapSource.Replace($sourceAnchor, $sourceNew)
Write-Utf8 $overlapSourcePath $overlapSource

# ---- randomizer breakable pending-state bridge ----
$breakablesPath = "$src\mods\randomizer\src\breakables.cpp"
if (-not (Test-Path $breakablesPath -PathType Leaf)) {
  throw 'Randomizer breakables.cpp is missing before .720 color restoration.'
}
$breakables = Read-Normalized $breakablesPath
if (-not $breakables.Contains('is_shuffled_and_uncollected')) {
  throw 'Randomizer pending/uncollected breakable predicate is missing before .720 color restoration.'
}
if (-not $breakables.Contains('tpr_breakable_marker_pending')) {
  $breakables += @'

extern "C" bool tpr_breakable_marker_pending(fopAc_ac_c* actor) noexcept {
    if (actor == nullptr) {
        return false;
    }
    try {
        return randomizer::breakables::is_shuffled_and_uncollected(actor);
    } catch (...) {
        return false;
    }
}
'@
}
Write-Utf8 $breakablesPath $breakables

# ---- gold randomized/unclaimed pots ----
$potPath = "$src\src\d\actor\d_a_obj_carry.cpp"
$pot = Read-Normalized $potPath
$potIncludeAnchor = '#include "d/actor/d_a_obj_carry.h"'
if (-not $pot.Contains($potIncludeAnchor)) {
  throw 'Carry actor include anchor changed before .720 gold-pot marker.'
}
if (-not $pot.Contains('tpr_breakable_marker_pending')) {
  $pot = $pot.Replace(
    $potIncludeAnchor,
    $potIncludeAnchor + "`n" + 'extern "C" bool tpr_breakable_marker_pending(fopAc_ac_c* actor) noexcept;')
}
$potDrawAnchor = @(
  '    g_env_light.settingTevStruct(8, &current.pos, &tevStr);',
  '    g_env_light.setLightTevColorType_MAJI(mpModel, &tevStr);',
  '',
  '    if (mType == TYPE_BOKKURI) {'
) -join "`n"
$potDrawNew = @(
  '    g_env_light.settingTevStruct(8, &current.pos, &tevStr);',
  '    g_env_light.setLightTevColorType_MAJI(mpModel, &tevStr);',
  '',
  '    // .720 randomized breakable marker: gold only while this pot still owns',
  '    // an unclaimed shuffled reward. Environment lighting runs first so a collected',
  '    // pot naturally returns to its vanilla color on the next draw/reload.',
  '    J3DGXColor* tprPotMarkerColor = nullptr;',
  '    J3DGXColor tprPotSavedMarkerColor{};',
  '    if (tpr_breakable_marker_pending(this)) {',
  '        J3DMaterial* material = mpModel->getModelData()->getMaterialNodePointer(0);',
  '        if (material != nullptr) {',
  '            tprPotMarkerColor = material->getTevKColor(0);',
  '            if (tprPotMarkerColor != nullptr) {',
  '                tprPotSavedMarkerColor = *tprPotMarkerColor;',
  '                tprPotMarkerColor->r = 255;',
  '                tprPotMarkerColor->g = 185;',
  '                tprPotMarkerColor->b = 42;',
  '                tprPotMarkerColor->a = 255;',
  '            }',
  '        }',
  '    }',
  '',
  '    if (mType == TYPE_BOKKURI) {'
) -join "`n"
if (-not $pot.Contains($potDrawAnchor)) {
  throw 'Carry actor draw anchor changed before .720 gold-pot marker.'
}
$pot = $pot.Replace($potDrawAnchor, $potDrawNew)
$potRestoreAnchor = @(
  '    mDoExt_modelUpdateDL(mpModel);',
  '',
  '    if (mType == TYPE_IRON_BALL) {'
) -join "`n"
$potRestoreNew = @(
  '    mDoExt_modelUpdateDL(mpModel);',
  '    if (tprPotMarkerColor != nullptr) {',
  '        *tprPotMarkerColor = tprPotSavedMarkerColor;',
  '    }',
  '',
  '    if (mType == TYPE_IRON_BALL) {'
) -join "`n"
if (-not $pot.Contains($potRestoreAnchor)) {
  throw 'Carry actor model-update anchor changed before .720 marker restore.'
}
$pot = $pot.Replace($potRestoreAnchor, $potRestoreNew)
Write-Utf8 $potPath $pot

# ---- green randomized/unclaimed pumpkins ----
$pumpkinPath = "$src\src\d\actor\d_a_obj_pumpkin.cpp"
$pumpkin = Read-Normalized $pumpkinPath
$pumpkinIncludeAnchor = '#include "d/actor/d_a_obj_pumpkin.h"'
if (-not $pumpkin.Contains($pumpkinIncludeAnchor)) {
  throw 'Pumpkin actor include anchor changed before .720 green marker.'
}
if (-not $pumpkin.Contains('tpr_breakable_marker_pending')) {
  $pumpkin = $pumpkin.Replace(
    $pumpkinIncludeAnchor,
    $pumpkinIncludeAnchor + "`n" + 'extern "C" bool tpr_breakable_marker_pending(fopAc_ac_c* actor) noexcept;')
}
$pumpkinDrawAnchor = @(
  '        g_env_light.settingTevStruct(0, &current.pos, &tevStr);',
  '        g_env_light.setLightTevColorType_MAJI(mpModel, &tevStr);',
  '        if (field_0xBA8 == 0) {'
) -join "`n"
$pumpkinDrawNew = @(
  '        g_env_light.settingTevStruct(0, &current.pos, &tevStr);',
  '        g_env_light.setLightTevColorType_MAJI(mpModel, &tevStr);',
  '',
  '        // .720 randomized breakable marker: green only while the shuffled reward',
  '        // remains unclaimed. The saved material color is restored immediately after',
  '        // this draw so shared materials cannot leak the marker into vanilla pumpkins.',
  '        J3DGXColor* tprPumpkinMarkerColor = nullptr;',
  '        J3DGXColor tprPumpkinSavedMarkerColor{};',
  '        if (tpr_breakable_marker_pending(this)) {',
  '            J3DMaterial* material = mpModel->getModelData()->getMaterialNodePointer(0);',
  '            if (material != nullptr) {',
  '                tprPumpkinMarkerColor = material->getTevKColor(0);',
  '                if (tprPumpkinMarkerColor != nullptr) {',
  '                    tprPumpkinSavedMarkerColor = *tprPumpkinMarkerColor;',
  '                    tprPumpkinMarkerColor->r = 24;',
  '                    tprPumpkinMarkerColor->g = 255;',
  '                    tprPumpkinMarkerColor->b = 48;',
  '                    tprPumpkinMarkerColor->a = 255;',
  '                }',
  '            }',
  '        }',
  '        if (field_0xBA8 == 0) {'
) -join "`n"
if (-not $pumpkin.Contains($pumpkinDrawAnchor)) {
  throw 'Pumpkin actor draw anchor changed before .720 green marker.'
}
$pumpkin = $pumpkin.Replace($pumpkinDrawAnchor, $pumpkinDrawNew)
$pumpkinRestoreAnchor = @(
  '        mDoExt_modelUpdateDL(mpModel);',
  '        mpModel->getModelData()->getMaterialNodePointer(0)->getShape()->show();'
) -join "`n"
$pumpkinRestoreNew = @(
  '        mDoExt_modelUpdateDL(mpModel);',
  '        if (tprPumpkinMarkerColor != nullptr) {',
  '            *tprPumpkinMarkerColor = tprPumpkinSavedMarkerColor;',
  '        }',
  '        mpModel->getModelData()->getMaterialNodePointer(0)->getShape()->show();'
) -join "`n"
if (-not $pumpkin.Contains($pumpkinRestoreAnchor)) {
  throw 'Pumpkin actor model-update anchor changed before .720 marker restore.'
}
$pumpkin = $pumpkin.Replace($pumpkinRestoreAnchor, $pumpkinRestoreNew)
Write-Utf8 $pumpkinPath $pumpkin

# Permanent regression guards.
$overlapVerify = Read-Normalized $overlapSourcePath
$breakablesVerify = Read-Normalized $breakablesPath
$potVerify = Read-Normalized $potPath
$pumpkinVerify = Read-Normalized $pumpkinPath
foreach ($marker in @(
  'fopOvlpM_ForceClearStale',
  'req->peektime = 0;',
  'cReq_Command(&req->base, 2);'
)) {
  if (-not $overlapVerify.Contains($marker)) { throw "Missing .720 overlap recovery marker: $marker" }
}
foreach ($marker in @(
  'tpr_breakable_marker_pending',
  'is_shuffled_and_uncollected'
)) {
  if (-not $breakablesVerify.Contains($marker)) { throw "Missing .720 breakable bridge marker: $marker" }
}
foreach ($marker in @(
  'tprPotMarkerColor->r = 255;',
  'tprPotMarkerColor->g = 185;',
  'tprPotMarkerColor->b = 42;',
  '*tprPotMarkerColor = tprPotSavedMarkerColor;'
)) {
  if (-not $potVerify.Contains($marker)) { throw "Missing .720 gold-pot lifecycle marker: $marker" }
}
foreach ($marker in @(
  'tprPumpkinMarkerColor->r = 24;',
  'tprPumpkinMarkerColor->g = 255;',
  'tprPumpkinMarkerColor->b = 48;',
  '*tprPumpkinMarkerColor = tprPumpkinSavedMarkerColor;'
)) {
  if (-not $pumpkinVerify.Contains($marker)) { throw "Missing .720 green-pumpkin lifecycle marker: $marker" }
}

git -C $src diff --check
if ($LASTEXITCODE -ne 0) { throw '.720 source transforms failed git diff --check.' }

Write-Host 'Applied .720 stale-overlap recovery and breakable color lifecycle hardening.'
