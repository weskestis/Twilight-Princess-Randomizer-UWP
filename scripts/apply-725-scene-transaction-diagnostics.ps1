$ErrorActionPreference = 'Stop'

function Read-Normalized([string]$Path) {
  return [IO.File]::ReadAllText($Path).Replace("`r`n", "`n")
}
function Write-Utf8([string]$Path, [string]$Text) {
  [IO.File]::WriteAllText($Path, $Text, [Text.UTF8Encoding]::new($false))
}

$src = $env:TPR_SRC
if (-not $src) { throw 'TPR_SRC is not set.' }

# ---- node scene-change transaction diagnostics ----
$nodeReqPath = "$src\src\f_pc\f_pc_node_req.cpp"
$nodeReq = Read-Normalized $nodeReqPath

$nodeIncludeAnchor = '#include "f_pc/f_pc_debug_sv.h"'
if (-not $nodeReq.Contains($nodeIncludeAnchor)) {
  throw 'Node request include anchor changed before .725 diagnostics.'
}
if (-not $nodeReq.Contains('#include "f_pc/f_pc_name.h"')) {
  $nodeReq = $nodeReq.Replace(
    $nodeIncludeAnchor,
    $nodeIncludeAnchor + "`n" + '#include "f_pc/f_pc_name.h"')
}

$nodeQueueAnchor = 'static node_list_class l_fpcNdRq_Queue = {NULL, NULL, 0};'
$nodeDiag = @'
static node_create_request* tpr_xbox_find_scene_change_request() {
    request_node_class* req_node =
        reinterpret_cast<request_node_class*>(l_fpcNdRq_Queue.mpHead);
    while (req_node != nullptr) {
        node_create_request* req = req_node->node_create_req;
        if (req != nullptr && req->parameters == 2) {
            return req;
        }
        req_node = reinterpret_cast<request_node_class*>(req_node->node.mpNextNode);
    }
    return nullptr;
}

extern "C" int tpr_xbox_scene_change_present() noexcept {
    return tpr_xbox_find_scene_change_request() != nullptr ? 1 : 0;
}

extern "C" int tpr_xbox_scene_change_phase() noexcept {
    node_create_request* req = tpr_xbox_find_scene_change_request();
    return req != nullptr ? req->phase_request.id : -1;
}

extern "C" int tpr_xbox_scene_change_target() noexcept {
    node_create_request* req = tpr_xbox_find_scene_change_request();
    return req != nullptr ? req->name : -1;
}

extern "C" int tpr_xbox_scene_change_creating_id() noexcept {
    node_create_request* req = tpr_xbox_find_scene_change_request();
    return req != nullptr ? static_cast<int>(req->creating_id) : -1;
}

extern "C" int tpr_xbox_scene_change_old_id() noexcept {
    node_create_request* req = tpr_xbox_find_scene_change_request();
    return req != nullptr ? static_cast<int>(req->node_proc.id) : -1;
}

extern "C" int tpr_xbox_scene_change_creating() noexcept {
    node_create_request* req = tpr_xbox_find_scene_change_request();
    if (req == nullptr || req->creating_id == fpcM_ERROR_PROCESS_ID_e) {
        return 0;
    }
    return fpcCtRq_IsCreatingByID(req->creating_id) == TRUE ? 1 : 0;
}

extern "C" int tpr_xbox_scene_change_created_exists() noexcept {
    node_create_request* req = tpr_xbox_find_scene_change_request();
    if (req == nullptr || req->creating_id == fpcM_ERROR_PROCESS_ID_e) {
        return 0;
    }
    return fpcEx_IsExist(req->creating_id) == TRUE ? 1 : 0;
}
'@

if ($nodeReq.Contains('tpr_xbox_scene_change_phase()')) {
  throw '.725 node diagnostics already present unexpectedly.'
}
if (-not $nodeReq.Contains($nodeQueueAnchor)) {
  throw 'Node request queue anchor changed before .725 diagnostics.'
}
$nodeReq = $nodeReq.Replace(
  $nodeQueueAnchor,
  $nodeQueueAnchor + "`n`n" + $nodeDiag)
Write-Utf8 $nodeReqPath $nodeReq

# ---- deletion queue diagnostics ----
$deletorPath = "$src\src\f_pc\f_pc_deletor.cpp"
$deletor = Read-Normalized $deletorPath

$deleteAnchor = @'
BOOL fpcDt_IsComplete() {
    return fpcDtTg_IsEmpty();
}
'@

$deleteDiag = @'
BOOL fpcDt_IsComplete() {
    return fpcDtTg_IsEmpty();
}

static base_process_class* tpr_xbox_delete_queue_entry(int index, delete_tag_class** outTag) {
    if (index < 0) {
        return nullptr;
    }

    node_class* node = g_fpcDtTg_Queue.mpHead;
    int current = 0;
    while (node != nullptr) {
        if (current == index) {
            auto* tag = reinterpret_cast<delete_tag_class*>(node);
            if (outTag != nullptr) {
                *outTag = tag;
            }
            return static_cast<base_process_class*>(tag->base.mpTagData);
        }
        ++current;
        node = node->mpNextNode;
    }

    return nullptr;
}

extern "C" int tpr_xbox_delete_queue_size() noexcept {
    return g_fpcDtTg_Queue.mSize;
}

extern "C" int tpr_xbox_delete_queue_entry_name(int index) noexcept {
    base_process_class* proc = tpr_xbox_delete_queue_entry(index, nullptr);
    return proc != nullptr ? proc->name : -1;
}

extern "C" int tpr_xbox_delete_queue_entry_profname(int index) noexcept {
    base_process_class* proc = tpr_xbox_delete_queue_entry(index, nullptr);
    return proc != nullptr ? proc->profname : -1;
}

extern "C" int tpr_xbox_delete_queue_entry_id(int index) noexcept {
    base_process_class* proc = tpr_xbox_delete_queue_entry(index, nullptr);
    return proc != nullptr ? static_cast<int>(proc->id) : -1;
}

extern "C" int tpr_xbox_delete_queue_entry_state(int index) noexcept {
    base_process_class* proc = tpr_xbox_delete_queue_entry(index, nullptr);
    return proc != nullptr ? proc->state.init_state : -1;
}

extern "C" int tpr_xbox_delete_queue_entry_timer(int index) noexcept {
    delete_tag_class* tag = nullptr;
    (void)tpr_xbox_delete_queue_entry(index, &tag);
    return tag != nullptr ? tag->timer : -1;
}
'@

if ($deletor.Contains('tpr_xbox_delete_queue_size()')) {
  throw '.725 delete diagnostics already present unexpectedly.'
}
if (-not $deletor.Contains($deleteAnchor)) {
  throw 'Deletor completion anchor changed before .725 diagnostics.'
}
$deletor = $deletor.Replace($deleteAnchor, $deleteDiag)
Write-Utf8 $deletorPath $deletor

# Regression guards
$nodeVerify = Read-Normalized $nodeReqPath
$deleteVerify = Read-Normalized $deletorPath
foreach ($marker in @(
  'tpr_xbox_scene_change_present()',
  'tpr_xbox_scene_change_phase()',
  'tpr_xbox_scene_change_target()',
  'tpr_xbox_scene_change_creating_id()',
  'req->phase_request.id')) {
  if (-not $nodeVerify.Contains($marker)) {
    throw "Missing .725 scene transaction marker: $marker"
  }
}
foreach ($marker in @(
  'tpr_xbox_delete_queue_size()',
  'tpr_xbox_delete_queue_entry_name',
  'g_fpcDtTg_Queue.mSize',
  'tag->timer')) {
  if (-not $deleteVerify.Contains($marker)) {
    throw "Missing .725 delete queue marker: $marker"
  }
}

git -C $src diff --check
if ($LASTEXITCODE -ne 0) {
  throw '.725 engine diagnostics failed git diff --check.'
}

Write-Host 'Applied .725 scene-transaction/deletion-queue diagnostics.'
