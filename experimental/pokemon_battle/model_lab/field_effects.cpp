#include "field_effects.hpp"
#include <algorithm>
#include <cmath>

namespace pokemon_battle::runtime {
namespace {
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
void draw_ball(const cXyz& position,float wobble,const Mtx view) {
    Mtx world,matrix; MTXIdentity(world); world[0][3]=position.x; world[1][3]=position.y; world[2][3]=position.z;
    MTXConcat(view,world,matrix); setup(matrix);
    constexpr int rings=10,sectors=16;
    constexpr float pi=3.14159265358979323846f;
    GXBegin(GX_TRIANGLES,GX_VTXFMT0,rings*sectors*6);
    for(int r=0;r<rings;++r) for(int s=0;s<sectors;++s) {
        const auto point=[&](int ring,int sector) {
            const float phi=pi*float(ring)/rings,theta=2*pi*float(sector)/sectors;
            const float x=8*std::sin(phi)*std::cos(theta),y=8*std::cos(phi),z=8*std::sin(phi)*std::sin(theta);
            const GXColor color=(r==4 || r==5)?GXColor{24,26,32,255}:(r<4?GXColor{240,35,50,255}:GXColor{240,240,235,255});
            vertex(x*std::cos(wobble)-y*std::sin(wobble),x*std::sin(wobble)+y*std::cos(wobble),z,color);
        };
        point(r,s); point(r+1,s); point(r+1,s+1); point(r,s); point(r+1,s+1); point(r,s+1);
    }
    GXEnd();
}
void draw_spark(const cXyz& start,const cXyz& end,const Mtx view) {
    setup(view); GXBegin(GX_TRIANGLES,GX_VTXFMT0,24);
    for(int i=0;i<4;++i) {
        const float a=float(i)/4,b=float(i+1)/4;
        const float ax=start.x+(end.x-start.x)*a,ay=start.y+(end.y-start.y)*a+(i%2?9:-9),az=start.z+(end.z-start.z)*a;
        const float bx=start.x+(end.x-start.x)*b,by=start.y+(end.y-start.y)*b+(i%2?-9:9),bz=start.z+(end.z-start.z)*b;
        constexpr GXColor color{255,235,55,255};
        vertex(ax,ay-2,az,color); vertex(bx,by-2,bz,color); vertex(bx,by+2,bz,color);
        vertex(ax,ay-2,az,color); vertex(bx,by+2,bz,color); vertex(ax,ay+2,az,color);
    }
    GXEnd();
}
}
