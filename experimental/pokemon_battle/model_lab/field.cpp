#include "field.hpp"
#include "field_effects.hpp"
#include "spawn_context.hpp"
#include "party_save.hpp"
#include "../field/combat.hpp"
#include "mods/service.hpp"
#include "mods/svc/actor.h"
#include "mods/svc/hook.hpp"
#include "mods/svc/save.h"
#include "d/d_bg_s_acch.h"
#include "d/d_bg_s_gnd_chk.h"
#include "d/d_bg_s_lin_chk.h"
#include "d/d_cc_d.h"
#include "d/d_menu_window.h"
#include "m_Do/m_Do_controller_pad.h"
#include "JSystem/J3DGraphBase/J3DDrawBuffer.h"
#include "JSystem/J3DGraphBase/J3DShape.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <new>
#include <string>

extern const ActorService* svc_actor;
DEFINE_HOOK(&dMw_c::key_wait_proc, PokemonFieldInput);

namespace pokemon_battle::field {
namespace {
using namespace runtime;
constexpr ActorId no_actor=fpcM_ERROR_PROCESS_ID_e;
constexpr std::size_t companion_slot=3;
struct Storage final:PartyStorage {
    bool write(std::span<const std::uint8_t> bytes) override {
        return svc_save && svc_save->set_blob(mod_ctx,party_blob_name,bytes.data(),bytes.size())==MOD_OK;
    }
} storage;
PartySession party(storage);
struct Ticket {
    ActorId id=no_actor;
    std::uint64_t serial=0,epoch=0;
    Creature creature{};
    bool reserved=false,remove=false;
};
std::array<Ticket,4> tickets{};
std::uint64_t next_serial=1;
ProfileName field_profile=-1;
ActorHandle registration=0;
SaveObserverHandle save_observer=0;
std::shared_ptr<GxAsset> shared_asset;
UiElementHandle status_element=0,party_element=0;
std::string status="Load a Zelda save to begin.",last_party_text;
bool dirty=true,read_failed=false,initialized=false;
int save_slot=-1;
ActorId attempted_player=no_actor;

bool scene_ready() { return player_scene_ready(dComIfGp_getPlayer(0)); }
bool action_ready() { return scene_ready() && !dComIfGp_event_runCheck() && dComIfGs_getLife()>0; }
void message(std::string text,bool toast=false) {
    status=std::move(text); dirty=true;
    if(toast && svc_ui) {
        UiToastDesc desc=UI_TOAST_DESC_INIT;
        desc.title_rml="Pokemon"; desc.body_rml=status.c_str(); desc.duration_ms=2500;
        svc_ui->push_toast(mod_ctx,&desc);
    }
}
void remove_all() { for(auto& ticket:tickets) if(ticket.reserved) ticket.remove=true; party.cancel_throw(); }
void load_party() {
    remove_all(); party.deactivate(); read_failed=false;
    Party loaded; std::string error;
    const auto result=load_saved_party(svc_save,mod_ctx,loaded,error);
    if(result==LoadResult::Unavailable) { message("A writable Zelda save slot is required."); return; }
    if(result==LoadResult::Invalid) { read_failed=true; message(error); return; }
    party.activate(loaded); message("Party loaded. Select a Pokemon to summon, or start a wild encounter.");
    if(result==LoadResult::Created) message("24 Poke Balls ready. Start a wild encounter, weaken it, then throw a ball.");
}
void on_new_or_loaded(ModContext*,std::uint32_t slot,void*) { save_slot=static_cast<int>(slot); load_party(); }
void on_written(ModContext*,std::uint32_t slot,void*) {
    // The host flushes the already-staged blob after this notification. Do not
    // write stale memory if Save As changed the active slot.
    if(save_slot<0) save_slot=static_cast<int>(slot);
    else if(static_cast<int>(slot)!=save_slot) { save_slot=static_cast<int>(slot); load_party(); }
}
float distance(const cXyz& a,const cXyz& b) {
    const float x=a.x-b.x,y=a.y-b.y,z=a.z-b.z; return std::sqrt(x*x+y*y+z*z);
}
bool clear_line(const cXyz& a,const cXyz& b,const fopAc_ac_c* owner) {
    dBgS_ObjLinChk line; line.Set(&a,&b,owner); return !dComIfG_Bgsp().LineCross(&line);
}
bool ground_at(cXyz& position,float max_step=80.f) {
    cXyz probe=position; probe.y+=100;
    dBgS_ObjGndChk ground; ground.SetPos(&probe);
    const float y=dComIfG_Bgsp().GroundCross(&ground);
    if(!std::isfinite(y) || std::abs(y-position.y)>max_step) return false;
    position.y=y; return true;
}
class FieldActor;
class FieldPacket final:public J3DPacket {
public:
    FieldActor* owner=nullptr;
    void draw() override;
};
FieldActor* actor_for(std::size_t slot);
class FieldActor:public fopAc_ac_c {
public:
    std::shared_ptr<GxAsset> held;
    FieldPacket packet;
    Pose pose;
    Combat combat;
    dCcD_Stts collision_status;
    dCcD_Sph body,attack;
    dBgS_ObjAcch ground;
    dBgS_AcchCir wall;
    Mtx matrix{};
    std::size_t slot=0;
    std::uint64_t serial=0,epoch=0;
    ActorId target_id=no_actor,guarded_from=no_actor,weakened_target=no_actor;
    int guard_timer=0,weaken_timer=0,ball_ticks=-1,animation_clip=-1;
    double seconds=0;
    cXyz ball_start{},ball_end{},ball_position{},spark_end{};
    bool companion=false,closed=false;

    int create() {
        if(!fopAcM_CheckCondition(this,fopAcCnd_INIT_e)) {
            ::new(static_cast<void*>(this)) FieldActor;
            fopAcM_OnCondition(this,fopAcCnd_INIT_e);
        }
        slot=fopAcM_GetParam(this);
        if(slot>=tickets.size()) return cPhs_ERROR_e;
        const auto& ticket=tickets[slot]; serial=ticket.serial; epoch=ticket.epoch;
        if(!ticket.reserved || ticket.remove || epoch!=party.epoch() || !party.active() || !action_ready() || !shared_asset)
            return cPhs_ERROR_e;
        held=shared_asset; companion=slot==companion_slot; combat.creature=ticket.creature;
        combat.random=static_cast<std::uint32_t>(serial*2654435761u)^party.party().random;
        packet.owner=this;
        static const dCcD_SrcSph body_source={
            {{0,{{0,0,0},{0xd8fbfdff,3},0x79}}, {dCcD_SE_NONE,0,0,0,0}, {dCcD_SE_NONE,0,0,0,2}, {0}},
            {{{0,0,0},35}}
        };
        static const dCcD_SrcSph attack_source={
            {{0,{{AT_TYPE_NORMAL_SWORD,2,3},{0,0},0}}, {dCcD_SE_NONE,0,0,0,0}, {dCcD_SE_NONE,0,0,0,0}, {0}},
            {{{0,0,0},40}}
        };
        collision_status.Init(100,0,this);
        body.Set(body_source); body.SetStts(&collision_status); body.SetTgSPrm(companion?5:3);
        body.OffCoSetBit(); // Background walls handle movement; never push Link during a throw.
        attack.Set(attack_source); attack.SetStts(&collision_status); attack.OnAtNoConHit();
        attack.SetAtSPrm(companion?3:13);
        if(!companion) attack.SetAtType(AT_TYPE_CSTATUE_SWING);
        attack.SetAtHitCallback([](fopAc_ac_c* owner,dCcD_GObjInf*,fopAc_ac_c*,dCcD_GObjInf*) {
            if(owner) static_cast<FieldActor*>(owner)->combat.hit_landed=true;
        });
        ground.Set(&current.pos,&old.pos,this,1,&wall,&speed,nullptr,nullptr); wall.SetWall(25,25);
        fopAcM_SetMtx(this,matrix); fopAcM_setCullSizeBox(this,-240,-50,-240,240,220,240);
        attention_info.flags=companion?0:fopAc_AttnFlag_LOCK_e|fopAc_AttnFlag_BATTLE_e;
        attention_info.distances[fopAc_attn_BATTLE_e]=22;
        tickets[slot].id=fopAcM_GetID(this);
        pose_frame();
        message(companion?"Pikachu is following you. It will fight nearby enemies.":"Wild Pikachu appeared! Weaken it before throwing a Poke Ball.",true);
        return cPhs_COMPLEATE_e;
    }
    bool valid() const {
        auto* player=dComIfGp_getPlayer(0);
        return slot<tickets.size() && tickets[slot].serial==serial && !tickets[slot].remove &&
               party.active() && epoch==party.epoch() && player_scene_ready(player) &&
               layer_tag.layer==player->layer_tag.layer && fopAcM_GetRoomNo(this)==fopAcM_GetRoomNo(player);
    }
    bool persist() {
        if(!companion) return true;
        if(party.update_member(combat.creature,epoch)) return true;
        message("Party update failed. Recalling the companion to preserve your saved party.",true);
        tickets[slot].remove=true; return false;
    }
    void pose_frame() {
        const int clip=combat.clip();
        if(animation_clip!=clip || combat.began_move) { animation_clip=clip; seconds=0; }
        const auto index=static_cast<std::size_t>(std::max(0,clip));
        held->model().sample(index<held->model().clips().size()?index:0,seconds,clip==0,pose);
        mDoMtx_stack_c::transS(current.pos.x,current.pos.y,current.pos.z);
        mDoMtx_stack_c::YrotM(shape_angle.y); mDoMtx_stack_c::scaleM(100,100,100);
        MTXCopy(mDoMtx_stack_c::get(),matrix);
        eyePos=current.pos; eyePos.y+=60; attention_info.position=eyePos;
    }
    void walk_to(const cXyz& destination,float step) {
        const float dx=destination.x-current.pos.x,dz=destination.z-current.pos.z;
        const float length=std::sqrt(dx*dx+dz*dz);
        if(length<1) return;
        shape_angle.y=static_cast<s16>(std::atan2(dx,dz)*32768.0/3.14159265358979323846);
        cXyz next=current.pos; next.x+=dx/length*std::min(step,length); next.z+=dz/length*std::min(step,length);
        cXyz from=current.pos,to=next; from.y+=35; to.y+=35;
        if(!ground_at(next,45) || !clear_line(from,to,this)) return;
        const auto previous=current.pos; old.pos=current.pos; current.pos=next;
        ground.CrrPos(dComIfG_Bgsp());
        if(!ground_at(current.pos,55)) current.pos=previous;
    }
    fopAc_ac_c* choose_target();
    void receive_hit() {
        if(!body.ChkTgHit()) return;
        auto* hit=body.GetTgHitObj(); auto* source=body.GetTgHitAc();
        const bool friendly=companion && source==dComIfGp_getPlayer(0);
        if(hit && !friendly && ball_ticks<0 && !closed) {
            int damage=std::clamp(int(hit->GetAtAtp())*4,3,16);
            if(guard_timer && source && fopAcM_GetID(source)==guarded_from) damage=std::max(1,damage/2);
            if(combat.damage(damage)) {
                persist();
                if(!combat.creature.hp) {
                    attention_info.flags=0;
                    message(companion?"Your Pikachu fainted. Clear the encounter, then rest your party.":"Wild Pikachu fainted.",true);
                }
            }
        }
        body.ClrTgHit();
    }
    void status_move(fopAc_ac_c* target) {
        if(!target || move_info(combat.move)->damaging) return;
        if(fopAcM_GetProfName(target)==field_profile) {
            auto* other=static_cast<FieldActor*>(target);
            if(combat.move==Move::Growl) other->combat.attack_down=300;
            else other->combat.defense_down=300;
        } else {
            // Limit the adaptation to this pair; never patch Link's or a vanilla
            // enemy's global stats. These modifiers expire after ten seconds.
            if(combat.move==Move::Growl) { guarded_from=fopAcM_GetID(target); guard_timer=300; }
            else { weakened_target=fopAcM_GetID(target); weaken_timer=300; }
        }
        combat.hit_landed=true;
    }
    bool begin_ball() {
        if(companion || closed || ball_ticks>=0 || !combat.creature.hp || !valid()) return false;
        if(!party.begin_throw(serial)) return false;
        ball_ticks=0; ball_start=dComIfGp_getPlayer(0)->current.pos; ball_start.y+=65;
        ball_end=current.pos; ball_end.y+=12; ball_position=ball_start; attention_info.flags=0;
        attack.OffAtSetBit(); body.OffTgSetBit(); return true;
    }
    void tick_ball() {
        ++ball_ticks;
        if(ball_ticks<20) {
            const float t=float(ball_ticks)/20;
            ball_position=ball_start+(ball_end-ball_start)*t; ball_position.y+=100*t*(1-t);
        } else ball_position=ball_end;
        if(ball_ticks<70) return;
        const auto result=party.finish_throw(serial,epoch,combat.creature);
        ball_ticks=-1;
        if(result==CaptureResult::Caught) {
            closed=true; tickets[slot].remove=true;
            message("Caught Pikachu! Party "+std::to_string(party.party().count)+"/6. Save Zelda to keep your progress.",true);
        } else {
            attention_info.flags=fopAc_AttnFlag_LOCK_e|fopAc_AttnFlag_BATTLE_e;
            combat.phase=Phase::Ready; combat.timer=35; combat.invincible=20;
            message(result==CaptureResult::Escaped?"Pikachu broke free! Lower its HP and try again.":"Capture could not be stored. Pikachu was released safely.",true);
        }
    }
    int execute() {
        if(!valid() || closed) { fopAcM_delete(this); return 1; }
        const bool paused=dComIfGp_isPauseFlag() || !action_ready();
        attack.OffAtSetBit(); body.OffTgSetBit();
        if(paused) { body.ClrTgHit(); attack.ClrAtHit(); return 1; }
        collision_status.Move();
        if(ball_ticks>=0) { body.ClrTgHit(); tick_ball(); return 1; }
        receive_hit();
        if(tickets[slot].remove) return 1;
        if(guard_timer>0) --guard_timer;
        if(weaken_timer>0) --weaken_timer;
        fopAc_ac_c* target=choose_target();
        const float range=target?distance(current.pos,target->current.pos):99999.f;
        cXyz start=current.pos; start.y+=40;
        cXyz end=target?target->current.pos:current.pos; end.y+=40;
        const bool visible=target && clear_line(start,end,this);
        if(target && visible) shape_angle.y=static_cast<s16>(std::atan2(end.x-start.x,end.z-start.z)*32768.0/3.14159265358979323846);
        const auto before=combat.creature;
        combat.tick(visible && range<200);
        if(combat.creature!=before && !persist()) return 1;
        if(combat.began_move) message(std::string(companion?"Your Pikachu used ":"Wild Pikachu used ")+move_info(combat.move)->name+"!",true);
        if(combat.began_strike && visible && range<220) status_move(target);
        if(combat.phase==Phase::Fainted) {
            if(combat.timer>=60) tickets[slot].remove=true;
        } else if(combat.phase==Phase::Ready) {
            if(target && range>105) walk_to(target->current.pos,companion?4.f:2.8f);
            else if(!target && companion) {
                auto* player=dComIfGp_getPlayer(0);
                if(distance(current.pos,player->current.pos)>140) walk_to(player->current.pos,5.f);
            }
        } else if(combat.phase==Phase::Strike && combat.move==Move::QuickAttack && target && visible && range>65)
            walk_to(target->current.pos,12.f);
        if(combat.creature.hp && !combat.invincible) {
            body.OnTgSetBit(); body.SetC(start); body.SetR(35); dComIfG_Ccsp()->Set(&body);
        }
        if(combat.attacking() && visible && range<220) {
            cXyz center=current.pos; center.y+=40;
            const float dx=end.x-center.x,dz=end.z-center.z;
            const float length=std::sqrt(dx*dx+dz*dz);
            const bool electric=combat.move==Move::ThunderShock;
            if(length>1) { const float reach=std::min(length,electric?100.f:55.f); center.x+=dx/length*reach; center.z+=dz/length*reach; }
            attack.SetAtAtp(combat.attack_power()+((weaken_timer && fopAcM_GetID(target)==weakened_target)?1:0));
            attack.SetAtMtrl(electric?dCcD_MTRL_ELECTRIC:dCcD_MTRL_NONE);
            attack.SetC(center); attack.SetR(electric?85.f:45.f); attack.OnAtSetBit(); dComIfG_Ccsp()->Set(&attack);
            spark_end=end;
        }
        seconds+=1.0/30.0; pose_frame(); return 1;
    }
    int draw_actor() {
        if(held && valid() && !closed) dComIfGd_getOpaList()->entryImm(&packet,0);
        return 1;
    }
    int destroy() {
        // Base creation can fail before placement-new constructed our fields.
        // Its engine-populated parameters and ID still identify the reservation.
        const auto reserved_slot=static_cast<std::size_t>(fopAcM_GetParam(this));
        if(reserved_slot<tickets.size() && tickets[reserved_slot].id==fopAcM_GetID(this)) tickets[reserved_slot]=Ticket{};
        if(fopAcM_CheckCondition(this,fopAcCnd_INIT_e)) {
            if(ball_ticks>=0 && epoch==party.epoch()) party.cancel_throw();
            this->~FieldActor();
        }
        return 1;
    }
};
FieldActor* actor_for(std::size_t slot) {
    if(slot>=tickets.size() || !tickets[slot].reserved || tickets[slot].remove || tickets[slot].id==no_actor) return nullptr;
    auto* base=fopAcM_SearchByID(tickets[slot].id);
    if(!base || fopAcM_GetProfName(base)!=field_profile || !fopAcM_CheckCondition(base,fopAcCnd_INIT_e)) return nullptr;
    auto* actor=static_cast<FieldActor*>(base);
    return actor->serial==tickets[slot].serial && actor->valid()?actor:nullptr;
}
bool ordinary_enemy(const fopAc_ac_c* actor) {
    switch(fopAcM_GetProfName(actor)) {
    case fpcNm_E_S1_e: case fpcNm_E_RD_e: case fpcNm_E_BU_e: case fpcNm_E_BA_e:
    case fpcNm_E_DB_e: case fpcNm_E_HB_e: case fpcNm_E_SM_e: case fpcNm_E_ST_e: return true;
    default:return false;
    }
}
fopAc_ac_c* FieldActor::choose_target() {
    if(!combat.creature.hp) return nullptr;
    auto* player=dComIfGp_getPlayer(0);
    fopAc_ac_c* best=nullptr; float nearest=companion?650.f:750.f;
    if(companion) {
        for(std::size_t i=0;i<companion_slot;++i) if(auto* wild=actor_for(i)) {
            const float length=distance(current.pos,wild->current.pos);
            if(wild->combat.creature.hp && wild->ball_ticks<0 && length<nearest) { nearest=length; best=wild; }
        }
        struct Search { FieldActor* self; fopAc_ac_c* best; float distance; } search{this,best,nearest};
        fopAcM_Search([](void* actor,void* user)->void* {
            auto& search=*static_cast<Search*>(user); auto* candidate=static_cast<fopAc_ac_c*>(actor);
            if(!fopAcM_IsActor(candidate) || !ordinary_enemy(candidate) || candidate->health<=0 ||
               fopAcM_IsExecuting(fopAcM_GetID(candidate))==0 || candidate->layer_tag.layer!=search.self->layer_tag.layer ||
               fopAcM_GetRoomNo(candidate)!=fopAcM_GetRoomNo(search.self)) return nullptr;
            const float length=distance(search.self->current.pos,candidate->current.pos);
            if(length<search.distance) { search.distance=length; search.best=candidate; }
            return nullptr;
        },&search);
        best=search.best;
    } else {
        if(auto* buddy=actor_for(companion_slot); buddy && buddy->combat.creature.hp && distance(current.pos,buddy->current.pos)<350) best=buddy;
        else if(distance(current.pos,player->current.pos)<nearest) best=player;
    }
    target_id=best?fopAcM_GetID(best):no_actor;
    return best;
}
void FieldPacket::draw() {
    if(!owner || !owner->held) return;
    j3dSys.reinitGX(); J3DShape::resetVcdVatCache();
    const auto& view=j3dSys.getViewMtx();
    if(owner->ball_ticks<20) {
        Mtx view_model; MTXConcat(view,owner->matrix,view_model);
        owner->held->draw(owner->pose,view_model);
        if(owner->combat.creature.hp) {
            cXyz top=owner->current.pos; top.y+=115;
            draw_health(top,float(owner->combat.creature.hp)/owner->combat.creature.max_hp,owner->companion,view);
        }
    }
    if(owner->ball_ticks>=0) draw_ball(owner->ball_position,owner->ball_ticks>=20?0.4f*std::sin(float(owner->ball_ticks)):0.f,view);
    if(owner->combat.phase==Phase::Strike && owner->combat.move==Move::ThunderShock && owner->attack.ChkAtSet()) {
        cXyz from=owner->current.pos; from.y+=55; draw_spark(from,owner->spark_end,view);
    }
    j3dSys.reinitGX(); J3DShape::resetVcdVatCache();
}
const ActorProfileDesc descriptor={
    "pkfield",7,sizeof(FieldActor),fpcDwPi_OBJ_LBOX_e,fopAcStts_UNK_0x40000_e|fopAcStts_UNK_0x4000_e,
    fopAc_ACTOR_e,fopAc_CULLBOX_CUSTOM_e,
    [](void* p){return static_cast<FieldActor*>(p)->create();},
    [](void* p){return static_cast<FieldActor*>(p)->destroy();},
    [](void* p){return static_cast<FieldActor*>(p)->execute();},
    [](void*){return 1;},[](void* p){return static_cast<FieldActor*>(p)->draw_actor();}
};
bool spawn_slot(std::size_t slot) {
    if(!party.active() || !shared_asset || !action_ready() || tickets[slot].reserved) return false;
    auto* player=dComIfGp_getPlayer(0);
    const float angle=float(player->shape_angle.y)*3.14159265358979323846f/32768.f+(float(slot)-1)*0.5f;
    cXyz at=player->current.pos;
    const bool companion=slot==companion_slot;
    at.x+=std::sin(angle)*(companion?-100.f:240.f); at.z+=std::cos(angle)*(companion?-100.f:240.f);
    cXyz from=player->current.pos,to=at; from.y+=40; to.y+=40;
    if(!ground_at(at) || !clear_line(from,to,player)) { message("Move onto open ground before spawning.",true); return false; }
    auto& ticket=tickets[slot]; ticket=Ticket{};
    if(companion) {
        const auto& p=party.party();
        if(!p.count || !p.members[p.selected].hp) { message("Choose a healthy Pokemon or rest your party.",true); return false; }
        ticket.creature=p.members[p.selected];
    }
    ticket.reserved=true; ticket.serial=next_serial++; ticket.epoch=party.epoch();
    ActorSpawnParams params{}; params.parameters=static_cast<std::uint32_t>(slot);
    params.room_num=static_cast<std::int8_t>(fopAcM_GetRoomNo(player)); params.position={at.x,at.y,at.z}; params.scale={1,1,1};
    params.angle.y=static_cast<std::int16_t>(std::uint16_t(player->shape_angle.y)+0x8000u);
    if(create_in_player_scene(svc_actor,mod_ctx,player,field_profile,params,ticket.id)!=MOD_OK) {
        ticket=Ticket{}; message("Pokemon could not spawn.",true); return false;
    }
    return true;
}
void spawn_wild(ModContext*,void*) {
    for(std::size_t i=0;i<companion_slot;++i) if(!tickets[i].reserved) { spawn_slot(i); return; }
    message("Three wild Pokemon are already present.",true);
}
void throw_ball(ModContext*,void*) {
    if(!action_ready() || !party.active() || party.pending()) return;
    if(party.party().count==party_capacity) { message("Your party is full (6/6).",true); return; }
    if(!party.party().balls) { message("No Poke Balls left. Restock from the encounter panel.",true); return; }
    auto* player=dComIfGp_getPlayer(0); FieldActor* target=nullptr; float nearest=600;
    for(std::size_t i=0;i<companion_slot;++i) if(auto* wild=actor_for(i)) {
        const auto delta=wild->current.pos-player->current.pos;
        const float angle=float(player->shape_angle.y)*3.14159265358979323846f/32768.f;
        const float ahead=delta.x*std::sin(angle)+delta.z*std::cos(angle);
        const float length=distance(wild->current.pos,player->current.pos);
        cXyz from=player->current.pos,to=wild->current.pos; from.y+=40; to.y+=40;
        if(wild->combat.creature.hp && ahead>0 && length<nearest && clear_line(from,to,player)) { target=wild; nearest=length; }
    }
    if(!target) { message("Face a living wild Pokemon within throwing range.",true); return; }
    if(!target->begin_ball()) message("Throw could not be recorded in the active save.",true);
}
void summon(ModContext*,void*) {
    if(tickets[companion_slot].reserved) { tickets[companion_slot].remove=true; message("Recalling your Pokemon.",true); }
    else spawn_slot(companion_slot);
}
void next_member(ModContext*,void*) {
    if(party.select_next(1)) message("Selected Pikachu #"+std::to_string(party.party().members[party.party().selected].id)+".",true);
}
void clear_encounter(ModContext*,void*) { remove_all(); message("Clearing this encounter. Captured Pokemon remain in your party."); }
void rest(ModContext*,void*) {
    if(has_actors()) { message("Clear the encounter and recall your companion before resting."); return; }
    message(party.rest()?"Party HP and move PP restored. Save Zelda to keep this progress.":"Party could not be updated.");
}
void restock(ModContext*,void*) {
    message(party.restock()?"Practice supplies restored to 24 Poke Balls.":"Poke Balls could not be updated.");
}
HookAction input(ModContext*,void*,void*,void*) {
    if(!initialized || !party.active() || !action_ready() || dComIfGp_isPauseFlag() || !mDoCPd_c::getHoldR(PAD_1)) return HOOK_CONTINUE;
    const auto press=mDoCPd_c::getTrig(PAD_1);
    if(press&PAD_BUTTON_UP) throw_ball(nullptr,nullptr);
    else if(press&PAD_BUTTON_DOWN) summon(nullptr,nullptr);
    else if(press&PAD_BUTTON_RIGHT) next_member(nullptr,nullptr);
    else if(press&PAD_BUTTON_LEFT) { if(party.select_next(-1)) message("Selected previous party member.",true); }
    else return HOOK_CONTINUE;
    auto& pad=mDoCPd_c::getCpadInfo(PAD_1);
    pad.mPressedButtonFlags&=~(PAD_BUTTON_UP|PAD_BUTTON_DOWN|PAD_BUTTON_LEFT|PAD_BUTTON_RIGHT);
    return HOOK_SKIP_ORIGINAL;
}
bool no_world(ModContext*,void*) { return !party.active() || !action_ready() || !shared_asset; }
bool no_party(ModContext*,void*) { return no_world(nullptr,nullptr) || !party.party().count; }
ModResult button(UiElementHandle pane,const char* label,UiPressedFn fn,UiPredicateFn disabled=no_world) {
    UiControlDesc desc=UI_CONTROL_DESC_INIT; desc.label=label; desc.on_pressed=fn; desc.is_disabled=disabled;
    return svc_ui->pane_add_control(mod_ctx,pane,&desc,nullptr);
}
std::string party_text() {
    if(!party.active()) return read_failed?"Party unavailable: original save data preserved.":"Load a Zelda save to use the party.";
    const auto& p=party.party();
    std::string text="Poke Balls: "+std::to_string(p.balls)+" | Party: "+std::to_string(p.count)+"/6\n";
    for(std::size_t i=0;i<p.count;++i) {
        const auto& c=p.members[i];
        text+=(i==p.selected?"> ":"  ")+std::to_string(i+1)+". Pikachu #"+std::to_string(c.id)+" Lv."+std::to_string(c.level)+
              " | HP "+std::to_string(c.hp)+"/"+std::to_string(c.max_hp)+" | PP ";
        for(std::size_t j=0;j<move_capacity;++j) text+=std::to_string(c.pp[j])+(j+1==move_capacity?"\n":"/");
    }
    return text;
}
}
bool has_actors() { for(const auto& ticket:tickets) if(ticket.reserved) return true; return false; }
void set_asset(std::shared_ptr<GxAsset> asset) { shared_asset=std::move(asset); }
void update() {
    if(!initialized) return;
    if(!scene_ready()) { attempted_player=no_actor; return; }
    auto* player=dComIfGp_getPlayer(0);
    if(!party.active() && !read_failed && action_ready() && attempted_player!=fopAcM_GetID(player)) {
        attempted_player=fopAcM_GetID(player); load_party();
    }
}
ModResult initialize(ModError* error) {
    if(!svc_save || !svc_hook) return mods::set_error(error,MOD_UNAVAILABLE,"Save and hook services required for Pokemon encounters");
    auto r=svc_actor->register_actor(mod_ctx,&descriptor,&field_profile,&registration);
    if(r!=MOD_OK) return r;
    r=svc_save->observe_saves(mod_ctx,on_new_or_loaded,on_new_or_loaded,on_written,nullptr,&save_observer);
    if(r!=MOD_OK) { svc_actor->unregister_actor(mod_ctx,registration); registration=0; return r; }
    r=mods::hook::add_pre<PokemonFieldInput>(svc_hook,input);
    if(r!=MOD_OK) { shutdown(); return mods::set_error(error,r,"Could not install Pokemon controller shortcuts"); }
    initialized=true; return MOD_OK;
}
ModResult shutdown() {
    initialized=false;
    // Keep actor code and shared assets resident until the host finishes deleting
    // every actor. Never reset a party before its live instances have stopped.
    if(registration) { const auto r=svc_actor->unregister_actor(mod_ctx,registration); if(r!=MOD_OK) return r; }
    registration=0; field_profile=-1;
    if(save_observer) svc_save->unobserve_saves(mod_ctx,save_observer);
    save_observer=0; party.deactivate(); tickets={}; shared_asset.reset(); status_element=party_element=0;
    read_failed=false; save_slot=-1; attempted_player=no_actor; return MOD_OK;
}
ModResult build_panel(UiElementHandle pane) {
    update(); status_element=party_element=0; dirty=true; last_party_text=party_text();
    auto r=svc_ui->pane_add_section(mod_ctx,pane,"Pokemon Encounters"); if(r!=MOD_OK) return r;
    r=svc_ui->pane_add_text(mod_ctx,pane,"Fight wild Pikachu with Link's weapons. Lower HP improves capture odds. Hold R + D-pad Up to throw; Down to summon/recall; Left/Right to select your party member.",nullptr); if(r!=MOD_OK) return r;
    r=svc_ui->pane_add_text(mod_ctx,pane,"Save through Zelda's normal save menu to keep your party and Poke Balls. Each Zelda save has its own party.",nullptr); if(r!=MOD_OK) return r;
    r=svc_ui->pane_add_text(mod_ctx,pane,status.c_str(),&status_element); if(r!=MOD_OK) return r;
    r=svc_ui->pane_add_text(mod_ctx,pane,last_party_text.c_str(),&party_element); if(r!=MOD_OK) return r;
    r=button(pane,"Start wild encounter",spawn_wild); if(r!=MOD_OK) return r;
    r=button(pane,"Throw Poke Ball",throw_ball); if(r!=MOD_OK) return r;
    r=button(pane,"Summon / recall selected Pokemon",summon,no_party); if(r!=MOD_OK) return r;
    r=button(pane,"Select next party member",next_member,no_party); if(r!=MOD_OK) return r;
    r=button(pane,"Clear encounter and recall",clear_encounter); if(r!=MOD_OK) return r;
    r=button(pane,"Rest party (after clearing encounter)",rest,no_party); if(r!=MOD_OK) return r;
    return button(pane,"Restock practice Poke Balls",restock);
}
ModResult update_panel() {
    if(status_element && dirty) { const auto r=svc_ui->elem_set_text(mod_ctx,status_element,status.c_str()); if(r!=MOD_OK) return r; dirty=false; }
    const auto text=party_text();
    if(party_element && text!=last_party_text) { const auto r=svc_ui->elem_set_text(mod_ctx,party_element,text.c_str()); if(r!=MOD_OK) return r; last_party_text=text; }
    return MOD_OK;
}
} // namespace pokemon_battle::field
