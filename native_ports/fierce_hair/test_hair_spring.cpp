#include "hair_spring.hpp"
#include <cassert>
#include <iostream>
#include <limits>
using namespace fierce_hair;
int main(){
    Spring sim;Input input;Output out{};int checked=0;
    // Variable walking, turning, gust targets and abrupt movement over 50k
    // actual simulation frames. Limits must remain finite on every joint.
    for(int frame=0;frame<50000;++frame){
        const float t=static_cast<float>(frame);
        input.head_position={30.0f*std::sin(t*.021f),150.0f+4.0f*std::sin(t*.19f),t*.7f};
        input.facing=std::remainder(t*.04f,6.28318530718f);
        for(int i=0;i<5;++i){const float phase=static_cast<float>(i);input.target_x[i]=1.2f*std::sin(t*.13f+phase);input.target_z[i]=.8f*std::cos(t*.07f+phase);}
        out=sim.step(input);
        for(int i=0;i<5;++i){assert(std::isfinite(out.x[i])&&std::isfinite(out.z[i]));assert(std::abs(out.x[i])<=kLimits[i]+1e-6);assert(std::abs(out.z[i])<=kLimits[i]+1e-6);++checked;}
    }
    // Teleport and form changes cannot carry a stored hair impulse onward.
    input.head_position[0]+=1000;out=sim.step(input);for(float v:out.x)assert(v==0);
    input.active=false;out=sim.step(input);for(float v:out.z)assert(v==0);
    input.active=true;input.head_position={0,150,0};input.target_x.fill(0);input.target_z.fill(0);input.facing=0;
    sim.step(input);input.head_position[2]+=8;out=sim.step(input);
    bool moved=false;for(float v:out.x)moved|=std::abs(v)>1e-6;assert(moved);
    for(int i=0;i<1500;++i)out=sim.step(input);
    for(float v:out.x){assert(std::abs(v)<1e-6);}
    for(float v:out.z){assert(std::abs(v)<1e-6);}
    input.target_x[0]=std::numeric_limits<float>::quiet_NaN();out=sim.step(input);for(float v:out.x)assert(v==0);
    std::cout<<"PASS: 50000 simulation frames; "<<checked<<" joint bounds; teleport/form reset, settling, nonfinite input\n";
}
