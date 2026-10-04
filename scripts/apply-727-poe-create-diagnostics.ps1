$ErrorActionPreference = 'Stop'

function Read-Normalized([string]$Path) {
  return [IO.File]::ReadAllText($Path).Replace("`r`n", "`n")
}
function Write-Utf8([string]$Path, [string]$Text) {
  [IO.File]::WriteAllText($Path, $Text, [Text.UTF8Encoding]::new($false))
}

$src = $env:TPR_SRC
if (-not $src) { throw 'TPR_SRC is not set.' }

$path = "$src\src\f_pc\f_pc_stdcreate_req.cpp"
$text = Read-Normalized $path

$includeAnchor = '#include "d/d_s_play.h"'
if (-not $text.Contains($includeAnchor)) {
  throw '.727 could not find d_s_play include from .726 diagnostics.'
}
if (-not $text.Contains('#include "d/actor/d_a_e_hp.h"')) {
  $text = $text.Replace(
    $includeAnchor,
    $includeAnchor + "`n" + '#include "d/actor/d_a_e_hp.h"')
}

$insertAnchor = 'int fpcSCtRq_phase_Load(standard_create_request_class* i_request) {'
$diag = @'
extern "C" int tpr_xbox_create_queue_entry_process_create_phase(int index) noexcept {
    auto* req = tpr_xbox_create_queue_entry(index);
    return req != nullptr && req->base.process != nullptr ? req->base.process->state.create_phase : -1;
}

static daE_HP_c* tpr_xbox_create_queue_poe(int index) {
    auto* req = tpr_xbox_create_queue_entry(index);
    if (req == nullptr || req->base.process == nullptr ||
        req->base.process->name != fpcNm_E_HP_e)
    {
        return nullptr;
    }
    return reinterpret_cast<daE_HP_c*>(req->base.process);
}

extern "C" int tpr_xbox_create_queue_entry_poe_resource_phase(int index) noexcept {
    auto* poe = tpr_xbox_create_queue_poe(index);
    return poe != nullptr ? poe->mPhaseReq.id : -1;
}

extern "C" int tpr_xbox_create_queue_entry_poe_heap_present(int index) noexcept {
    auto* poe = tpr_xbox_create_queue_poe(index);
    return poe != nullptr && poe->heap != nullptr ? 1 : 0;
}

extern "C" int tpr_xbox_create_queue_entry_poe_morfso_present(int index) noexcept {
    auto* poe = tpr_xbox_create_queue_poe(index);
    return poe != nullptr && poe->mpMorfSO != nullptr ? 1 : 0;
}

extern "C" int tpr_xbox_create_queue_entry_poe_model_present(int index) noexcept {
    auto* poe = tpr_xbox_create_queue_poe(index);
    return poe != nullptr && poe->mpModel != nullptr ? 1 : 0;
}

extern "C" int tpr_xbox_create_queue_entry_poe_morf_present(int index) noexcept {
    auto* poe = tpr_xbox_create_queue_poe(index);
    return poe != nullptr && poe->mpMorf != nullptr ? 1 : 0;
}
'@

if ($text.Contains('tpr_xbox_create_queue_entry_poe_resource_phase')) {
  throw '.727 Poe diagnostics already present unexpectedly.'
}
if (-not $text.Contains($insertAnchor)) {
  throw '.727 stable create-function boundary changed.'
}
$text = $text.Replace(
  $insertAnchor,
  $diag + "`n`n" + $insertAnchor)
Write-Utf8 $path $text

foreach ($marker in @(
  'tpr_xbox_create_queue_entry_poe_resource_phase',
  'poe->mPhaseReq.id',
  'poe->heap != nullptr',
  'poe->mpMorfSO != nullptr',
  'poe->mpModel != nullptr',
  'poe->mpMorf != nullptr')) {
  if (-not $text.Contains($marker)) {
    throw "Missing .727 Poe diagnostic marker: $marker"
  }
}

git -C $src diff --check
if ($LASTEXITCODE -ne 0) {
  throw '.727 Poe creator diagnostics failed git diff --check.'
}

Write-Host 'Applied .727 Poe create/resource diagnostics.'
