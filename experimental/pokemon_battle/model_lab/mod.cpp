#include "gx_renderer.hpp"
#include "mods/service.hpp"
#include "mods/svc/actor.h"
#include "mods/svc/file.h"
#include "mods/svc/ui.h"
#include "d/d_com_inf_game.h"
#include "d/d_bg_s_gnd_chk.h"
#include "f_op/f_op_actor_mng.h"
#include "JSystem/J3DGraphBase/J3DDrawBuffer.h"
#include "JSystem/J3DGraphBase/J3DShape.h"
#include <cmath>
#include <memory>
#include <new>
#include <string>

DEFINE_MOD();
IMPORT_SERVICE(UiService, svc_ui);
IMPORT_SERVICE(FileService, svc_file);
IMPORT_SERVICE(ActorService, svc_actor);

namespace {
using namespace pokemon_battle::runtime;
std::shared_ptr<GxAsset> asset;
ProfileName profile=-1;
ActorHandle registration=0;
ActorId actor_id=fpcM_ERROR_PROCESS_ID_e;
std::size_t selected_clip=0;
std::uint64_t clip_revision=0;
UiElementHandle status_handle=0;
std::string status="Load a Pikachu .dpkm runtime pack.";
bool picking=false, alive=false, dirty=true, remove_requested=false;
std::uintptr_t generation=0;
void message(std::string text) { status=std::move(text); dirty=true; }
bool world_ready() {
    return dComIfGp_getPlayer(0) && !dComIfGp_isEnableNextStage() && !dComIfGp_event_runCheck();
}
class PreviewActor;
class Packet final : public J3DPacket {
public:
    PreviewActor* owner=nullptr;
    void draw() override;
};
class PreviewActor : public fopAc_ac_c {
public:
    std::shared_ptr<GxAsset> held;
    Packet packet;
    Pose pose;
    Mtx matrix{};
    double seconds=0;
    std::uint64_t revision=0;
    std::size_t clip=0;
    int create() {
        if(!fopAcM_CheckCondition(this,fopAcCnd_INIT_e)) {
            // The engine owns this memory. Standard placement new has the matching
            // no-op placement delete MSVC expects; default initialization preserves
            // engine-populated base fields, exactly as fopAcM_ct does on TARGET_PC.
            ::new (static_cast<void*>(this)) PreviewActor;
            fopAcM_OnCondition(this,fopAcCnd_INIT_e);
        }
        held=asset;
        if(!held || !world_ready()) { actor_id=fpcM_ERROR_PROCESS_ID_e; return cPhs_ERROR_e; }
        packet.owner=this;
        revision=clip_revision; clip=selected_clip;
        held->model().sample(clip,0,clip==0,pose);
        fopAcM_SetMtx(this,matrix);
        attention_info.flags=0;
        actor_id=fopAcM_GetID(this);
        remove_requested=false;
        message("Pikachu preview active. Select a clip to play it.");
        execute();
        return cPhs_COMPLEATE_e;
    }
    int execute() {
        if(remove_requested || !world_ready()) { fopAcM_delete(this); return 1; }
        if(revision!=clip_revision) { revision=clip_revision; clip=selected_clip; seconds=0; }
        if(!dComIfGp_isPauseFlag()) {
            held->model().sample(clip,seconds,clip==0,pose);
            // The pinned .749 simulation executes actors at 30 Hz (presentation is separate).
            seconds+=1.0/30.0;
        }
        mDoMtx_stack_c::transS(current.pos.x,current.pos.y,current.pos.z);
        mDoMtx_stack_c::YrotM(shape_angle.y);
        mDoMtx_stack_c::scaleM(100.f,100.f,100.f);
        MTXCopy(mDoMtx_stack_c::get(),matrix);
        eyePos=attention_info.position=current.pos;
        return 1;
    }
    int draw_actor() {
        if(held && !remove_requested && world_ready()) dComIfGd_getOpaList()->entryImm(&packet,0);
        return 1;
    }
    int destroy() {
        if(actor_id==fopAcM_GetID(this)) { actor_id=fpcM_ERROR_PROCESS_ID_e; remove_requested=false; }
        this->~PreviewActor();
        return 1;
    }
};
void Packet::draw() {
    if(!owner || !owner->held) return;
    Mtx view_model;
    MTXConcat(j3dSys.getViewMtx(),owner->matrix,view_model);
    j3dSys.reinitGX();
    J3DShape::resetVcdVatCache();
    owner->held->draw(owner->pose,view_model);
    j3dSys.reinitGX();
    J3DShape::resetVcdVatCache();
}
int create_actor(void* p) { return static_cast<PreviewActor*>(p)->create(); }
int delete_actor(void* p) { return static_cast<PreviewActor*>(p)->destroy(); }
int execute_actor(void* p) { return static_cast<PreviewActor*>(p)->execute(); }
int draw_actor(void* p) { return static_cast<PreviewActor*>(p)->draw_actor(); }
int can_delete(void*) { return 1; }
const ActorProfileDesc descriptor={
    "pkprev",7,sizeof(PreviewActor),fpcDwPi_OBJ_LBOX_e,
    fopAcStts_UNK_0x40000_e | fopAcStts_UNK_0x4000_e,
    fopAc_ACTOR_e,fopAc_CULLBOX_CUSTOM_e,
    create_actor,delete_actor,execute_actor,can_delete,draw_actor
};
bool has_actor() { return actor_id!=fpcM_ERROR_PROCESS_ID_e; }
bool cannot_load(ModContext*,void*) { return picking || has_actor(); }
bool cannot_spawn(ModContext*,void*) { return picking || !asset || has_actor() || !world_ready(); }
bool cannot_remove(ModContext*,void*) { return !has_actor() || remove_requested; }
bool cannot_clip(ModContext*,void*) { return !asset; }
void picked(ModContext*,ModResult result,const char* const* locations,std::uint32_t count,const char*,void* token) {
    if(!alive || reinterpret_cast<std::uintptr_t>(token)!=generation) return;
    picking=false;
    if(result!=MOD_OK || count!=1 || !locations) { message("No runtime pack selected."); return; }
    FileStreamHandle handle=0;
    try {
        std::uint64_t size=0, read=0;
        if(svc_file->open(mod_ctx,locations[0],FILE_OPEN_READ,&handle)!=MOD_OK) { message("Could not open runtime pack."); return; }
        if(svc_file->size(mod_ctx,handle,&size)!=MOD_OK || size<60 || size>max_pack_bytes) {
            svc_file->close(mod_ctx,handle); handle=0; message("Runtime pack size is invalid."); return;
        }
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        const auto read_result=svc_file->read(mod_ctx,handle,bytes.data(),size,&read);
        const auto close_result=svc_file->close(mod_ctx,handle); handle=0;
        if(read_result!=MOD_OK || close_result!=MOD_OK || read!=size) { message("Runtime pack could not be read completely."); return; }
        Model model; std::string error;
        if(!model.load(bytes,error)) { message(error); return; }
        auto replacement=std::make_shared<GxAsset>(std::move(model));
        asset=std::move(replacement); selected_clip=0; ++clip_revision;
        message("Loaded Pikachu: "+std::to_string(asset->model().clips().size())+" clips. Spawn beside Link, then use Next clip.");
    } catch(const std::exception& e) {
        if(handle) svc_file->close(mod_ctx,handle);
        message(std::string("Load failed: ")+e.what());
    }
}
void load(ModContext*,void*) {
    if(cannot_load(nullptr,nullptr)) return;
    const FileFilter filter{"Pikachu runtime model","dpkm"};
    FilePickOptions options=FILE_PICK_OPTIONS_INIT;
    options.filters=&filter; options.filter_count=1;
    picking=true;
    const auto result=svc_file->pick_file(mod_ctx,&options,picked,reinterpret_cast<void*>(generation));
    if(result!=MOD_OK) { picking=false; message("File picker unavailable."); }
}
void spawn(ModContext*,void*) {
    if(cannot_spawn(nullptr,nullptr)) return;
    auto* player=dComIfGp_getPlayer(0);
    const double angle=double(player->shape_angle.y)*3.14159265358979323846/32768.0;
    cXyz at=player->current.pos;
    at.x+=float(std::sin(angle)*140.); at.z+=float(std::cos(angle)*140.);
    cXyz probe=at; probe.y+=150.f;
    dBgS_ObjGndChk ground; ground.SetPos(&probe);
    const float height=dComIfG_Bgsp().GroundCross(&ground);
    if(!std::isfinite(height) || std::abs(height-at.y)>140.f) { message("No nearby ground for the preview."); return; }
    ActorSpawnParams params{};
    params.room_num=static_cast<std::int8_t>(fopAcM_GetRoomNo(player));
    params.position={at.x,height,at.z}; params.scale={1,1,1};
    params.angle.y=static_cast<std::int16_t>(std::uint16_t(player->shape_angle.y)+0x8000u);
    if(svc_actor->create_actor(mod_ctx,profile,&params,&actor_id)!=MOD_OK) {
        actor_id=fpcM_ERROR_PROCESS_ID_e; message("Pikachu preview could not spawn.");
    } else message("Pikachu preview requested.");
}
void remove(ModContext*,void*) {
    if(!has_actor()) return;
    // The owned actor observes this even if a synchronous deletion is temporarily unavailable.
    remove_requested=true;
    svc_actor->delete_actor(mod_ctx,actor_id);
    message("Removing Pikachu preview.");
}
void next(ModContext*,void*) {
    if(!asset) return;
    selected_clip=(selected_clip+1)%asset->model().clips().size(); ++clip_revision;
    message("Playing "+asset->model().clips()[selected_clip].name);
}
void idle(ModContext*,void*) {
    if(!asset) return;
    selected_clip=0; ++clip_revision; message("Playing "+asset->model().clips()[0].name);
}
ModResult update_panel(ModContext*,void*,ModError*) {
    if(!status_handle || !dirty) return MOD_OK;
    const auto result=svc_ui->elem_set_text(mod_ctx,status_handle,status.c_str());
    if(result==MOD_OK) dirty=false;
    return result;
}
ModResult button(UiElementHandle pane,const char* text,UiPressedFn action,UiPredicateFn disabled) {
    UiControlDesc control=UI_CONTROL_DESC_INIT;
    control.label=text; control.on_pressed=action; control.is_disabled=disabled;
    return svc_ui->pane_add_control(mod_ctx,pane,&control,nullptr);
}
ModResult build_panel(ModContext*,UiElementHandle pane,void*,ModError*) {
    status_handle=0; dirty=true;
    auto r=svc_ui->pane_add_section(mod_ctx,pane,"Pokemon Model Lab"); if(r!=MOD_OK) return r;
    r=svc_ui->pane_add_text(mod_ctx,pane,"Spawn an animated Pikachu preview beside Link. Choose a .dpkm model pack to begin.",nullptr); if(r!=MOD_OK) return r;
    r=svc_ui->pane_add_text(mod_ctx,pane,status.c_str(),&status_handle); if(r!=MOD_OK) return r;
    r=button(pane,"Load model pack",load,cannot_load); if(r!=MOD_OK) return r;
    r=button(pane,"Spawn beside Link",spawn,cannot_spawn); if(r!=MOD_OK) return r;
    r=button(pane,"Next clip",next,cannot_clip); if(r!=MOD_OK) return r;
    r=button(pane,"Play idle",idle,cannot_clip); if(r!=MOD_OK) return r;
    return button(pane,"Remove preview",remove,cannot_remove);
}
}
extern "C" {
MOD_EXPORT ModResult mod_initialize(ModError* error) {
    if(!svc_ui || !svc_file || !svc_actor) return mods::set_error(error,MOD_UNAVAILABLE,"UI, file and actor services required");
    ++generation; alive=true; picking=false;
    auto r=svc_actor->register_actor(mod_ctx,&descriptor,&profile,&registration);
    if(r!=MOD_OK) { alive=false; return r; }
    UiModsPanelDesc panel=UI_MODS_PANEL_DESC_INIT;
    panel.build=build_panel; panel.update=update_panel;
    r=svc_ui->register_mods_panel(mod_ctx,&panel);
    if(r!=MOD_OK) { svc_actor->unregister_actor(mod_ctx,registration); registration=0; alive=false; }
    return r;
}
MOD_EXPORT ModResult mod_update(ModError*) { return MOD_OK; }
MOD_EXPORT ModResult mod_shutdown(ModError*) {
    alive=false; ++generation;
    // Unregister synchronously deletes this profile's actors before releasing code/assets.
    if(registration) {
        const auto r=svc_actor->unregister_actor(mod_ctx,registration);
        if(r!=MOD_OK) return r;
    }
    registration=0; profile=-1; actor_id=fpcM_ERROR_PROCESS_ID_e;
    asset.reset(); picking=false; status_handle=0; remove_requested=false;
    message("Load a Pikachu .dpkm runtime pack.");
    return MOD_OK;
}
}
