$ErrorActionPreference = 'Stop'

function Read-Normalized([string]$Path) {
  return [IO.File]::ReadAllText($Path).Replace("`r`n", "`n")
}
function Write-Utf8([string]$Path, [string]$Text) {
  [IO.File]::WriteAllText($Path, $Text, [Text.UTF8Encoding]::new($false))
}
function Find-Function([string]$Text, [string]$Signature) {
  $start = $Text.IndexOf($Signature)
  if ($start -lt 0) {
    throw ".730 could not find function: $Signature"
  }
  $open = $Text.IndexOf('{', $start)
  if ($open -lt 0) {
    throw ".730 could not find opening brace for: $Signature"
  }
  $depth = 0
  for ($i = $open; $i -lt $Text.Length; ++$i) {
    if ($Text[$i] -eq '{') {
      ++$depth
    } elseif ($Text[$i] -eq '}') {
      --$depth
      if ($depth -eq 0) {
        return @{
          Start = $start
          Open = $open
          Close = $i
          End = $i + 1
        }
      }
    }
  }
  throw ".730 could not find closing brace for: $Signature"
}

$src = $env:TPR_SRC
if (-not $src) { throw 'TPR_SRC is not set.' }

$carryPath = "$src\src\d\actor\d_a_obj_carry.cpp"
$carry = Read-Normalized $carryPath

# Keep Randomizer pot-marker code completely away from non-pot carry actors.
$pendingOld = '    if (tpr_breakable_marker_pending(this)) {'
if (-not $carry.Contains($pendingOld)) {
  throw '.730 could not find breakable marker call in carry draw.'
}
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
$carry = $carry.Replace($pendingOld, $pendingNew)

# Persistent runtime checkpoints use the existing startup_guard activity journal.
# Keep the activity open from carry-init through the first carry execution frame;
# if Xbox dies anywhere in that window, next launch reports carry.pickup-*.
$includeAnchor = '#include "d/actor/d_a_obj_carry.h"'
if (-not $carry.Contains($includeAnchor)) {
  throw '.730 could not find carry include anchor.'
}
if (-not $carry.Contains('#include "dusk/startup_guard.hpp"')) {
  $carry = $carry.Replace(
    $includeAnchor,
    $includeAnchor + "`n" +
    '#if defined(_UWP)' + "`n" +
    '#include "dusk/startup_guard.hpp"' + "`n" +
    '#endif')
}

$helperAnchor = 'void daObjCarry_c::mode_init_carry()'
if (-not $carry.Contains($helperAnchor)) {
  throw '.730 could not find carry-init function boundary.'
}
$helpers = @'
#if defined(_UWP)
static fpc_ProcID tprXboxCarryActivityId = fpcM_ERROR_PROCESS_ID_e;

static bool tpr_xbox_diag_carry_actor(const daObjCarry_c* actor) noexcept {
    return actor != nullptr &&
           (actor->getType() == daObjCarry_c::TYPE_KIBAKO ||
            actor->getType() == daObjCarry_c::TYPE_TARU);
}

static bool tpr_xbox_carry_activity_matches(
    const daObjCarry_c* actor) noexcept
{
    return tpr_xbox_diag_carry_actor(actor) &&
           tprXboxCarryActivityId == fopAcM_GetID(actor);
}

static void tpr_xbox_carry_begin(
    daObjCarry_c* actor, const char* phase) noexcept
{
    if (!tpr_xbox_diag_carry_actor(actor) || phase == nullptr) {
        return;
    }
    tprXboxCarryActivityId = fopAcM_GetID(actor);
    dusk::startup_guard::begin_activity(phase);
}

static void tpr_xbox_carry_stage(
    daObjCarry_c* actor, const char* phase) noexcept
{
    if (!tpr_xbox_carry_activity_matches(actor) || phase == nullptr) {
        return;
    }
    dusk::startup_guard::stage(phase);
}

static void tpr_xbox_carry_end(
    daObjCarry_c* actor, const char* phase) noexcept
{
    if (!tpr_xbox_carry_activity_matches(actor) || phase == nullptr) {
        return;
    }
    dusk::startup_guard::end_activity(phase);
    tprXboxCarryActivityId = fpcM_ERROR_PROCESS_ID_e;
}
#else
static void tpr_xbox_carry_begin(
    daObjCarry_c*, const char*) noexcept {}
static void tpr_xbox_carry_stage(
    daObjCarry_c*, const char*) noexcept {}
static void tpr_xbox_carry_end(
    daObjCarry_c*, const char*) noexcept {}
#endif

'@
if ($carry.Contains('tpr_xbox_carry_begin(')) {
  throw '.730 carry checkpoint helper already present unexpectedly.'
}
$helperPos = $carry.IndexOf($helperAnchor)
$carry = $carry.Substring(0, $helperPos) + $helpers +
         $carry.Substring($helperPos)

# Structural player-pointer guard in mode_proc_carry, independent of cast/spacing.
$proc = Find-Function $carry 'int daObjCarry_c::mode_proc_carry()'
$playerCall = 'daPy_getPlayerActorClass()'
$playerCallPos = $carry.IndexOf($playerCall, $proc.Open)
if ($playerCallPos -lt 0 -or $playerCallPos -gt $proc.Close) {
  throw '.730 could not find player acquisition inside mode_proc_carry.'
}
$playerLineEnd = $carry.IndexOf(";`n", $playerCallPos)
if ($playerLineEnd -lt 0 -or $playerLineEnd -gt $proc.Close) {
  throw '.730 could not find end of player acquisition line.'
}
$playerInsert = $playerLineEnd + 2
$playerGuard = @'
#if defined(_UWP)
    if (player == nullptr) {
        return 1;
    }
#endif
'@
$carry = $carry.Substring(0, $playerInsert) + $playerGuard +
         $carry.Substring($playerInsert)

# Defensive model guard for wooden boxes/barrels.
$modelPattern =
  'if\s*\(\s*mType\s*==\s*TYPE_KIBAKO\s*\|\|\s*mType\s*==\s*TYPE_TARU\s*\)\s*\{\s*mpModel->calc\(\);\s*\}'
$modelMatches = [regex]::Matches($carry, $modelPattern)
if ($modelMatches.Count -ne 1) {
  throw ".730 expected one box/barrel model calc block, found $($modelMatches.Count)."
}
$carry = [regex]::Replace(
  $carry,
  $modelPattern,
  'if ((mType == TYPE_KIBAKO || mType == TYPE_TARU) && mpModel != nullptr) {' +
  "`n" + '        mpModel->calc();' + "`n" + '    }',
  1)

# Carry pickup activity: begin before init, keep it active until the first
# mode_proc_carry collision-offset call succeeds.
$initCarry = Find-Function $carry 'void daObjCarry_c::mode_init_carry()'
$carry = $carry.Insert(
  $initCarry.Open + 1,
  "`n    tpr_xbox_carry_begin(this, \"carry.pickup-begin\");")
$initCarry = Find-Function $carry 'void daObjCarry_c::mode_init_carry()'
$carry = $carry.Insert(
  $initCarry.Close,
  '    tpr_xbox_carry_stage(this, "carry.pickup-init-ready");' + "`n")

# End the pickup activity after the first player collision-offset operation.
$proc = Find-Function $carry 'int daObjCarry_c::mode_proc_carry()'
$offsetPattern = 'player->setGrabCollisionOffset\s*\([^;]+\);'
$procText = $carry.Substring($proc.Open, $proc.Close - $proc.Open + 1)
$offsetMatch = [regex]::Match($procText, $offsetPattern)
if (-not $offsetMatch.Success) {
  throw '.730 could not find grab collision-offset call structurally.'
}
$offsetAbsStart = $proc.Open + $offsetMatch.Index
$offsetAbsEnd = $offsetAbsStart + $offsetMatch.Length
$offsetOriginal = $carry.Substring($offsetAbsStart, $offsetMatch.Length)
$offsetReplacement =
  'tpr_xbox_carry_stage(this, "carry.grab-offset-begin");' + "`n        " +
  $offsetOriginal + "`n        " +
  'tpr_xbox_carry_end(this, "carry.pickup-first-frame-ready");'
$carry = $carry.Substring(0, $offsetAbsStart) + $offsetReplacement +
         $carry.Substring($offsetAbsEnd)

# Drop activity boundaries.
$drop = Find-Function $carry 'void daObjCarry_c::mode_init_drop('
$carry = $carry.Insert(
  $drop.Open + 1,
  "`n    tpr_xbox_carry_begin(this, \"carry.drop-begin\");")
$drop = Find-Function $carry 'void daObjCarry_c::mode_init_drop('
$carry = $carry.Insert(
  $drop.Close,
  '    tpr_xbox_carry_end(this, "carry.drop-ready");' + "`n")

# Break activity boundaries and effect checkpoint.
$break = Find-Function $carry 'void daObjCarry_c::obj_break('
$carry = $carry.Insert(
  $break.Open + 1,
  "`n    tpr_xbox_carry_begin(this, \"carry.break-begin\");")
$break = Find-Function $carry 'void daObjCarry_c::obj_break('
$breakText = $carry.Substring($break.Open, $break.Close - $break.Open + 1)
$effectCall = '        eff_break_call();'
$effectRel = $breakText.IndexOf($effectCall)
if ($effectRel -lt 0) {
  throw '.730 could not find break effect call.'
}
$effectAbs = $break.Open + $effectRel
$effectReplacement =
  '        tpr_xbox_carry_stage(this, "carry.break-effect-begin");' + "`n" +
  $effectCall + "`n" +
  '        tpr_xbox_carry_stage(this, "carry.break-effect-ready");'
$carry = $carry.Substring(0, $effectAbs) + $effectReplacement +
         $carry.Substring($effectAbs + $effectCall.Length)
$break = Find-Function $carry 'void daObjCarry_c::obj_break('
$carry = $carry.Insert(
  $break.Close,
  '    tpr_xbox_carry_end(this, "carry.break-ready");' + "`n")

Write-Utf8 $carryPath $carry

# Advance the hard-crash journal/report source to .730 if an earlier layer
# still contains the .729 identifiers.
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
  'tpr_xbox_carry_begin',
  'tpr_xbox_carry_stage',
  'tpr_xbox_carry_end',
  'startup_guard::begin_activity',
  'startup_guard::end_activity',
  'carry.pickup-begin',
  'carry.pickup-init-ready',
  'carry.grab-offset-begin',
  'carry.pickup-first-frame-ready',
  'carry.drop-begin',
  'carry.drop-ready',
  'carry.break-begin',
  'carry.break-effect-begin',
  'carry.break-effect-ready',
  'carry.break-ready',
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
