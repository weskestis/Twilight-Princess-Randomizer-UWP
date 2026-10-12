#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Independent implementation of secondary hair motion, informed by the
// spring/damping approach in ByTheEightDivines' Skirt Physics mod.
// Input targets come from Link's existing wind/hair update. Time is one
// simulation frame, not one rendered/interpolated frame.
namespace fierce_hair {
constexpr float kRadiansPerBam=3.14159265358979323846f/32768.0f;
constexpr std::array<int,5> kHairJoints{1,2,3,4,5};
constexpr std::array<float,5> kLimits{0.080f,0.130f,0.105f,0.105f,0.105f};
struct Input {
    std::array<float,3> head_position{};
    std::array<float,5> target_x{},target_z{};
    float facing=0;
    bool active=true;
};
struct Axis {float angle=0,velocity=0;};
struct Output {std::array<float,5> x{},z{};};
class Spring {
    std::array<Axis,5> x_{},z_{};
    std::array<float,3> last_position_{},filtered_velocity_{};
    float last_facing_=0;
    bool initialized_=false;
    static void integrate(Axis& a,float target,float drive,float spring,float damping,float limit) {
        target=std::clamp(target,-limit,limit);
        a.velocity+=spring*(target-a.angle)-damping*a.velocity+drive;
        a.angle+=a.velocity;
        if(a.angle>limit){a.angle=limit;a.velocity=std::min(a.velocity,0.0f);}
        if(a.angle<-limit){a.angle=-limit;a.velocity=std::max(a.velocity,0.0f);}
    }
    static bool finite(const Input& in) {
        if(!std::isfinite(in.facing))return false;
        for(float v:in.head_position)if(!std::isfinite(v))return false;
        for(float v:in.target_x)if(!std::isfinite(v))return false;
        for(float v:in.target_z)if(!std::isfinite(v))return false;
        return true;
    }
public:
    void reset(){*this=Spring{};}
    Output step(const Input& in) {
        Output out{};
        if(!in.active||!finite(in)){reset();return out;}
        float distance2=0;
        for(int i=0;i<3;++i){float d=in.head_position[i]-last_position_[i];distance2+=d*d;}
        if(!initialized_||distance2>300.0f*300.0f){
            reset();initialized_=true;last_position_=in.head_position;last_facing_=in.facing;
            return out;
        }
        std::array<float,3> acceleration{};
        for(int i=0;i<3;++i){
            float velocity=in.head_position[i]-last_position_[i];
            float next=filtered_velocity_[i]+0.30f*(velocity-filtered_velocity_[i]);
            acceleration[i]=std::clamp(next-filtered_velocity_[i],-2.5f,2.5f);
            filtered_velocity_[i]=next;
        }
        last_position_=in.head_position;
        float turn=std::clamp(std::remainder(in.facing-last_facing_,6.28318530718f),-0.35f,0.35f);
        last_facing_=in.facing;
        float s=std::sin(in.facing),c=std::cos(in.facing);
        float lateral_acc=acceleration[0]*c-acceleration[2]*s;
        float forward_acc=acceleration[0]*s+acceleration[2]*c;
        for(int i=0;i<5;++i){
            bool tip=(i==1||i>=3);
            float spring=tip?0.075f:0.14f,damping=tip?0.32f:0.46f;
            float gain=tip?0.0023f:0.0009f;
            float chain_x=i==1?-0.12f*x_[0].velocity:0;
            float chain_z=i==1?-0.12f*z_[0].velocity:0;
            integrate(x_[i],0.25f*in.target_x[i],-gain*forward_acc+chain_x,spring,damping,kLimits[i]);
            integrate(z_[i],0.25f*in.target_z[i],gain*lateral_acc-0.002f*turn+chain_z,spring,damping,kLimits[i]);
            out.x[i]=x_[i].angle;out.z[i]=z_[i].angle;
        }
        return out;
    }
};
inline std::int16_t bam(float radians){return static_cast<std::int16_t>(std::lround(radians/kRadiansPerBam));}
}
