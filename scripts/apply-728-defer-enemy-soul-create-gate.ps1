$ErrorActionPreference = 'Stop'

$roots = @(
  "$env:TPR_SRC\mods\randomizer",
  "$env:TPR_SRC\src\dusk\randomizer"
) | Where-Object { Test-Path $_ -PathType Container }

if ($roots.Count -eq 0) {
  throw '.728 could not locate the assembled Randomizer source tree.'
}

$sourceFiles = @()
foreach ($root in $roots) {
  $sourceFiles += Get-ChildItem $root -Recurse -File |
    Where-Object { $_.Extension -in '.cpp','.cc','.cxx','.hpp','.h' }
}

$matches = @()
foreach ($file in $sourceFiles) {
  $lines = [IO.File]::ReadAllLines($file.FullName)
  for ($i = 0; $i -lt $lines.Length; ++$i) {
    $line = $lines[$i]
    if ($line.Contains('fpcMtd_Create_EnemySouls') -and
        ($line.Contains('add_pre') -or
         $line.Contains('hook_add_pre') -or
         $line.Contains('ADD_HOOK_PRE')))
    {
      $matches += [pscustomobject]@{
        File = $file.FullName
        Line = $i
        Text = $line
      }
    }
  }
}

if ($matches.Count -ne 1) {
  Write-Host "Found $($matches.Count) candidate fpcMtd_Create_EnemySouls pre-hook registrations."
  foreach ($m in $matches) {
    Write-Host "$($m.File):$($m.Line + 1): $($m.Text)"
  }
  throw '.728 requires exactly one Enemy Souls pre-create hook registration.'
}

$target = $matches[0]
$lines = [IO.File]::ReadAllLines($target.File)
$indent = ([regex]::Match($lines[$target.Line], '^\s*')).Value
$lines[$target.Line] =
  $indent + '// Xbox .728: defer Enemy Soul gating until execute; actor create must finish.'
[IO.File]::WriteAllLines($target.File, $lines, [Text.UTF8Encoding]::new($false))

$combined = ($sourceFiles | ForEach-Object {
  try { [IO.File]::ReadAllText($_.FullName) } catch { '' }
}) -join "`n"

if ($combined -match '(?m)^\s*.*(?:add_pre|hook_add_pre|ADD_HOOK_PRE).*fpcMtd_Create_EnemySouls') {
  throw '.728 Enemy Souls pre-create hook registration is still active.'
}
if ($combined -notmatch 'fopAc_Execute_EnemySouls') {
  throw '.728 lost the Enemy Souls execution gate hook.'
}
if ($combined -notmatch 'fopAc_Create_EnemySouls') {
  throw '.728 lost the Enemy Souls post-create tracking hook.'
}
if ($combined -notmatch 'Fail-closed Enemy Soul gate blocked') {
  throw '.728 unexpectedly lost Enemy Soul fail-closed runtime logic.'
}

git -C $env:TPR_SRC diff --check
if ($LASTEXITCODE -ne 0) {
  throw '.728 Enemy Soul create-gate deferral failed git diff --check.'
}

Write-Host "Applied .728 fix: removed Enemy Souls pre-create gate at $($target.File)."
Write-Host 'Enemy Soul execute gating and post-create/first-defeat tracking remain active.'
