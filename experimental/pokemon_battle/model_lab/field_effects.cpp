#include "field_effects.hpp"
#include <algorithm>
#include <cmath>
#include <dolphin/gx/GXExtra.h>

namespace pokemon_battle::runtime {
namespace {
effects::Pack effect_pack;
std::vector<GXTexObj> effect_textures;
void setup(const Mtx matrix) {
    GXClearVtxDesc(); GXSetVtxDesc(GX_VA_POS,GX_DIRECT); GXSetVtxDesc(GX_VA_CLR0,GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);
    GXSetVtxAttrFmt(GX_VTXFMT0,GX_VA_CLR0,GX_CLR_RGBA,GX_RGBA8,0);
    GXSetNumChans(1); GXSetChanCtrl(GX_COLOR0A0,GX_DISABLE,GX_SRC_REG,GX_SRC_VTX,GX_LIGHT_NULL,GX_DF_NONE,GX_AF_NONE);
    GXSetNumTexGens(0); GXSetNumIndStages(0); GXSetNumTevStages(1); GXSetTevDirect(GX_TEVSTAGE0);
    GXSetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD_NULL,GX_TEXMAP_NULL,GX_COLOR0A0);
    GXSetTevOp(GX_TEVSTAGE0,GX_PASSCLR); GXSetTevSwapMode(GX_TEVSTAGE0,GX_TEV_SWAP0,GX_TEV_SWAP0);
    GXSetTevSwapModeTable(GX_TEV_SWAP0,GX_CH_RED,GX_CH_GREEN,GX_CH_BLUE,GX_CH_ALPHA);
    GXSetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
    GXSetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0); GXSetZCompLoc(GX_TRUE);
    GXSetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE); GXSetCullMode(GX_CULL_NONE);
    GXSetColorUpdate(GX_TRUE); GXSetAlphaUpdate(GX_FALSE); GXSetFog(GX_FOG_NONE,0,1,0,1,GXColor{0,0,0,0});
    GXLoadPosMtxImm(matrix,GX_PNMTX0); GXSetCurrentMtx(GX_PNMTX0);
}
void vertex(float x,float y,float z,GXColor color) { GXPosition3f32(x,y,z); GXColor4u8(color.r,color.g,color.b,color.a); }
void quad(float left,float right,float bottom,float top,float z,GXColor color) {
    vertex(left,bottom,z,color); vertex(right,bottom,z,color); vertex(right,top,z,color);
    vertex(left,bottom,z,color); vertex(right,top,z,color); vertex(left,top,z,color);
}
}
void draw_health(const cXyz& position,float fraction,bool companion,const Mtx view) {
    Vec screen{}; MTXMultVec(view,&position,&screen);
    Mtx identity; MTXIdentity(identity); setup(identity);
    GXBegin(GX_TRIANGLES,GX_VTXFMT0,12);
    quad(screen.x-27,screen.x+27,screen.y-3,screen.y+5,screen.z,{18,20,24,255});
    quad(screen.x-25,screen.x-25+50*std::clamp(fraction,0.f,1.f),screen.y-1,screen.y+3,screen.z+0.1f,
         companion?GXColor{70,220,160,255}:GXColor{235,90,75,255});
    GXEnd();
}
void draw_ball(const cXyz& position,float wobble,const Mtx view,float opening,float spin,float glow) {
    Mtx world,matrix; MTXIdentity(world); world[0][3]=position.x; world[1][3]=position.y; world[2][3]=position.z;
    MTXConcat(view,world,matrix); setup(matrix);
    constexpr int rings=16,sectors=24;
    constexpr float pi=3.14159265358979323846f;
    const float hinge=std::clamp(opening,0.f,1.f)*pi*.72f;
    const auto draw=[&](float x,float y,float z,GXColor color,bool lid) {
        if(lid) {z+=8;const float ny=y*std::cos(hinge)+z*std::sin(hinge);z=-y*std::sin(hinge)+z*std::cos(hinge)-8;y=ny;}
        const float sx=x*std::cos(spin)+z*std::sin(spin);z=-x*std::sin(spin)+z*std::cos(spin);x=sx;
        vertex(x*std::cos(wobble)-y*std::sin(wobble),x*std::sin(wobble)+y*std::cos(wobble),z,color);
    };
    GXBegin(GX_TRIANGLES,GX_VTXFMT0,rings*sectors*6+sectors*6+sectors*6);
    for(int r=0;r<rings;++r) for(int s=0;s<sectors;++s) {
        const auto point=[&](int ring,int sector) {
            const float phi=pi*float(ring)/rings,theta=2*pi*float(sector)/sectors;
            const float x=8*std::sin(phi)*std::cos(theta),y=8*std::cos(phi),z=8*std::sin(phi)*std::sin(theta);
            const float light=.65f+.35f*std::max(0.f,(-x+y*2+z)/20);
            GXColor color=r<rings/2?GXColor{240,30,48,255}:GXColor{245,242,228,255};
            if(std::abs(y)<.45f)color={28,30,38,255};
            color.r=static_cast<u8>(color.r*light);color.g=static_cast<u8>(color.g*light);color.b=static_cast<u8>(color.b*light);
            draw(x,y,z,color,r<rings/2);
        };
        point(r,s); point(r+1,s); point(r+1,s+1); point(r,s); point(r+1,s+1); point(r,s+1);
    }
    // Both shell interiors and the separate circular latch remain visible
    // throughout the hinge animation, rather than stretching a closed sphere.
    for(int s=0;s<sectors;++s) {
        const float a=2*pi*s/sectors,b=2*pi*(s+1)/sectors;
        for(bool lid:{false,true}) {
            draw(0,0,0,{26,29,38,255},lid);
            draw(8*std::cos(a),0,8*std::sin(a),{26,29,38,255},lid);
            draw(8*std::cos(b),0,8*std::sin(b),{26,29,38,255},lid);
        }
        for(int layer=0;layer<2;++layer) {
            const float radius=layer==0?2.4f:1.5f,z=8.05f+layer*.12f;
            const GXColor color=layer==0?GXColor{18,22,28,255}:GXColor{static_cast<u8>(245-120*glow),255,static_cast<u8>(240-70*glow),255};
            draw(0,-.5f,z,color,false);
            draw(std::cos(a)*radius,std::sin(a)*radius-.5f,z,color,false);
            draw(std::cos(b)*radius,std::sin(b)*radius-.5f,z,color,false);
        }
    }
    GXEnd();
}
bool load_effects(std::span<const std::uint8_t> bytes,std::string& error) {
    effects::Pack next;if(!next.load(bytes,error))return false;
    release_effects();effect_pack=std::move(next);effect_textures.resize(effect_pack.textures().size());
    for(std::size_t i=0;i<effect_textures.size();++i) {
        const auto& t=effect_pack.textures()[i];
        GXInitTexObj(&effect_textures[i],effect_pack.pixels(i).data(),t.width,t.height,GX_TF_RGBA8,GX_CLAMP,GX_CLAMP,GX_FALSE);
        GXInitTexObjLOD(&effect_textures[i],GX_LINEAR,GX_LINEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);
    }
    return true;
}
void release_effects() {
    for(auto& texture:effect_textures)GXDestroyTexObj(&texture);
    effect_textures.clear();effect_pack=effects::Pack{};
}
void draw_effects(const effects::Frame& frame,const Mtx view) {
    Mtx identity;MTXIdentity(identity);setup(identity);
    GXSetZMode(GX_TRUE,GX_LEQUAL,GX_FALSE);GXSetBlendMode(GX_BM_BLEND,GX_BL_SRCALPHA,GX_BL_ONE,GX_LO_COPY);
    const auto project=[&](const effects::Point& point) {const Vec v{point[0],point[1],point[2]};Vec out{};MTXMultVec(view,&v,&out);return out;};
    const auto color=[](const effects::Color& c){return GXColor{static_cast<u8>(std::clamp(c[0],0.f,1.f)*255),static_cast<u8>(std::clamp(c[1],0.f,1.f)*255),static_cast<u8>(std::clamp(c[2],0.f,1.f)*255),static_cast<u8>(std::clamp(c[3],0.f,1.f)*255)};};
    if(!frame.ribbons.empty()) {
        GXBegin(GX_TRIANGLES,GX_VTXFMT0,static_cast<u16>(frame.ribbons.size()*6));
        for(const auto& r:frame.ribbons) {
            const auto a=project(r.from),b=project(r.to);const auto c=color(r.color);
            const float dx=b.x-a.x,dy=b.y-a.y,length=std::sqrt(dx*dx+dy*dy);
            const float x=length>.001f?-dy/length*r.width*.5f:r.width*.5f,y=length>.001f?dx/length*r.width*.5f:0;
            vertex(a.x-x,a.y-y,a.z,c);vertex(b.x-x,b.y-y,b.z,c);vertex(b.x+x,b.y+y,b.z,c);
            vertex(a.x-x,a.y-y,a.z,c);vertex(b.x+x,b.y+y,b.z,c);vertex(a.x+x,a.y+y,a.z,c);
        }
        GXEnd();
    }
    if(effect_textures.empty())return;
    GXSetVtxDesc(GX_VA_TEX0,GX_DIRECT);GXSetVtxAttrFmt(GX_VTXFMT0,GX_VA_TEX0,GX_TEX_ST,GX_F32,0);
    GXSetNumTexGens(1);GXSetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX2x4,GX_TG_TEX0,GX_IDENTITY);
    GXSetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD0,GX_TEXMAP0,GX_COLOR0A0);GXSetTevOp(GX_TEVSTAGE0,GX_MODULATE);
    for(bool additive:{false,true})for(std::size_t texture=0;texture<effect_textures.size();++texture) {
        std::size_t count=0;for(const auto& s:frame.sprites)if(std::size_t(s.image)==texture && s.additive==additive)++count;
        if(!count)continue;
        GXSetBlendMode(GX_BM_BLEND,GX_BL_SRCALPHA,additive?GX_BL_ONE:GX_BL_INVSRCALPHA,GX_LO_COPY);
        GXLoadTexObj(&effect_textures[texture],GX_TEXMAP0);
        GXBegin(GX_TRIANGLES,GX_VTXFMT0,static_cast<u16>(count*6));
        for(const auto& s:frame.sprites)if(std::size_t(s.image)==texture && s.additive==additive) {
            const auto center=project(s.position);const auto c=color(s.color);const float cosine=std::cos(s.rotation),sine=std::sin(s.rotation);
            const auto corner=[&](float u,float v) {
                const float x=(u-.5f)*s.width,y=(.5f-v)*s.height;
                vertex(center.x+x*cosine-y*sine,center.y+x*sine+y*cosine,center.z,c);GXTexCoord2f32(u,v);
            };
            corner(0,0);corner(0,1);corner(1,1);corner(0,0);corner(1,1);corner(1,0);
        }
        GXEnd();
    }
}
}
