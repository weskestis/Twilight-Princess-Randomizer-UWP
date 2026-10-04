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

$includeAnchor = '#include "f_pc/f_pc_debug_sv.h"'
if (-not $text.Contains($includeAnchor)) {
  throw 'Standard create include anchor changed before .726 diagnostics.'
}
$extraIncludes = @'
#include "f_pc/f_pc_create_tag.h"
#include "f_pc/f_pc_name.h"
#include "d/d_s_play.h"
'@
if (-not $text.Contains('#include "d/d_s_play.h"')) {
  $text = $text.Replace($includeAnchor, $includeAnchor + "`n" + $extraIncludes)
}

$structAnchor = @'
} standard_create_request_class;

int fpcSCtRq_phase_Load(standard_create_request_class* i_request) {
'@

$diag = @'
} standard_create_request_class;

static standard_create_request_class* tpr_xbox_find_create_request(int id) {
    node_class* node = g_fpcCtTg_Queue.mpHead;
    while (node != nullptr) {
        auto* tag = reinterpret_cast<create_tag*>(node);
        auto* base = static_cast<create_request*>(tag->base.mpTagData);
        if (base != nullptr && static_cast<int>(base->id) == id) {
            return reinterpret_cast<standard_create_request_class*>(base);
        }
        node = node->mpNextNode;
    }
    return nullptr;
}

static standard_create_request_class* tpr_xbox_create_queue_entry(int index) {
    if (index < 0) {
        return nullptr;
    }
    node_class* node = g_fpcCtTg_Queue.mpHead;
    int current = 0;
    while (node != nullptr) {
        if (current == index) {
            auto* tag = reinterpret_cast<create_tag*>(node);
            return reinterpret_cast<standard_create_request_class*>(tag->base.mpTagData);
        }
        ++current;
        node = node->mpNextNode;
    }
    return nullptr;
}

extern "C" int tpr_xbox_create_request_present(int id) noexcept {
    return tpr_xbox_find_create_request(id) != nullptr ? 1 : 0;
}

extern "C" int tpr_xbox_create_request_phase(int id) noexcept {
    auto* req = tpr_xbox_find_create_request(id);
    return req != nullptr ? req->phase_request.id : -1;
}

extern "C" int tpr_xbox_create_request_process_present(int id) noexcept {
    auto* req = tpr_xbox_find_create_request(id);
    return req != nullptr && req->base.process != nullptr ? 1 : 0;
}

extern "C" int tpr_xbox_create_request_process_name(int id) noexcept {
    auto* req = tpr_xbox_find_create_request(id);
    return req != nullptr && req->base.process != nullptr ? req->base.process->name : -1;
}

extern "C" int tpr_xbox_create_request_process_profname(int id) noexcept {
    auto* req = tpr_xbox_find_create_request(id);
    return req != nullptr && req->base.process != nullptr ? req->base.process->profname : -1;
}

extern "C" int tpr_xbox_create_request_process_init_state(int id) noexcept {
    auto* req = tpr_xbox_find_create_request(id);
    return req != nullptr && req->base.process != nullptr ? req->base.process->state.init_state : -1;
}

extern "C" int tpr_xbox_create_request_process_create_phase(int id) noexcept {
    auto* req = tpr_xbox_find_create_request(id);
    return req != nullptr && req->base.process != nullptr ? req->base.process->state.create_phase : -1;
}

extern "C" int tpr_xbox_create_request_layer_creating(int id) noexcept {
    auto* req = tpr_xbox_find_create_request(id);
    if (req == nullptr || req->base.process == nullptr) {
        return -1;
    }
    auto* nodeProc = reinterpret_cast<process_node_class*>(req->base.process);
    if (!fpcBs_Is_JustOfType(g_fpcNd_type, nodeProc->base.subtype)) {
        return 0;
    }
    return fpcLy_IsCreatingMesg(&nodeProc->layer) == TRUE ? 1 : 0;
}

extern "C" int tpr_xbox_play_scene_create_phase(int id) noexcept {
    auto* req = tpr_xbox_find_create_request(id);
    if (req == nullptr || req->base.process == nullptr) {
        return -1;
    }
    const s16 name = req->base.process->name;
    if (name != fpcNm_PLAY_SCENE_e && name != fpcNm_OPENING_SCENE_e) {
        return -1;
    }
    auto* play = reinterpret_cast<dScnPly_c*>(req->base.process);
    return play->field_0x1c4.id;
}

extern "C" int tpr_xbox_create_queue_size() noexcept {
    return g_fpcCtTg_Queue.mSize;
}

extern "C" int tpr_xbox_create_queue_entry_id(int index) noexcept {
    auto* req = tpr_xbox_create_queue_entry(index);
    return req != nullptr ? static_cast<int>(req->base.id) : -1;
}

extern "C" int tpr_xbox_create_queue_entry_target(int index) noexcept {
    auto* req = tpr_xbox_create_queue_entry(index);
    return req != nullptr ? req->process_name : -1;
}

extern "C" int tpr_xbox_create_queue_entry_phase(int index) noexcept {
    auto* req = tpr_xbox_create_queue_entry(index);
    return req != nullptr ? req->phase_request.id : -1;
}

extern "C" int tpr_xbox_create_queue_entry_process_name(int index) noexcept {
    auto* req = tpr_xbox_create_queue_entry(index);
    return req != nullptr && req->base.process != nullptr ? req->base.process->name : -1;
}

extern "C" int tpr_xbox_create_queue_entry_process_state(int index) noexcept {
    auto* req = tpr_xbox_create_queue_entry(index);
    return req != nullptr && req->base.process != nullptr ? req->base.process->state.init_state : -1;
}

int fpcSCtRq_phase_Load(standard_create_request_class* i_request) {
'@

if ($text.Contains('tpr_xbox_play_scene_create_phase')) {
  throw '.726 create diagnostics already present unexpectedly.'
}
if (-not $text.Contains($structAnchor)) {
  throw 'Standard create struct boundary changed before .726 diagnostics.'
}
$text = $text.Replace($structAnchor, $diag)
Write-Utf8 $path $text

foreach ($marker in @(
  'tpr_xbox_create_request_phase',
  'tpr_xbox_create_request_layer_creating',
  'tpr_xbox_play_scene_create_phase',
  'tpr_xbox_create_queue_size',
  'play->field_0x1c4.id')) {
  if (-not $text.Contains($marker)) {
    throw "Missing .726 creator diagnostic marker: $marker"
  }
}

git -C $src diff --check
if ($LASTEXITCODE -ne 0) {
  throw '.726 creator diagnostics failed git diff --check.'
}

Write-Host 'Applied .726 PLAY_SCENE creator diagnostics.'
