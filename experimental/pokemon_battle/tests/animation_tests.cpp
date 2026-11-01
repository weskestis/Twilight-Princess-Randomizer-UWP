#include "../field/animation.hpp"
#include "runtime_fixture.hpp"
#include <cmath>
#include <iostream>
#include <limits>

using namespace pokemon_battle;
using fixture::require;
namespace {
runtime::Model model_fixture() {
    const auto triangle=fixture::make();
    fixture::Bytes data(triangle.begin(),triangle.begin()+188);
    constexpr const char* names[]={"pikachu_00_Idle","pikachu_01_Physical_B","pikachu_02_Physical+Extra_3",
        "pikachu_03_Physical_C","pikachu_04_Extra_4","pikachu_05_Special+Extra_1","pikachu_06_Extra_2",
        "pikachu_07_Special_A","pikachu_08_Special_C","pikachu_09_Physical","pikachu_10_Faint",
        "pikachu_11_Damage","pikachu_12_Sub_Blink","pikachu_13_Sub_SleepOnPose","pikachu_14_Sub_SleepOffPose",
        "pikachu_15_Walk","pikachu_16_Run"};
    fixture::overwrite(data,32,17);
    for(const auto* name:names) {
        const auto begin=data.size();
        data.insert(data.end(),triangle.begin()+188,triangle.end());
        std::fill_n(data.begin()+static_cast<std::ptrdiff_t>(begin),64,std::uint8_t(0));
        std::memcpy(data.data()+begin,name,std::strlen(name));
    }
    fixture::checksum(data);
    runtime::Model model; std::string error;
    require(model.load(data,error),error.c_str()); return model;
}
void locomotion(const runtime::Model& model) {
    field::Animation animation; field::Combat combat; runtime::Pose pose;
    require(animation.sample(model,combat,0,pose,false),"initial pose");
    require(animation.clip()==0,"initial idle");
    for(int i=0;i<20;++i) require(animation.sample(model,combat,1.4f,pose),"walk pose");
    require(animation.clip()==15,"moving actor must use a walking clip, not idle");
    require(std::abs(animation.gait_phase())<1e-5 || std::abs(animation.gait_phase()-1)<1e-5,"one stride per actual 28 units");
    const auto phase=animation.gait_phase();
    for(int i=0;i<10;++i) animation.sample(model,combat,0,pose);
    require(animation.clip()==0 && animation.gait_phase()==phase,"blocked/stopped actor returns idle without advancing feet");
    animation.sample(model,combat,5,pose);
    require(animation.clip()==16,"running at follow speed");
    animation.sample(model,combat,4.2f,pose);
    require(animation.clip()==16,"run threshold hysteresis");
    animation.sample(model,combat,3.7f,pose);
    require(animation.clip()==15,"slow down to walk");
    const auto paused_phase=animation.gait_phase(), paused_time=animation.sample_time();
    const auto point=pose.vertices[0].position;
    animation.sample(model,combat,5,pose,false);
    require(animation.gait_phase()==paused_phase && animation.sample_time()==paused_time && pose.vertices[0].position==point,"paused movement must not advance animation");
    require(!animation.sample(model,combat,std::numeric_limits<float>::quiet_NaN(),pose),"invalid movement rejected");
    runtime::Model old; std::string error; require(old.load(fixture::make(),error),"legacy pack");
    field::Animation legacy;
    require(legacy.sample(old,combat,5,pose) && legacy.clip()==0,"old packs remain safe");
}
void transitions(const runtime::Model& model) {
    field::Animation animation; field::Combat combat; runtime::Pose pose;
    animation.sample(model,combat,0,pose,false);
    model.sample(0,1,false,pose); // visibly different old pose, x=1
    combat.phase=field::Phase::Windup; combat.move=field::Move::QuickAttack;
    combat.timer=23; combat.began_move=true;
    animation.sample(model,combat,0,pose);
    require(animation.clip()==2 && std::abs(pose.vertices[0].position[0]-0.84375f)<1e-5f,"four-tick eased crossfade, not a hard pose snap");
    require(pose.materials[0].uv[2]==0,"facial atlases must step to destination, never crossfade");
    combat.began_move=false;
    combat.timer=0; animation.sample(model,combat,0,pose);
    const auto contact=animation.sample_time();
    combat.phase=field::Phase::Strike; combat.timer=5; animation.sample(model,combat,0,pose);
    require(animation.sample_time()==contact,"windup and strike are contiguous");
    combat.timer=0; animation.sample(model,combat,0,pose);
    const auto release=animation.sample_time();
    combat.phase=field::Phase::Recover; combat.timer=44; animation.sample(model,combat,0,pose);
    require(animation.sample_time()==release && release>contact,"strike and recovery are contiguous");
    combat.timer=0; animation.sample(model,combat,0,pose);
    require(pose.finished,"recovery reaches original clip end before returning idle");
    combat.phase=field::Phase::Hurt; combat.timer=16; animation.sample(model,combat,0,pose);
    require(animation.clip()==11 && animation.sample_time()==0,"hurt starts at the beginning");
    combat.timer=0; animation.sample(model,combat,0,pose);
    require(pose.finished,"hurt no longer truncates before the recovery pose");
    combat.phase=field::Phase::Fainted; combat.timer=60; animation.sample(model,combat,0,pose);
    require(animation.clip()==10 && pose.finished,"faint holds final pose");
    for(const auto& vertex:pose.vertices) {
        float length=0; for(float n:vertex.normal) length+=n*n;
        require(std::abs(length-1)<1e-5,"transition normals stay normalized");
    }
}
}
int main() {
    const auto model=model_fixture(); locomotion(model); transitions(model);
    require(std::abs(int(field::turn_toward(0,-32768)))==2048,"180 degree turn is bounded");
    require(field::turn_toward(32000,-32000)==-32000,"turn takes shortest arc across wrap");
    require(field::forward_alignment(0,-32768)==0,"no backwards sliding while facing away");
    require(field::forward_alignment(1234,1234)==1,"straight movement unmodified");
    std::cout<<"PASS: distance-matched walk/run, blocked movement, pause, turn limits, blended poses and complete attack/hurt playback\n";
}
