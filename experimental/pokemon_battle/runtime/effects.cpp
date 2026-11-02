#include "effects.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace pokemon_battle::effects {
namespace {
constexpr float pi=3.14159265358979323846f;
std::uint32_t u32(const std::uint8_t* p) {return p[0]|std::uint32_t(p[1])<<8|std::uint32_t(p[2])<<16|std::uint32_t(p[3])<<24;}
float clamp(float x) {return std::clamp(x,0.f,1.f);}
Point mix(const Point& a,const Point& b,float t) {return {a[0]+(b[0]-a[0])*t,a[1]+(b[1]-a[1])*t,a[2]+(b[2]-a[2])*t};}
Point offset(Point p,float x,float y,float z=0) {p[0]+=x;p[1]+=y;p[2]+=z;return p;}
float noise(int index,std::uint32_t seed) {
    auto value=std::uint32_t(index)*747796405u+seed*2891336453u+277803737u;
    value=((value>>((value>>28)+4))^value)*277803737u;return float((value>>22)^value)/4294967295.f;
}
void sprite(Frame& f,Image image,Point p,float size,Color color,float angle=0,float aspect=1,bool add=true) {
    if(f.sprites.size()>=256 || size<=.001f || color[3]<=.001f)return;
    f.sprites.push_back({image,p,size,size*aspect,angle,color,add});
}
void ribbon(Frame& f,Point a,Point b,float width,Color color) {
    if(f.ribbons.size()<192 && width>0 && color[3]>0)f.ribbons.push_back({a,b,width,color});
}
void burst(Frame& f,Point at,float age,Color color,std::uint32_t seed) {
    if(age<0 || age>.65f)return;
    const float fade=1-age/.65f;color[3]*=fade;
    sprite(f,Image::Flash,at,20+age*80,color,age*2);
    sprite(f,Image::Ring,at,12+age*130,color,-age);
    for(int i=0;i<12;++i) {
        const float angle=2*pi*noise(i,seed),speed=30+55*noise(i+25,seed);
        sprite(f,Image::Spark,offset(at,std::cos(angle)*age*speed,std::sin(angle)*age*speed,15*std::sin(angle*3)*age),3+fade*5,color,angle);
    }
}
}
bool Pack::load(std::span<const std::uint8_t> bytes,std::string& error) {
    try {
        if(bytes.size()<20 || bytes.size()>4u*1024u*1024u || std::memcmp(bytes.data(),"DPFX0001",8) ||
           u32(bytes.data()+8)!=bytes.size() || u32(bytes.data()+12)!=runtime::crc32(bytes.subspan(16)) ||
           u32(bytes.data()+16)!=image_names.size())throw std::runtime_error("Invalid Pokemon effect pack");
        Pack next;std::size_t at=20;
        for(const char* name:image_names) {
            if(bytes.size()-at<44 || !std::memchr(bytes.data()+at,0,32) || std::strcmp(reinterpret_cast<const char*>(bytes.data()+at),name))
                throw std::runtime_error("Missing effect texture");
            const auto w=u32(bytes.data()+at+32),h=u32(bytes.data()+at+36),size=u32(bytes.data()+at+40);at+=44;
            if(w<4 || h<4 || w>512 || h>512 || w%4 || h%4 || size!=w*h*4 || size>bytes.size()-at)
                throw std::runtime_error("Invalid effect pixels");
            next.textures_.push_back({static_cast<std::uint16_t>(w),static_cast<std::uint16_t>(h),at,size});at+=size;
        }
        if(at!=bytes.size())throw std::runtime_error("Trailing effect data");
        next.bytes_.assign(bytes.begin(),bytes.end());*this=std::move(next);error.clear();return true;
    } catch(const std::exception& e) {error=e.what();return false;}
}
std::span<const std::uint8_t> Pack::pixels(std::size_t i) const {
    if(i>=textures_.size())return {};
    const auto& t=textures_[i];return std::span(bytes_).subspan(t.offset,t.bytes);
}
void ambient(Frame& f,std::uint16_t species,float t,const Origins& o) {
    if(!std::isfinite(t) || t<0)return;
    if(species==4 || species==5 || species==6) {
        const float size=std::clamp(o.size*.2f,12.f,28.f);
        sprite(f,Image::Glow,o.tail,size*1.5f,{1,.22f,.01f,.45f});
        for(int i=0;i<9;++i) {
            const float age=std::fmod(t*1.9f+i/9.f,1.f),s=size*(1-age*.8f);
            auto p=offset(o.tail,std::sin(t*7+i*2.f)*size*.10f,age*size*1.5f, std::cos(t*5+i)*size*.08f);
            sprite(f,Image::Fire,p,s,{1,.15f+.70f*(1-age),.02f,(1-age)*.85f},std::sin(t*2+i)*.3f,1.4f);
        }
    }
    if(species==92)for(int i=0;i<14;++i) {
        const float angle=t*.5f+2*pi*i/14,ring=o.size*.42f;
        auto p=offset(o.body,std::cos(angle)*ring,std::sin(angle)*ring*.8f,std::sin(angle*2)*ring*.4f);
        sprite(f,Image::Gas,p,o.size*.7f,{.45f,.18f,.68f,.36f},t*.3f+i,1,false);
    }
}
void move(Frame& f,field::Visual visual,float t,const Origins& o,const Point& target,std::uint32_t seed) {
    using field::Visual;
    if(!std::isfinite(t) || t<0 || t>2.5f)return;
    constexpr float contact=23.f/30.f;
    const float charge=clamp(t/contact),age=t-contact,travel=clamp(age/.17f),fade=clamp(1-(age-.2f)/.55f);
    const bool active=age>=0 && age<.75f;
    const float spin=float(seed%71)+t*4;
    const Point tip=mix(o.mouth,target,travel);
    Color hit{1,.8f,.4f,.8f};
    if(visual==Visual::Electric) {
        for(const auto& cheek:{o.left,o.right}) {
            sprite(f,Image::Glow,cheek,5+charge*16,{1,.65f,.02f,age<0?charge*.8f:fade});
            if(charge>.25f && age<.35f)sprite(f,Image::Bolt,cheek,12+charge*o.size*.6f,{.65f,.82f,1,charge*.8f},spin);
            if(active && age<.25f) {
                Point previous=cheek;
                for(int i=1;i<=12;++i) {
                    auto p=mix(cheek,target,i/12.f);
                    if(i<12)p=offset(p,(noise(i+int(t*60),seed)-.5f)*14,(noise(i+100+int(t*60),seed)-.5f)*18);
                    ribbon(f,previous,p,7,{.25f,.48f,1,fade*.35f});
                    ribbon(f,previous,p,3.5f,{1,.72f,.04f,fade*.8f});
                    ribbon(f,previous,p,1.1f,{1,1,.9f,fade});previous=p;
                    if(i%3==0)sprite(f,Image::BoltBranch,p,30,{.9f,.9f,1,fade*.75f},spin+i);
                }
            }
        }
        if(age>=0)burst(f,target,age,{1,.85f,.18f,.8f},seed);
        return;
    }
    if(visual==Visual::Flame || visual==Visual::Dragon) {
        const bool dragon=visual==Visual::Dragon;
        sprite(f,Image::Glow,o.mouth,4+charge*15,dragon?Color{.3f,.4f,1,age<0?charge:fade}:Color{1,.27f,.02f,age<0?charge*.7f:fade});
        if(active && age<.4f)for(int i=0;i<22;++i) {
            const float u=std::fmod(i/22.f+age*2.2f,1.f)*travel;
            auto p=mix(o.mouth,target,u);p=offset(p,(noise(i,seed)-.5f)*u*28,std::sin(u*7+t*8)*u*10);
            sprite(f,Image::Fire,p,8+u*33,dragon?Color{.3f+.5f*u,.3f,.95f,fade*.7f}:Color{1,.72f-.6f*u,.015f,fade*.75f},spin+i,1.3f);
        }
        hit=dragon?Color{.6f,.3f,1,.8f}:Color{1,.35f,.03f,.9f};
    } else if(visual==Visual::Water || visual==Visual::Ice || visual==Visual::Bubble) {
        const bool ice=visual==Visual::Ice,bubble=visual==Visual::Bubble;
        if(age<0)sprite(f,Image::Glow,o.mouth,charge*18,{.3f,.8f,1,charge*.65f});
        if(active && age<.4f) {
            if(!bubble)ribbon(f,o.mouth,tip,ice?5.f:9.f,{.35f,.8f,1,fade*.5f});
            for(int i=0;i<(bubble?10:24);++i) {
                const float u=std::fmod(i/(bubble?10.f:24.f)+age*2.5f,1.f)*travel;
                auto p=mix(o.mouth,target,u);p=offset(p,std::sin(i*2.f+t*8)*(bubble?14:5),std::cos(i*3.f+t*7)*(bubble?10:5));
                sprite(f,bubble?Image::Bubble:ice?Image::Star:Image::Droplet,p,bubble?12.f:ice?7.f:8.f,{.45f,.85f,1,fade*.8f},spin+i,ice?1.6f:1.f);
            }
        }
        hit={.4f,.85f,1,.8f};
    } else if(visual==Visual::Shadow || visual==Visual::Psychic || visual==Visual::Dark) {
        const auto position=age<0?o.mouth:tip;
        const float size=(age<0?charge:fade)*30;
        const bool shadow=visual!=Visual::Psychic;
        sprite(f,Image::Glow,position,size*1.9f,{.35f,.08f,.9f,.6f*fade});
        sprite(f,Image::Glow,position,size,{.035f,.01f,.07f,.95f*fade},0,1,false);
        sprite(f,Image::Vortex,position,size*1.8f,shadow?Color{.5f,.08f,1,fade}:Color{1,.25f,.9f,fade},spin);
        for(int i=0;i<5;++i) {
            const float a=spin+i*pi*.4f;
            sprite(f,Image::Spark,offset(position,std::cos(a)*size*.55f,std::sin(a)*size*.55f),5,{.65f,.3f,1,fade});
        }
        hit={.65f,.15f,1,.9f};
    } else if(visual==Visual::Vine) {
        if(active)for(int side:{-1,1}) {
            Point previous=offset(o.body,float(side)*10,0);
            const float reach=age<.22f?travel:clamp(1-(age-.22f)/.25f);
            for(int i=1;i<=18;++i) {
                const float u=i/18.f;auto p=mix(previous,target,u*reach);
                p=mix(offset(o.body,side*10.f,0),target,u*reach);p[0]+=std::sin(u*pi)*side*24;p[1]+=std::sin(u*pi)*20;
                ribbon(f,previous,p,5,{.08f,.32f,.06f,fade});ribbon(f,previous,p,2,{.45f,.8f,.12f,fade});previous=p;
            }
        }
        hit={.45f,1,.25f,.8f};
    } else if(visual==Visual::Leaf || visual==Visual::Needles || visual==Visual::Stars || visual==Visual::Solar) {
        const bool solar=visual==Visual::Solar;
        if(age<0)sprite(f,Image::Glow,o.mouth,charge*24,{.65f,1,.2f,charge*.6f});
        if(active && age<.4f) {
            if(solar) {ribbon(f,o.mouth,tip,18,{.45f,1,.2f,fade*.3f});ribbon(f,o.mouth,tip,7,{1,1,.6f,fade});}
            for(int i=0;i<10;++i) {
                const float u=clamp(travel-i*.035f);auto p=mix(o.mouth,target,u);
                p=offset(p,std::sin(i*2+spin)*12,std::cos(i*2+spin)*12);
                sprite(f,visual==Visual::Stars?Image::Star:Image::Needle,p,visual==Visual::Stars?13.f:5.f,
                       visual==Visual::Leaf?Color{.3f,.85f,.12f,fade}:Color{1,.95f,.4f,fade},spin+i,visual==Visual::Stars?1.f:3.f);
            }
        }
        hit={.7f,1,.3f,.8f};
    } else if(visual==Visual::Poison || visual==Visual::Smoke) {
        const bool smoke=visual==Visual::Smoke;const auto center=age<0?o.mouth:tip;
        for(int i=0;i<12;++i) {
            const float a=spin+i*2.f,r=(age<0?charge:fade)*18;
            sprite(f,Image::Gas,offset(center,std::cos(a)*r,std::sin(a*1.3f)*r),12+r,
                smoke?Color{.38f,.38f,.42f,fade*.35f}:Color{.6f,.13f,.75f,fade*.45f},a,1,false);
        }
        hit={.7f,.2f,.9f,.5f};
    } else if(visual==Visual::Sound || visual==Visual::Confuse || visual==Visual::Hypnosis || visual==Visual::Tail) {
        if(active)for(int i=0;i<5;++i) {
            const float u=clamp(age*3-i*.14f);if(u<=0)continue;
            sprite(f,visual==Visual::Hypnosis?Image::Vortex:Image::Ring,mix(o.mouth,target,u),12+u*35,
                   visual==Visual::Sound?Color{1,.7f,.25f,fade*.7f}:Color{.9f,.3f,1,fade*.7f},spin+i);
        }
        return;
    } else {
        if(!active)return;
        if(visual==Visual::Dash || visual==Visual::Dark)for(int i=0;i<7;++i)
            ribbon(f,offset(o.body,(i-3)*5.f,3),offset(o.body,(i-3)*5.f,3,-30.f-8.f*i),2,{.8f,.9f,1,fade*.5f});
        if(visual==Visual::Spin || visual==Visual::Wind)sprite(f,Image::Ring,o.body,o.size*1.4f,{.8f,.95f,1,fade*.6f},spin, .35f);
        if(visual==Visual::Slash || visual==Visual::Bite)for(int i=0;i<3;++i)
            ribbon(f,offset(target,-15+i*10.f,20),offset(target,5+i*10.f,-20),3,{1,.9f,.75f,fade});
        if(visual==Visual::Lick)ribbon(f,o.mouth,tip,9,{1,.2f,.45f,fade});
    }
    if(age>=.15f)burst(f,target,age-.15f,hit,seed);
}
void transfer(Frame& f,float p,const Point& pokemon,const Point& ball,float size,bool emerging) {
    if(!std::isfinite(p) || p<0 || p>1)return;
    const float fade=std::sin(pi*p);const Color color=emerging?Color{.6f,.8f,1,fade}:Color{1,.16f,.28f,fade};
    ribbon(f,pokemon,ball,5+fade*5,color);
    sprite(f,Image::Glow,ball,20+fade*35,color);
    for(int i=0;i<22;++i) {
        const float u=std::fmod(p*1.5f+i/22.f,1.f),a=u*4*pi+i;
        auto at=mix(emerging?ball:pokemon,emerging?pokemon:ball,u);
        const float radius=(1-u)*size*.45f;
        at=offset(at,std::cos(a)*radius,std::sin(a)*radius,std::cos(a*1.3f)*radius*.4f);
        sprite(f,Image::Star,at,3+5*(1-u),color,a);
    }
    sprite(f,Image::Flash,pokemon,size*fade*1.25f,color,p*2);
}
} // namespace pokemon_battle::effects
