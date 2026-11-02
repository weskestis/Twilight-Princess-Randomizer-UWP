#include "animation.hpp"
#include "roster.hpp"
#include <algorithm>
#include <cmath>
#include <string_view>

namespace pokemon_battle::field {
namespace {
constexpr double tick=1.0/30.0;
std::size_t find_clip(const runtime::Model& model, std::string_view name, std::size_t fallback=0) {
    for(std::size_t i=0;i<model.clips().size();++i) if(model.clips()[i].name.ends_with(name)) return i;
    return fallback<model.clips().size()?fallback:0;
}
int angle_delta(std::int16_t angle,std::int16_t target) {
    int delta=int(target)-int(angle);
    if(delta>32767) delta-=65536;
    if(delta<-32768) delta+=65536;
    return delta;
}
double progress(int timer,int duration) { return std::clamp(1.0-double(timer)/duration,0.0,1.0); }
double attack_position(const Combat& combat) {
    // The original clips have different anticipation/recovery lengths. These
    // contact windows map the visual attack onto the actual six-tick strike.
    double contact=0.42, release=0.58;
    switch(combat.move) {
    case Move::ThunderShock: contact=0.55; release=0.80; break;
    case Move::QuickAttack: contact=0.50; release=0.66; break;
    case Move::Growl: contact=0.30; release=0.65; break;
    case Move::TailWhip: contact=0.48; release=0.72; break;
    case Move::Struggle: contact=0.35; release=0.55; break;
    default:break;
    }
    if(combat.phase==Phase::Windup) return progress(combat.timer,23)*contact;
    if(combat.phase==Phase::Strike) return contact+progress(combat.timer,5)*(release-contact);
    return release+progress(combat.timer,44)*(1-release);
}
void blend(const runtime::Pose& from,runtime::Pose& to,float amount) {
    if(from.vertices.size()!=to.vertices.size()) return;
    for(std::size_t i=0;i<to.vertices.size();++i) {
        auto& out=to.vertices[i]; const auto& old=from.vertices[i];
        float length=0;
        for(int axis=0;axis<3;++axis) {
            out.position[axis]=old.position[axis]+(out.position[axis]-old.position[axis])*amount;
            out.normal[axis]=old.normal[axis]+(out.normal[axis]-old.normal[axis])*amount;
            length+=out.normal[axis]*out.normal[axis];
        }
        if(length>1e-10f) for(auto& n:out.normal) n/=std::sqrt(length);
        else out.normal={0,1,0};
    }
    if(from.joints.size()==to.joints.size()) for(std::size_t j=0;j<to.joints.size();++j)
        for(std::size_t axis=0;axis<3;++axis)to.joints[j][axis]=from.joints[j][axis]+(to.joints[j][axis]-from.joints[j][axis])*amount;
    // Facial atlases are discrete images: retain the destination UV state.
}
}
std::int16_t turn_toward(std::int16_t angle,std::int16_t target,int limit) {
    const int delta=std::clamp(angle_delta(angle,target),-std::max(0,limit),std::max(0,limit));
    int result=int(angle)+delta;
    if(result>32767) result-=65536;
    if(result<-32768) result+=65536;
    return static_cast<std::int16_t>(result);
}
float forward_alignment(std::int16_t angle,std::int16_t target) {
    return std::max(0.f,static_cast<float>(std::cos(double(angle_delta(angle,target))*3.14159265358979323846/32768.0)));
}
bool Animation::sample(const runtime::Model& model,const Combat& combat,float distance,runtime::Pose& pose,bool advance) {
    if(model.clips().empty() || !std::isfinite(distance) || distance<0) return false;
    if(!advance && clip_!=static_cast<std::size_t>(-1)) return true;
    const bool moving=combat.phase==Phase::Ready && distance>0.02f;
    const auto idle=find_clip(model,"_Idle");
    const auto walk=find_clip(model,"_FieldWalk",find_clip(model,"_15_Walk",idle));
    const auto run=find_clip(model,"_FieldRun",find_clip(model,"_16_Run",walk));
    const auto* species=species_info(combat.creature.species);
    const bool new_rig=model.bone_count()>0 && species;
    const float walk_length=new_rig?species->walk_stride:walk_stride;
    const float run_length=new_rig?species->run_stride:run_stride;
    std::size_t selected=idle;
    bool loop=false;
    if(moving) {
        // Hysteresis avoids rapid walk/run switching during gradual acceleration.
        const float threshold=new_rig?std::max(1.f,walk_length/18.f):4.5f;
        running_=running_?distance>threshold*.8444444f:distance>threshold;
        selected=running_?run:walk;
        if(advance) gait_phase_=std::fmod(gait_phase_+double(distance)/std::max(.01f,running_?run_length:walk_length),1.0);
    } else if(combat.phase!=Phase::Ready) {
        const int requested=combat.clip();
        selected=requested>=0 && static_cast<std::size_t>(requested)<model.clips().size()?static_cast<std::size_t>(requested):idle;
    }
    const bool changed=selected!=clip_ || combat.began_move;
    if(changed) { from_=pose; clip_=selected; blend_tick_=0; }
    else if(advance && blend_tick_<4) ++blend_tick_;
    const auto& clip=model.clips()[selected];
    const double duration=double(clip.frames-1)/clip.fps;
    if(moving && selected!=idle) sample_time_=gait_phase_*duration;
    else if(combat.phase==Phase::Ready) {
        if(advance) idle_time_=std::fmod(idle_time_+tick,double(clip.frames)/clip.fps);
        sample_time_=idle_time_; loop=true;
    } else if(combat.phase==Phase::Hurt) sample_time_=progress(combat.timer,16)*duration;
    else if(combat.phase==Phase::Fainted) sample_time_=std::min(duration,combat.timer*tick);
    else sample_time_=attack_position(combat)*duration;
    if(!model.sample(selected,sample_time_,loop,pose)) return false;
    if(!from_.vertices.empty() && blend_tick_<4) {
        const float t=float(blend_tick_+1)/4.f;
        blend(from_,pose,t*t*(3.f-2.f*t));
    }
    return true;
}
}
