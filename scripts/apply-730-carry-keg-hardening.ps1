$ErrorActionPreference = 'Stop'

function Read-Normalized([string]$Path) {
  return [IO.File]::ReadAllText($Path).Replace("`r`n", "`n")
}
function Write-Utf8([string]$Path, [string]$Text) {
  [IO.File]::WriteAllText($Path, $Text, [Text.UTF8Encoding]::new($false))
}

$src = $env:TPR_SRC
if (-not $src) { throw 'TPR_SRC is not set.' }

$carryPath = "$src\src\d\actor\d_a_obj_carry.cpp"
$carry = Read-Normalized $carryPath

# Keep Randomizer pot marker code completely away from non-pot carry actors.
$pendingOld = '    if (tpr_breakable_marker_pending(this)) {'
$pendingNew = @'
    const bool tprRandomizedPotType =
        mType == TYPE_TSUBO || mType == TYPE_OOTSUBO ||
        mType == TYPE_TSUBO_2 || mType == TYPE_AOTSUBO ||
        mType == TYPE_TSUBO_S || mType == TYPE_TSUBO_B;
    if (tprRandomizedPotType && mpModel != nullptr &&
        mpModel->getModelData() != nullptr &&
        tpr_breakable_marker_pending(this))
    {
'@
if (-not $carry.Contains($pendingOld)) {
  throw '.730 could not find breakable marker call in carry draw.'
}
$carry = $carry.Replace($pendingOld, $pendingNew)

# Defensive guards for the generic box/barrel carry path.
$procCarryAnchor = @'
int daObjCarry_c::mode_proc_carry() {
    daPy_py_c* player = (daPy_py_c*)daPy_getPlayerActorClass();
'@
$procCarryNew = @'
int daObjCarry_c::mode_proc_carry() {
    daPy_py_c* player = (daPy_py_c*)daPy_getPlayerActorClass();
#if defined(_UWP)
    if (player == nullptr) {
        return 1;
    }
#endif
'@
if (-not $carry.Contains($procCarryAnchor)) {
  throw '.730 could not find mode_proc_carry player anchor.'
}
$carry = $carry.Replace($procCarryAnchor, $procCarryNew)

$modelCalcOld = @'
    if (mType == TYPE_KIBAKO || mType == TYPE_TARU) {
        mpModel->calc();
    }
'@
$modelCalcNew = @'
    if ((mType == TYPE_KIBAKO || mType == TYPE_TARU) && mpModel != nullptr) {
        mpModel->calc();
    }
'@
if (-not $carry.Contains($modelCalcOld)) {
  throw '.730 could not find box/barrel model calc anchor.'
}
$carry = $carry.Replace($modelCalcOld, $modelCalcNew)

# Persistent carry-object checkpoints. These write only for boxes/barrels and only
# on pickup/drop/break boundaries, so there is no per-frame filesystem churn.
$includeAnchor = '#include "d/actor/d_a_obj_carry.h"'
if (-not $carry.Contains($includeAnchor)) {
  throw '.730 could not find carry include anchor.'
}
if (-not $carry.Contains('#include "dusk/main.h"')) {
  $carry = $carry.Replace(
    $includeAnchor,
    $includeAnchor + "`n" + '#if defined(_UWP)' + "`n" +
    '#include "dusk/main.h"' + "`n" +
    '#include <filesystem>' + "`n" +
    '#include <fstream>' + "`n" +
    '#endif')
}

$helperAnchor = 'void daObjCarry_c::mode_init_carry() {'
if (-not $carry.Contains($helperAnchor)) {
  throw '.730 could not find carry-init function boundary.'
}
$helpers = @'
#if defined(_UWP)
static bool tpr_xbox_diag_carry_actor(const daObjCarry_c* actor) noexcept {
    return actor != nullptr &&
           (actor->getType() == daObjCarry_c::TYPE_KIBAKO ||
            actor->getType() == daObjCarry_c::TYPE_TARU);
}

static void tpr_xbox_carry_checkpoint(
    daObjCarry_c* actor, const char* phase) noexcept
{
    if (!tpr_xbox_diag_carry_actor(actor) || phase == nullptr) {
        return;
    }
    try {
        std::filesystem::create_directories(dusk::ConfigPath);
        std::ofstream marker(
            dusk::ConfigPath / "xbox-startup-stage-730.txt",
            std::ios::out | std::ios::trunc);
        marker << "carry." << phase
               << " type=" << actor->getType()
               << " mode=" << static_cast<int>(actor->mMode)
               << " id=" << static_cast<unsigned>(fopAcM_GetID(actor))
               << " room=" << static_cast<int>(fopAcM_GetRoomNo(actor))
               << " carry_now=" << (fopAcM_checkCarryNow(actor) ? 1 : 0)
               << " model=" << (actor->mpModel != nullptr ? 1 : 0)
               << "\n";
    } catch (...) {
    }
}
#else
static void tpr_xbox_carry_checkpoint(
    daObjCarry_c*, const char*) noexcept
{
}
#endif

'@
if ($carry.Contains('tpr_xbox_carry_checkpoint(')) {
  throw '.730 carry checkpoint helper already present unexpectedly.'
}
$carry = $carry.Replace($helperAnchor, $helpers + $helperAnchor)

# Pickup start/end checkpoints.
$carry = $carry.Replace(
  'void daObjCarry_c::mode_init_carry() {' + "`n" +
  '    mAcch.ClrMoveBGOnly();',
  'void daObjCarry_c::mode_init_carry() {' + "`n" +
  '    tpr_xbox_carry_checkpoint(this, "pickup-begin");' + "`n" +
  '    mAcch.ClrMoveBGOnly();')

$carryModeAnchor = '    mMode = MODE_CARRY;' + "`n" + '}'
if (-not $carry.Contains($carryModeAnchor)) {
  throw '.730 could not find carry-mode completion anchor.'
}
$carry = $carry.Replace(
  $carryModeAnchor,
  '    mMode = MODE_CARRY;' + "`n" +
  '    tpr_xbox_carry_checkpoint(this, "pickup-ready");' + "`n" +
  '}')

# The collision-offset call is the first player-side operation executed while
# carrying a normal barrel/box. Bracket it so a hard crash names the boundary.
$offsetAnchor = '        player->setGrabCollisionOffset(field_0xd08.x, field_0xd08.z, NULL);'
if (-not $carry.Contains($offsetAnchor)) {
  throw '.730 could not find grab collision-offset call.'
}
$carry = $carry.Replace(
  $offsetAnchor,
  '        tpr_xbox_carry_checkpoint(this, "grab-offset-begin");' + "`n" +
  $offsetAnchor + "`n" +
  '        tpr_xbox_carry_checkpoint(this, "grab-offset-ready");')

# Release/drop boundaries.
$dropAnchor = 'void daObjCarry_c::mode_init_drop(u8 param_0) {' + "`n" +
              '    mAcch.ClrMoveBGOnly();'
if (-not $carry.Contains($dropAnchor)) {
  throw '.730 could not find drop-init anchor.'
}
$carry = $carry.Replace(
  $dropAnchor,
  'void daObjCarry_c::mode_init_drop(u8 param_0) {' + "`n" +
  '    tpr_xbox_carry_checkpoint(this, "drop-begin");' + "`n" +
  '    mAcch.ClrMoveBGOnly();')

$dropReadyAnchor = '    mMode = MODE_DROP;' + "`n" +
                   '    field_0xdb4 = 0;'
if (-not $carry.Contains($dropReadyAnchor)) {
  throw '.730 could not find drop-ready anchor.'
}
$carry = $carry.Replace(
  $dropReadyAnchor,
  '    mMode = MODE_DROP;' + "`n" +
  '    field_0xdb4 = 0;' + "`n" +
  '    tpr_xbox_carry_checkpoint(this, "drop-ready");')

# Break boundaries, including the vanilla item spawn and carry cancellation.
$breakAnchor = 'void daObjCarry_c::obj_break(bool i_createItem, bool i_cancelCarry, bool i_doBreakEff) {' + "`n" +
               '    int item_no = getItemNo();'
if (-not $carry.Contains($breakAnchor)) {
  throw '.730 could not find obj_break anchor.'
}
$carry = $carry.Replace(
  $breakAnchor,
  'void daObjCarry_c::obj_break(bool i_createItem, bool i_cancelCarry, bool i_doBreakEff) {' + "`n" +
  '    tpr_xbox_carry_checkpoint(this, "break-begin");' + "`n" +
  '    int item_no = getItemNo();')

$breakEndAnchor = @'
    if (i_doBreakEff) {
        eff_break_call();
        se_break(NULL);
    }
}
'@
if (-not $carry.Contains($breakEndAnchor)) {
  throw '.730 could not find obj_break completion anchor.'
}
$breakEndNew = @'
    if (i_doBreakEff) {
        tpr_xbox_carry_checkpoint(this, "break-effect-begin");
        eff_break_call();
        se_break(NULL);
        tpr_xbox_carry_checkpoint(this, "break-effect-ready");
    }
    tpr_xbox_carry_checkpoint(this, "break-ready");
}
'@
$carry = $carry.Replace($breakEndAnchor, $breakEndNew)

Write-Utf8 $carryPath $carry

# Advance the hard-crash report/journal source to .730.
foreach ($versionPath in @(
  "$src\src\dusk\ui\prelaunch.cpp",
  "$src\src\dusk\startup_guard.hpp"))
{
  $versionText = Read-Normalized $versionPath
  $versionText = $versionText.Replace('1.4.1.729', '1.4.1.730')
  $versionText = $versionText.Replace(
    'xbox-startup-stage-729.txt',
    'xbox-startup-stage-730.txt')
  Write-Utf8 $versionPath $versionText
}

# Verification.
$verify = Read-Normalized $carryPath
foreach ($marker in @(
  'tprRandomizedPotType',
  'mType == TYPE_TSUBO',
  'mType == TYPE_TSUBO_B',
  'tpr_xbox_carry_checkpoint',
  'xbox-startup-stage-730.txt',
  'carry." << phase',
  'pickup-begin',
  'pickup-ready',
  'grab-offset-begin',
  'grab-offset-ready',
  'drop-begin',
  'drop-ready',
  'break-begin',
  'break-ready',
  '(mType == TYPE_KIBAKO || mType == TYPE_TARU) && mpModel != nullptr',
  'if (player == nullptr)')) {
  if (-not $verify.Contains($marker)) {
    throw "Missing .730 carry hardening marker: $marker"
  }
}

git -C $src diff --check
if ($LASTEXITCODE -ne 0) {
  throw '.730 carry-object hardening failed git diff --check.'
}

Write-Host 'Applied .730 non-pot marker isolation, box/barrel guards, and persistent carry crash checkpoints.'
