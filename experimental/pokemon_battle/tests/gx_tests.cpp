#include "../model_lab/gx_renderer.hpp"
#include "../model_lab/field_effects.hpp"
#include "runtime_fixture.hpp"
#include <dolphin/gx/GXExtra.h>
#include <cmath>
#include <iostream>
using fixture::require;
namespace {
std::size_t created=0,destroyed=0,loaded=0,draws=0,vertices=0,expected=0,in_draw=0;
}
extern "C" {
void C_MTXIdentity(Mtx matrix) { for(int r=0;r<3;++r) for(int c=0;c<4;++c) matrix[r][c]=r==c?1.f:0.f; }
void C_MTXMultVec(const Mtx matrix,const Vec* from,Vec* to) {
    const auto point=*from;
    to->x=matrix[0][0]*point.x+matrix[0][1]*point.y+matrix[0][2]*point.z+matrix[0][3];
    to->y=matrix[1][0]*point.x+matrix[1][1]*point.y+matrix[1][2]*point.z+matrix[1][3];
    to->z=matrix[2][0]*point.x+matrix[2][1]*point.y+matrix[2][2]*point.z+matrix[2][3];
}
void C_MTXConcat(const Mtx a,const Mtx b,Mtx out) {
    Mtx result{};
    for(int r=0;r<3;++r) for(int c=0;c<4;++c) {
        for(int k=0;k<3;++k) result[r][c]+=a[r][k]*b[k][c];
        if(c==3) result[r][c]+=a[r][3];
    }
    for(int r=0;r<3;++r) for(int c=0;c<4;++c) out[r][c]=result[r][c];
}
void GXInitTexObj(GXTexObj*,const void* data,u16 w,u16 h,GXTexFmt format,GXTexWrapMode s,GXTexWrapMode t,GXBool mip) {
    require(data && w>=4 && h>=4 && format==GX_TF_RGBA8 && s==GX_CLAMP && t==GX_CLAMP && !mip,"texture setup");++created;
}
void GXInitTexObjLOD(GXTexObj*,GXTexFilter a,GXTexFilter b,f32,f32,f32,GXBool,GXBool,GXAnisotropy) { require(a==GX_LINEAR && b==GX_LINEAR,"filter"); }
void GXInitTexObjWrapMode(GXTexObj*,GXTexWrapMode s,GXTexWrapMode t) {require(s<=GX_MIRROR && t<=GX_MIRROR,"source wrap modes");}
void GXDestroyTexObj(GXTexObj*) { ++destroyed; }
void GXLoadTexObj(GXTexObj*,GXTexMapID) { ++loaded; }
void GXClearVtxDesc() {}
void GXSetVtxDesc(GXAttr,GXAttrType) {}
void GXSetVtxAttrFmt(GXVtxFmt,GXAttr,GXCompCnt,GXCompType,u8) {}
void GXSetNumChans(u8) {}
void GXSetChanCtrl(GXChannelID,GXBool,GXColorSrc,GXColorSrc,u32,GXDiffuseFn,GXAttnFn) {}
void GXSetNumTexGens(u8) {}
void GXSetTexCoordGen2(GXTexCoordID,GXTexGenType,GXTexGenSrc,u32,GXBool,u32) {}
void GXSetNumTevStages(u8) {}
void GXSetNumIndStages(u8) {}
void GXSetTevDirect(GXTevStageID) {}
void GXSetTevOrder(GXTevStageID,GXTexCoordID,GXTexMapID,GXChannelID) {}
void GXSetTevSwapMode(GXTevStageID,GXTevSwapSel,GXTevSwapSel) {}
void GXSetTevSwapModeTable(GXTevSwapSel,GXTevColorChan,GXTevColorChan,GXTevColorChan,GXTevColorChan) {}
void GXSetTevOp(GXTevStageID,GXTevMode) {}
void GXSetBlendMode(GXBlendMode,GXBlendFactor,GXBlendFactor,GXLogicOp) {}
void GXSetAlphaCompare(GXCompare,u8,GXAlphaOp,GXCompare,u8) {}
void GXSetZCompLoc(GXBool) {}
void GXSetZMode(GXBool enabled,GXCompare compare,GXBool) {require(enabled && compare==GX_LEQUAL,"depth setup");}
void GXSetCullMode(GXCullMode) {}
void GXSetColorUpdate(GXBool) {}
void GXSetAlphaUpdate(GXBool) {}
void GXSetFog(GXFogType,f32,f32,f32,f32,GXColor) {}
void GXLoadPosMtxImm(const void*,u32) {}
void GXSetCurrentMtx(u32) {}
void GXBegin(GXPrimitive primitive,GXVtxFmt,u16 count) {
    require(!in_draw && primitive==GX_TRIANGLES && count%3==0,"begin");
    ++draws;expected=count;in_draw=1;
}
void GXPosition3f32(f32 x,f32 y,f32 z) {require(in_draw && expected && std::isfinite(x+y+z),"vertex");--expected;++vertices;}
void GXColor4u8(u8,u8,u8,u8) {}
void GXTexCoord2f32(f32 s,f32 t) {require(std::isfinite(s+t),"texture coordinate");}
void GXEnd() {require(in_draw && expected==0,"end vertex count");in_draw=0;}
}
int main(int argc,char** argv) {
    for(int input=-1;input<argc;++input) {
        auto bytes=input==-1?fixture::skeletal():input==0?fixture::make():fixture::read(argv[input]);
        pokemon_battle::runtime::Model model;std::string error;
        require(model.load(bytes,error),error.c_str());
        const auto nindices=model.indices().size(), nbatches=model.batches().size(), ntextures=model.textures().size();
        const auto before=created;
        {
            pokemon_battle::runtime::GxAsset asset(std::move(model));
            pokemon_battle::runtime::Pose pose;
            Mtx matrix{{1,0,0,0},{0,1,0,0},{0,0,1,0}};
            for(std::size_t c=0;c<asset.model().clips().size();++c) {
                require(asset.model().sample(c,0,false,pose),"sample");
                draws=vertices=loaded=0;asset.draw(pose,matrix);
                require(draws==nbatches && loaded==nbatches && vertices==nindices,"draw coverage");
            }
            pose.vertices.clear();draws=0;asset.draw(pose,matrix);require(draws==0,"invalid pose drawn");
        }
        require(created==before+ntextures && created==destroyed,"texture lifetime");
    }
    Mtx view; C_MTXIdentity(view);
    const cXyz from{20,30,-400};
    draws=vertices=0;
    pokemon_battle::runtime::draw_health(from,0.5f,false,view);
    pokemon_battle::runtime::draw_ball(from,0.3f,view);
    pokemon_battle::effects::Frame frame;
    pokemon_battle::effects::Origins origins;origins.body=origins.mouth=origins.left=origins.right={20,30,-400};
    pokemon_battle::effects::move(frame,pokemon_battle::field::Visual::Electric,.8f,origins,{40,50,-600});
    pokemon_battle::runtime::draw_effects(frame,view);
    require(draws==3 && vertices==12+2592+frame.ribbons.size()*6 && !in_draw,"ball and branching electric geometry, complete submissions");
    for(int i=0;i<=10;++i) {
        draws=vertices=0;pokemon_battle::runtime::draw_ball(from,.3f,view,i/10.f,1.3f,.7f);
        require(draws==1 && vertices==2592 && !in_draw,"hinged shell and latch throughout opening");
    }
    fixture::Bytes effects_bytes{'D','P','F','X','0','0','0','1'};
    fixture::integer(effects_bytes,0);fixture::integer(effects_bytes,0);fixture::integer(effects_bytes,16);
    for(auto name:pokemon_battle::effects::image_names) {
        char label[32]{};std::strcpy(label,name);effects_bytes.insert(effects_bytes.end(),label,label+32);
        for(auto n:{4u,4u,64u})fixture::integer(effects_bytes,n);
        effects_bytes.insert(effects_bytes.end(),64,255);
    }
    fixture::checksum(effects_bytes);std::string effect_error;
    require(pokemon_battle::runtime::load_effects(effects_bytes,effect_error),"effect textures loaded");
    draws=vertices=loaded=0;pokemon_battle::runtime::draw_effects(frame,view);
    require(loaded>0 && vertices==6*(frame.ribbons.size()+frame.sprites.size()) && !in_draw,"every source-textured particle submitted");
    effects_bytes.back()^=1;
    require(!pokemon_battle::runtime::load_effects(effects_bytes,effect_error),"corrupt effect replacement rejected");
    vertices=0;pokemon_battle::runtime::draw_effects(frame,view);
    require(vertices==6*(frame.ribbons.size()+frame.sprites.size()),"failed replacement retains GPU textures");
    pokemon_battle::runtime::release_effects();require(created==destroyed,"effect texture lifetime");
    std::cout<<"PASS: production GX renderer and field effects, texture setup, depth, invalid pose and cleanup (recording host)\n";
}
