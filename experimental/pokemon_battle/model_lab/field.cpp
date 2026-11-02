#include "field.hpp"
#include "field_effects.hpp"
#include "spawn_context.hpp"
#include "party_save.hpp"
#include "content.hpp"
#include "mods/svc/config.h"
#include "../field/roster.hpp"
#include "../field/combat.hpp"
#include "../field/animation.hpp"
#include "../field/ball.hpp"
#include "d/actor/d_a_alink.h"
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
    std::shared_ptr<GxAsset> asset;
    bool reserved=false,remove=false;
};
std::array<Ticket,4> tickets{};
std::uint64_t next_serial=1;
ProfileName field_profile=-1;
ActorHandle registration=0;
SaveObserverHandle save_observer=0;
std::size_t encounter_species=9;
UiElementHandle status_element=0,party_element=0;
std::string status="Load a Zelda save to begin.",last_party_text;
bool dirty=true,read_failed=false,initialized=false;
int save_slot=-1;
ActorId attempted_player=no_actor,encounter_player=no_actor;
ConfigVarHandle encounters_config=0;
bool natural_encounters=true;
cXyz encounter_position{};
float encounter_distance=0;
std::uint32_t encounter_random=0x4d453230u;

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
    party.activate(loaded);encounter_player=no_actor;encounter_distance=0; message("Party loaded. Select a Pokemon to summon, or start a wild encounter.");
    if(result==LoadResult::Created) message("Choose Bulbasaur, Charmander or Squirtle in the Pokemon Mode panel. 24 Poke Balls are ready.",true);
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
// Only the animation started by this module is ever reset. Native item,
// cutscene and damage animations retain control as soon as they replace it.
struct LinkThrow {
    ActorId player_id=no_actor;
    std::uint64_t owner=0;
    int ticks=0;
    static bool allowed() {
        if(!action_ready())return false;
        auto* link=static_cast<daAlink_c*>(dComIfGp_getPlayer(0));
        return !link->checkWolf() && !link->checkRideOn() && link->checkNoEquipItem() &&
            link->getGrabActorID()==no_actor && link->mProcID>=daAlink_c::PROC_WAIT && link->mProcID<=daAlink_c::PROC_MOVE_TURN &&
            (link->checkNoSetUpperAnime() || link->checkSmallUpperGuardAnime());
    }
    bool begin(std::uint64_t token) {
        if(owner || !allowed())return false;
        auto* link=static_cast<daAlink_c*>(dComIfGp_getPlayer(0));
        link->setUpperAnime(dRes_ID_ALANM_BCK_BOMBTHROW_e,daAlink_c::UPPER_2,.6f,0,-1,3);
        player_id=fopAcM_GetID(link);owner=token;ticks=0;return true;
    }
    bool owns(std::uint64_t token) const {
        auto* player=dComIfGp_getPlayer(0);
        return owner==token && player_scene_ready(player) && fopAcM_GetID(player)==player_id &&
            static_cast<daAlink_c*>(player)->checkUpperAnime(dRes_ID_ALANM_BCK_BOMBTHROW_e);
    }
    void end(std::uint64_t token) {
        if(owner!=token)return;
        if(owns(token) && action_ready())static_cast<daAlink_c*>(dComIfGp_getPlayer(0))->resetUpperAnime(daAlink_c::UPPER_2,3);
        owner=0;player_id=no_actor;ticks=0;
    }
    void tick(std::uint64_t token) {
        if(owner!=token)return;
        if(!owns(token)) {owner=0;player_id=no_actor;return;}
        auto* link=static_cast<daAlink_c*>(dComIfGp_getPlayer(0));
        if(++ticks>75 || link->mUpperFrameCtrl[daAlink_c::UPPER_2].checkAnmEnd())end(token);
    }
    bool released(std::uint64_t token) const {
        return owns(token) && static_cast<daAlink_c*>(dComIfGp_getPlayer(0))->mUpperFrameCtrl[daAlink_c::UPPER_2].getFrame()>=4;
    }
} link_throw;
effects::Point point(const cXyz& value) {return {value.x,value.y,value.z};}
cXyz xyz(const effects::Point& value) {return {value[0],value[1],value[2]};}
class FieldActor;
class FieldPacket final:public J3DPacket {
public:
    FieldActor* owner=nullptr;
    bool particles=false;
    void draw() override;
};
FieldActor* actor_for(std::size_t slot);
class FieldActor:public fopAc_ac_c {
public:
    std::shared_ptr<GxAsset> held;
    FieldPacket packet,particle_packet;
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
    int guard_timer=0,weaken_timer=0,held_ticks=0,age=0;
    BallSequence ball;
    bool capture_caught=false,throw_started=false;
    float move_seconds=-1;
    Animation animation;
    float move_speed=0;
    cXyz ball_start{},ball_end{},ball_position{},spark_end{};
    bool companion=false,closed=false,following=false;
    const Species& species() const { return *species_info(combat.creature.species); }
    std::string name() const { return species().name; }
    float center_height() const { return species().height*(combat.creature.species==92 || combat.creature.species==93?.85f:.5f); }

    bool transferring() const {return ball.active();}
    cXyz center() const {auto p=current.pos;p.y+=center_height();return p;}

    int create() {
        if(!fopAcM_CheckCondition(this,fopAcCnd_INIT_e)) {
            ::new(static_cast<void*>(this)) FieldActor;
            fopAcM_OnCondition(this,fopAcCnd_INIT_e);
        }
        slot=fopAcM_GetParam(this);
        if(slot>=tickets.size()) return cPhs_ERROR_e;
        const auto& ticket=tickets[slot]; serial=ticket.serial; epoch=ticket.epoch;
        if(!ticket.reserved || ticket.remove || epoch!=party.epoch() || !party.active() || !action_ready() || !ticket.asset)
            return cPhs_ERROR_e;
        held=ticket.asset; companion=slot==companion_slot; combat.creature=ticket.creature;
        combat.random=static_cast<std::uint32_t>(serial*2654435761u)^party.party().random;
        packet.owner=particle_packet.owner=this;particle_packet.particles=true;
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
        if(companion) {
            ball.begin(BallKind::Summon);ball_start=static_cast<daPy_py_c*>(dComIfGp_getPlayer(0))->getRightHandPos();
            ball_end=current.pos;ball_end.y+=12;ball_position=ball_start;
            attention_info.flags=0;
        }
        pose_frame();
        message(companion?name()+" is following you.":"Wild "+name()+" appeared!",true);
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
    void pose_frame(float traveled=0,bool advance=false) {
        animation.sample(held->model(),combat,traveled,pose,advance);
        mDoMtx_stack_c::transS(current.pos.x,current.pos.y+species().origin_y*species().scale,current.pos.z);
        mDoMtx_stack_c::YrotM(shape_angle.y); mDoMtx_stack_c::scaleM(species().scale,species().scale,species().scale);
        MTXCopy(mDoMtx_stack_c::get(),matrix);
        eyePos=current.pos; eyePos.y+=center_height(); attention_info.position=eyePos;
    }
    void face(const cXyz& destination) {
        const auto angle=static_cast<s16>(std::atan2(destination.x-current.pos.x,destination.z-current.pos.z)*32768.0/3.14159265358979323846);
        shape_angle.y=turn_toward(shape_angle.y,angle);
    }
    void walk_to(const cXyz& destination,float step,float stop_distance) {
        const float dx=destination.x-current.pos.x,dz=destination.z-current.pos.z;
        const float length=std::sqrt(dx*dx+dz*dz);
        if(length<=stop_distance+0.1f) { move_speed=0; return; }
        const auto angle=static_cast<s16>(std::atan2(dx,dz)*32768.0/3.14159265358979323846);
        shape_angle.y=turn_toward(shape_angle.y,angle);
        const float alignment=forward_alignment(shape_angle.y,angle);
        if(alignment<0.3f) { move_speed=0; return; }
        if(combat.paralysis)step*=.5f;
        const float wanted=std::min(step,(length-stop_distance)*0.35f)*alignment;
        const float acceleration=step>6.f?2.5f:0.8f;
        move_speed+=std::clamp(wanted-move_speed,-acceleration,acceleration);
        const float travel=std::min(move_speed,length-stop_distance);
        const double heading=double(shape_angle.y)*3.14159265358979323846/32768.0;
        cXyz next=current.pos; next.x+=float(std::sin(heading))*travel; next.z+=float(std::cos(heading))*travel;
        cXyz from=current.pos,to=next; from.y+=35; to.y+=35;
        if(!ground_at(next,45) || !clear_line(from,to,this)) { move_speed=0; return; }
        const auto previous=current.pos; old.pos=current.pos; current.pos=next;
        ground.CrrPos(dComIfG_Bgsp());
        if(!ground_at(current.pos,55)) { current.pos=previous; move_speed=0; }
    }
    fopAc_ac_c* choose_target();
    void receive_hit() {
        if(!body.ChkTgHit()) return;
        auto* hit=body.GetTgHitObj(); auto* source=body.GetTgHitAc();
        const bool friendly=companion && source==dComIfGp_getPlayer(0);
        if(hit && !friendly && !transferring() && !closed) {
            int damage=std::clamp(int(hit->GetAtAtp())*4,3,16);
            if(source && fopAcM_GetProfName(source)==field_profile)
                damage=static_cast<FieldActor*>(source)->combat.damage_against(combat.creature);
            if(guard_timer && source && fopAcM_GetID(source)==guarded_from) damage=std::max(1,damage/2);
            if(combat.damage(damage)) {
                persist();
                if(!combat.creature.hp) {
                    attention_info.flags=0;
                    message((companion?"Your ":"Wild ")+name()+" fainted.",true);
                }
            }
        }
        body.ClrTgHit();
    }
    void status_move(fopAc_ac_c* target) {
        if(!target || move_info(combat.move)->damaging) return;
        if(fopAcM_GetProfName(target)==field_profile) {
            auto* other=static_cast<FieldActor*>(target);
            other->combat.apply_status(move_info(combat.move)->status);
        } else {
            // Limit the adaptation to this pair; never patch Link's or a vanilla
            // enemy's global stats. These modifiers expire after ten seconds.
            if(combat.move!=Move::TailWhip) { guarded_from=fopAcM_GetID(target); guard_timer=300; }
            else { weakened_target=fopAcM_GetID(target); weaken_timer=300; }
        }
        combat.hit_landed=true;
    }
    bool begin_ball() {
        if(companion || closed || transferring() || !combat.creature.hp || !valid() || !LinkThrow::allowed() || link_throw.owner)return false;
        if(!party.begin_throw(serial)) return false;
        ball.begin(BallKind::Capture);held_ticks=0;throw_started=false;capture_caught=false;
        combat.phase=Phase::Ready;combat.timer=45;
        ball_start=static_cast<daPy_py_c*>(dComIfGp_getPlayer(0))->getRightHandPos();ball_end=center();ball_position=ball_start;
        attention_info.flags=0;move_speed=0;move_seconds=-1;
        attack.OffAtSetBit();body.OffTgSetBit();return true;
    }
    void begin_recall() {
        if(!companion || transferring())return;
        ball.begin(BallKind::Recall);ball_position=static_cast<daPy_py_c*>(dComIfGp_getPlayer(0))->getRightHandPos();
        ball_end=ball_position;attention_info.flags=0;move_seconds=-1;move_speed=0;
    }
    void tick_ball() {
        if(ball.stage()==BallStage::Held) {
            ball_position=static_cast<daPy_py_c*>(dComIfGp_getPlayer(0))->getRightHandPos();
            if(!throw_started) {throw_started=link_throw.begin(serial);}
            if(throw_started && link_throw.released(serial)) {
                ball_start=ball_position;ball.release();
            } else if(++held_ticks>60 || (throw_started && !link_throw.owns(serial))) {
                party.cancel_throw(serial);link_throw.end(serial);ball.cancel();
                if(companion)tickets[slot].remove=true;
                else attention_info.flags=fopAc_AttnFlag_LOCK_e|fopAc_AttnFlag_BATTLE_e;
                message("Throw interrupted. Sheathe your weapon and stand on open ground to throw.",true);return;
            }
        }
        if(ball.tick()) {
            const auto result=party.finish_throw(serial,epoch,combat.creature);
            capture_caught=result==CaptureResult::Caught;ball.resolve(capture_caught);
            message(capture_caught?"Caught "+name()+"! Party "+std::to_string(party.party().count)+"/6. Save Zelda to keep your progress.":
                result==CaptureResult::Escaped?name()+" broke free!":"Capture could not be stored. "+name()+" was released safely.",true);
        }
        const auto f=ball.frame();
        if(ball.kind()==BallKind::Recall)ball_position=static_cast<daPy_py_c*>(dComIfGp_getPlayer(0))->getRightHandPos();
        else if(ball.stage()==BallStage::Flight) {
            ball_position=ball_start+(ball_end-ball_start)*f.progress;ball_position.y+=100*f.progress*(1-f.progress);
        } else if(ball.stage()==BallStage::Drop) {
            ball_position=ball_end;const float ground=current.pos.y+8;
            const float t=f.progress;
            if(t<.45f)ball_position.y=ball_end.y+(ground-ball_end.y)*(t/.45f)*(t/.45f);
            else ball_position.y=ground+18*std::abs(std::sin((t-.45f)/.55f*6.2831853f))*(1-t)/.55f;
        } else if(ball.stage()==BallStage::Shake || ball.stage()==BallStage::Resolve || ball.stage()==BallStage::Success ||
                  ball.stage()==BallStage::EscapeOpen || ball.stage()==BallStage::Escape || ball.stage()==BallStage::EscapeClose) {
            ball_position=ball_end;ball_position.y=current.pos.y+8;
        } else if(ball.stage()==BallStage::Return) {
            const auto hand=static_cast<daPy_py_c*>(dComIfGp_getPlayer(0))->getRightHandPos();ball_position=ball_end+(hand-ball_end)*f.progress;
            ball_position.y+=60*f.progress*(1-f.progress);
        } else if(ball.stage()!=BallStage::Held)ball_position=ball_end;
        if(ball.stage()==BallStage::Done) {
            const auto kind=ball.kind();ball.cancel();
            if(capture_caught || kind==BallKind::Recall) {closed=true;tickets[slot].remove=true;}
            else {
                combat.phase=Phase::Ready;combat.timer=35;combat.invincible=20;
                attention_info.flags=companion?0:fopAc_AttnFlag_LOCK_e|fopAc_AttnFlag_BATTLE_e;
            }
        }
        pose_frame(0,true);
    }
    effects::Origins origins() const {
        effects::Origins out;out.size=species().height;out.body=point(center());
        out.mouth=out.left=out.right=out.tail=out.body;
        for(std::size_t j=0;j<held->model().joints().size() && j<pose.joints.size();++j) {
            const auto& name=held->model().joints()[j].name;Vec from{pose.joints[j][0],pose.joints[j][1],pose.joints[j][2]},to{};
            MTXMultVec(matrix,&from,&to);const effects::Point at{to.x,to.y,to.z};
            if(name.ends_with("_Mouth"))out.mouth=at;
            if(name.ends_with("_EyeLeft"))out.left=at;
            if(name.ends_with("_EyeRight"))out.right=at;
            if(name.ends_with("_Tail"))out.tail=at;
        }
        // Electric charge belongs just below Pikachu/Raichu's animated eyes.
        out.left[1]-=species().height*.06f;out.right[1]-=species().height*.06f;
        return out;
    }
    int execute() {
        if(!valid() || closed) { fopAcM_delete(this); return 1; }
        const bool paused=dComIfGp_isPauseFlag() || !action_ready();
        attack.OffAtSetBit(); body.OffTgSetBit();
        if(paused) { body.ClrTgHit(); attack.ClrAtHit(); return 1; }
        ++age;link_throw.tick(serial);
        collision_status.Move();
        if(transferring()) { body.ClrTgHit(); tick_ball(); return 1; }
        receive_hit();
        if(tickets[slot].remove) return 1;
        if(guard_timer>0) --guard_timer;
        if(weaken_timer>0) --weaken_timer;
        fopAc_ac_c* target=choose_target();
        const float range=target?distance(current.pos,target->current.pos):99999.f;
        cXyz start=current.pos; start.y+=40;
        cXyz end=target?target->current.pos:current.pos; end.y+=40;
        const bool visible=target && clear_line(start,end,this);
        const cXyz previous_position=current.pos;
        const auto before=combat.creature;
        combat.tick(visible && range<200);
        if(combat.creature!=before && !persist()) return 1;
        if(combat.began_move) {move_seconds=0;spark_end=end;}
        else if(combat.phase==Phase::Hurt || combat.phase==Phase::Fainted)move_seconds=-1;
        else if(move_seconds>=0) {move_seconds+=1.f/30;if(move_seconds>2.5f)move_seconds=-1;}
        if(combat.began_move) message(std::string(companion?"Your ":"Wild ")+name()+" used "+move_info(combat.move)->name+"!",true);
        if(combat.began_strike && !combat.accuracy_roll())combat.hit_landed=true;
        if(combat.began_strike && !combat.hit_landed && visible && range<220) status_move(target);
        if(combat.phase==Phase::Fainted) {
            move_speed=0;
            const auto& clip=held->model().clips()[species().clips[0]];
            if(combat.timer>=std::max(60,static_cast<int>(clip.frames/clip.fps*30)+15)) tickets[slot].remove=true;
        } else if(combat.sleep>0) {
            move_speed=0;
        } else if(combat.phase==Phase::Ready) {
            if(target && range>105) walk_to(target->current.pos,companion?4.f:2.8f,100.f);
            else if(!target && companion) {
                auto* player=dComIfGp_getPlayer(0);
                const float separation=distance(current.pos,player->current.pos);
                if(separation>140) following=true;
                if(separation<111) following=false;
                if(following) walk_to(player->current.pos,5.f,110.f);
                else move_speed=0;
            } else { move_speed=0; if(target && visible) face(target->current.pos); }
        } else if(combat.phase==Phase::Strike && combat.move==Move::QuickAttack && target && visible && range>65) {
            walk_to(target->current.pos,12.f,60.f);
        } else {
            move_speed=0;
            if(target && visible && combat.phase==Phase::Windup) face(target->current.pos);
        }
        if(combat.creature.hp && !combat.invincible) {
            cXyz center=current.pos; center.y+=center_height();
            body.OnTgSetBit(); body.SetC(center); body.SetR(std::clamp(species().height*.33f,14.f,42.f)); dComIfG_Ccsp()->Set(&body);
        }
        if(combat.attacking() && visible && range<220) {
            cXyz center=current.pos; center.y+=40;
            const float dx=end.x-center.x,dz=end.z-center.z;
            const float length=std::sqrt(dx*dx+dz*dz);
            const auto* move=move_info(combat.move);
            const bool electric=move->type==13;
            const bool ranged=move->type>=10 || move->visual==Visual::Shadow || move->visual==Visual::Leaf || move->visual==Visual::Needles;
            if(length>1) { const float reach=std::min(length,ranged?200.f:55.f); center.x+=dx/length*reach; center.z+=dz/length*reach; }
            // Projectiles arrive five simulation ticks after their source clip
            // releases them. Electric/contact moves connect immediately.
            if(ranged && !electric && combat.timer>0) {pose_frame(0,true);return 1;}
            attack.SetAtAtp(combat.attack_power()+((weaken_timer && fopAcM_GetID(target)==weakened_target)?1:0));
            attack.SetAtMtrl(static_cast<u8>(electric?dCcD_MTRL_ELECTRIC:dCcD_MTRL_NONE));
            attack.SetC(center); attack.SetR(ranged?35.f:45.f); attack.OnAtSetBit(); dComIfG_Ccsp()->Set(&attack);
            spark_end=end;
        }
        const float dx=current.pos.x-previous_position.x,dz=current.pos.z-previous_position.z;
        pose_frame(std::sqrt(dx*dx+dz*dz),true); return 1;
    }
    int draw_actor() {
        if(held && valid() && !closed) {dComIfGd_getOpaList()->entryImm(&packet,0);dComIfGd_entryZSortXluList(&particle_packet,current.pos);}
        return 1;
    }
    int destroy() {
        // Base creation can fail before placement-new constructed our fields.
        // Its engine-populated parameters and ID still identify the reservation.
        const auto reserved_slot=static_cast<std::size_t>(fopAcM_GetParam(this));
        if(reserved_slot<tickets.size() && tickets[reserved_slot].id==fopAcM_GetID(this)) tickets[reserved_slot]=Ticket{};
        if(fopAcM_CheckCondition(this,fopAcCnd_INIT_e)) {
            if(transferring() && epoch==party.epoch()) party.cancel_throw(serial);
            link_throw.end(serial);
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
            if(wild->combat.creature.hp && !wild->transferring() && length<nearest) { nearest=length; best=wild; }
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
    const auto ball=owner->ball.frame();
    if(!particles) {
    if(ball.scale>.001f) {
        Mtx model;MTXCopy(owner->matrix,model);
        if(ball.scale<1) {
            const auto body=owner->center(),target=owner->ball_position;
            const auto center=body+(target-body)*(1-ball.scale);
            for(int r=0;r<3;++r) {
                for(int c=0;c<3;++c)model[r][c]*=ball.scale;
                const float origin=r==0?body.x:r==1?body.y:body.z;
                const float at=r==0?center.x:r==1?center.y:center.z;
                model[r][3]=at+(model[r][3]-origin)*ball.scale;
            }
        }
        Mtx view_model; MTXConcat(view,model,view_model);
        owner->held->draw(owner->pose,view_model,ball.opacity);
        if(owner->combat.creature.hp && !owner->transferring()) {
            cXyz top=owner->current.pos;top.y+=owner->center_height()+owner->species().height*.6f+10;
            draw_health(top,float(owner->combat.creature.hp)/owner->combat.creature.max_hp,owner->companion,view);
        }
    }
    if(ball.show_ball)draw_ball(owner->ball_position,ball.wobble,view,ball.opening,ball.spin,ball.glow);
    } else {
    effects::Frame effects;const auto origins=owner->origins();
    if(ball.scale>.001f) {
        effects::ambient(effects,owner->combat.creature.species,owner->age/30.f,origins);
        if(owner->transferring())for(auto& sprite:effects.sprites) {
            for(int axis=0;axis<3;++axis)sprite.position[axis]=origins.body[axis]+(point(owner->ball_position)[axis]-origins.body[axis])*(1-ball.scale)+(sprite.position[axis]-origins.body[axis])*ball.scale;
            sprite.width*=ball.scale;sprite.height*=ball.scale;sprite.color[3]*=ball.scale;
        }
    }
    if(!owner->transferring() && owner->move_seconds>=0) {
        if(owner->combat.creature.species==9 && owner->combat.move==Move::HydroPump) {
            for(const auto cannon:{"Bone_19","Bone_24"})for(std::size_t j=0;j<owner->held->model().joints().size();++j)
                if(owner->held->model().joints()[j].name==cannon) {
                    const auto& p=owner->pose.joints[j];const Vec source{p[0],p[1],p[2]};Vec at{};MTXMultVec(owner->matrix,&source,&at);
                    auto nozzle=origins;nozzle.mouth={at.x,at.y,at.z};
                    effects::move(effects,Visual::Water,owner->move_seconds,nozzle,point(owner->spark_end),static_cast<std::uint32_t>(owner->serial));
                }
        } else effects::move(effects,move_info(owner->combat.move)->visual,owner->move_seconds,origins,point(owner->spark_end),static_cast<std::uint32_t>(owner->serial));
    }
    if(ball.transfer>=0)effects::transfer(effects,ball.transfer,origins.body,point(owner->ball_position),origins.size,ball.emerging);
    if(owner->ball.stage()==BallStage::Success) {
        auto at=point(owner->ball_position);at[1]+=12;
        for(int i=0;i<5;++i) {
            const float angle=i*1.256637f,t=ball.progress;
            auto p=at;p[0]+=std::cos(angle)*t*48;p[1]+=std::sin(angle)*t*48+20*t;
            effects.sprites.push_back({effects::Image::Star,p,12,12,t*2,{.4f,1,.55f,1-t},true});
        }
    }
    draw_effects(effects,view);
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
    if(!party.active() || !action_ready() || tickets[slot].reserved) return false;
    auto* player=dComIfGp_getPlayer(0);
    const float angle=float(player->shape_angle.y)*3.14159265358979323846f/32768.f+(float(slot)-1)*0.5f;
    cXyz at=player->current.pos;
    const bool companion=slot==companion_slot;
    at.x+=std::sin(angle)*(companion?140.f:240.f); at.z+=std::cos(angle)*(companion?140.f:240.f);
    cXyz from=player->current.pos,to=at; from.y+=40; to.y+=40;
    if(!ground_at(at) || !clear_line(from,to,player)) { message("Move onto open ground before spawning.",true); return false; }
    auto& ticket=tickets[slot]; ticket=Ticket{};
    if(companion) {
        const auto& p=party.party();
        if(!p.count || !p.members[p.selected].hp) { message("Choose a healthy Pokemon or rest your party.",true); return false; }
        ticket.creature=p.members[p.selected];
    } else ticket.creature=make_creature(roster()[encounter_species].id,12);
    std::string error;
    ticket.asset=content::get(ticket.creature.species,error);
    if(!ticket.asset) {ticket=Ticket{};message(error,true);return false;}
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
void next_species(ModContext*,void*) {
    encounter_species=(encounter_species+1)%roster().size();dirty=true;
    message(std::string("Next encounter: ")+roster()[encounter_species].name,true);
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
    if(!target->begin_ball()) message("Sheathe your weapon and stand on open ground before throwing. A writable save is required.",true);
}
void summon(ModContext*,void*) {
    if(!action_ready())return;
    if(auto* actor=actor_for(companion_slot)) {actor->begin_recall();message("Recalling your Pokemon.",true);}
    else if(!tickets[companion_slot].reserved) {
        if(!LinkThrow::allowed() || link_throw.owner) {message("Sheathe your weapon and stand on open ground to summon.",true);return;}
        spawn_slot(companion_slot);
    }
}
void next_member(ModContext*,void*) {
    if(party.select_next(1)) message(std::string("Selected ")+species_info(party.party().members[party.party().selected].species)->name+".",true);
}
void starter(ModContext*,void* value) {
    if(has_actors()) {message("Recall your Pokemon and clear the encounter first.",true);return;}
    const auto id=static_cast<std::uint16_t>(reinterpret_cast<std::uintptr_t>(value));
    message(party.choose_starter(id)?std::string(species_info(id)->name)+" joined your party! Hold R + Down to summon.":"A starter is available once, while this save has an empty party.",true);
}
void toggle_encounters(ModContext*,void*) {
    if(svc_config->set_bool(mod_ctx,encounters_config,!natural_encounters)!=MOD_OK) {message("Encounter setting could not be saved.",true);return;}
    natural_encounters=!natural_encounters;encounter_distance=0;
    message(natural_encounters?"Wild encounters enabled in areas with ordinary enemies.":"Wild encounters paused. Manual encounters remain available.",true);
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
bool no_world(ModContext*,void*) { return !party.active() || !action_ready(); }
bool no_party(ModContext*,void*) { return no_world(nullptr,nullptr) || !party.party().count; }
ModResult button(UiElementHandle pane,const char* label,UiPressedFn fn,UiPredicateFn disabled=no_world) {
    UiControlDesc desc=UI_CONTROL_DESC_INIT; desc.label=label; desc.on_pressed=fn; desc.is_disabled=disabled;
    return svc_ui->pane_add_control(mod_ctx,pane,&desc,nullptr);
}
std::string party_text() {
    if(!party.active()) return read_failed?"Party unavailable: original save data preserved.":"Load a Zelda save to use the party.";
    const auto& p=party.party();
    std::string text="Poke Balls: "+std::to_string(p.balls)+" | Party: "+std::to_string(p.count)+"/6\nWild encounters: "+(natural_encounters?"On":"Paused")+"\nNext encounter: "+roster()[encounter_species].name+"\n";
    for(std::size_t i=0;i<p.count;++i) {
        const auto& c=p.members[i];
        text+=(i==p.selected?"> ":"  ")+std::to_string(i+1)+". "+species_info(c.species)->name+" #"+std::to_string(c.id)+" Lv."+std::to_string(c.level)+
              " | HP "+std::to_string(c.hp)+"/"+std::to_string(c.max_hp)+" | PP ";
        for(std::size_t j=0;j<move_capacity;++j) text+=std::to_string(c.pp[j])+(j+1==move_capacity?"\n":"/");
    }
    return text;
}
}
bool has_actors() { for(const auto& ticket:tickets) if(ticket.reserved) return true; return false; }
void update() {
    if(!initialized) return;
    if(!scene_ready()) { attempted_player=encounter_player=no_actor;encounter_distance=0; return; }
    auto* player=dComIfGp_getPlayer(0);
    if(!party.active() && !read_failed && action_ready() && attempted_player!=fopAcM_GetID(player)) {
        attempted_player=fopAcM_GetID(player); load_party();
    }
    // Movement, not presentation frames, controls encounter frequency. Keep
    // towns, empty puzzle rooms, cutscenes and save transitions undisturbed.
    if(encounter_player!=fopAcM_GetID(player)) {
        encounter_player=fopAcM_GetID(player);encounter_position=player->current.pos;encounter_distance=0;return;
    }
    const auto traveled=distance(encounter_position,player->current.pos);encounter_position=player->current.pos;
    if(!natural_encounters || !party.active() || !action_ready() || dComIfGp_isPauseFlag() || party.pending())return;
    for(std::size_t i=0;i<companion_slot;++i)if(tickets[i].reserved)return;
    if(traveled<40)encounter_distance+=traveled;
    if(encounter_distance<1000)return;
    struct Nearby {fopAc_ac_c* player;bool found=false;} nearby{player};
    fopAcM_Search([](void* value,void* user)->void* {
        auto& near=*static_cast<Nearby*>(user);auto* actor=static_cast<fopAc_ac_c*>(value);
        if(fopAcM_IsActor(actor) && ordinary_enemy(actor) && actor->health>0 && fopAcM_IsExecuting(fopAcM_GetID(actor)) &&
           actor->layer_tag.layer==near.player->layer_tag.layer && fopAcM_GetRoomNo(actor)==fopAcM_GetRoomNo(near.player) &&
           distance(actor->current.pos,near.player->current.pos)<900)near.found=true;
        return nullptr;
    },&nearby);
    if(!nearby.found) {encounter_distance=1000;return;}
    const auto selected=encounter_species;encounter_species=random_next(encounter_random)%roster().size();
    spawn_slot(1);encounter_species=selected;encounter_distance=0;
}
ModResult initialize(ModError* error) {
    if(!svc_save || !svc_hook) return mods::set_error(error,MOD_UNAVAILABLE,"Save and hook services required for Pokemon encounters");
    ConfigVarDesc config=CONFIG_VAR_DESC_INIT;config.name="wild-encounters";config.default_bool=true;
    auto r=svc_config->register_var(mod_ctx,&config,&encounters_config);if(r!=MOD_OK)return r;
    r=svc_config->get_bool(mod_ctx,encounters_config,&natural_encounters);if(r!=MOD_OK)return r;
    r=svc_actor->register_actor(mod_ctx,&descriptor,&field_profile,&registration);
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
    save_observer=0; party.deactivate(); tickets={}; status_element=party_element=0;
    if(encounters_config && svc_config)svc_config->unregister_var(mod_ctx,encounters_config);
    encounters_config=0;encounter_player=no_actor;encounter_distance=0;
    read_failed=false; save_slot=-1; attempted_player=no_actor; return MOD_OK;
}
ModResult build_panel(UiElementHandle pane) {
    update(); status_element=party_element=0; dirty=true; last_party_text=party_text();
    auto r=svc_ui->pane_add_section(mod_ctx,pane,"Pokemon Mode"); if(r!=MOD_OK) return r;
    r=svc_ui->pane_add_text(mod_ctx,pane,"Explore with 20 Pokemon in Vanilla or Randomizer. Wild Pokemon appear as you travel through enemy areas. Sheathe your weapon to throw. Hold R + D-pad Up to throw; Down to summon/recall; Left/Right to select your party member. Lower HP improves capture odds.",nullptr); if(r!=MOD_OK) return r;
    r=svc_ui->pane_add_text(mod_ctx,pane,"Save through Zelda's normal save menu to keep your party and Poke Balls. Each Zelda save has its own party.",nullptr); if(r!=MOD_OK) return r;
    r=svc_ui->pane_add_text(mod_ctx,pane,status.c_str(),&status_element); if(r!=MOD_OK) return r;
    r=svc_ui->pane_add_text(mod_ctx,pane,last_party_text.c_str(),&party_element); if(r!=MOD_OK) return r;
    for(const auto id:{1,4,7}) {
        const auto label=std::string("Choose starter: ")+species_info(static_cast<std::uint16_t>(id))->name;
        UiControlDesc choice=UI_CONTROL_DESC_INIT;choice.label=label.c_str();choice.on_pressed=starter;
        choice.user_data=reinterpret_cast<void*>(static_cast<std::uintptr_t>(id));
        choice.is_disabled=[](ModContext*,void*){return !party.active() || party.party().count || has_actors();};
        r=svc_ui->pane_add_control(mod_ctx,pane,&choice,nullptr);if(r!=MOD_OK)return r;
    }
    r=button(pane,"Toggle wild encounters",toggle_encounters);if(r!=MOD_OK)return r;
    r=button(pane,"Start wild encounter",spawn_wild); if(r!=MOD_OK) return r;
    r=button(pane,"Next encounter species",next_species); if(r!=MOD_OK) return r;
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
