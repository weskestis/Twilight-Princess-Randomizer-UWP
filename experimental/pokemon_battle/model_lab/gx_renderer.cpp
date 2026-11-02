#include "gx_renderer.hpp"
#include <dolphin/gx/GXExtra.h>
#include <algorithm>
#include <cmath>
#include <utility>

namespace pokemon_battle::runtime {
GxAsset::GxAsset(Model model) : model_(std::move(model)), textures_(model_.textures().size()) {
    for(std::size_t i=0;i<textures_.size();++i) {
        const auto& t=model_.textures()[i];
        GXInitTexObj(&textures_[i],model_.texture_data(i).data(),t.width,t.height,
                     GX_TF_RGBA8,GX_CLAMP,GX_CLAMP,GX_FALSE);
        GXInitTexObjLOD(&textures_[i],GX_LINEAR,GX_LINEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);
    }
}
GxAsset::~GxAsset() { for(auto& texture:textures_) GXDestroyTexObj(&texture); }
void GxAsset::draw(const Pose& pose, const Mtx view_model,float opacity) {
    if(pose.vertices.size()!=model_.vertex_count() || pose.materials.size()!=model_.batches().size()) return;
    opacity=std::clamp(opacity,0.f,1.f);
    if(opacity<=.001f)return;
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS,GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0,GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0,GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);
    GXSetVtxAttrFmt(GX_VTXFMT0,GX_VA_CLR0,GX_CLR_RGBA,GX_RGBA8,0);
    GXSetVtxAttrFmt(GX_VTXFMT0,GX_VA_TEX0,GX_TEX_ST,GX_F32,0);
    GXSetNumChans(1);
    GXSetChanCtrl(GX_COLOR0A0,GX_DISABLE,GX_SRC_REG,GX_SRC_VTX,GX_LIGHT_NULL,GX_DF_NONE,GX_AF_NONE);
    GXSetNumTexGens(1);
    GXSetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX2x4,GX_TG_TEX0,GX_IDENTITY);
    GXSetNumTevStages(1);
    GXSetNumIndStages(0);
    GXSetTevDirect(GX_TEVSTAGE0);
    GXSetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD0,GX_TEXMAP0,GX_COLOR0A0);
    GXSetTevSwapMode(GX_TEVSTAGE0,GX_TEV_SWAP0,GX_TEV_SWAP0);
    GXSetTevSwapModeTable(GX_TEV_SWAP0,GX_CH_RED,GX_CH_GREEN,GX_CH_BLUE,GX_CH_ALPHA);
    GXSetTevOp(GX_TEVSTAGE0,GX_MODULATE);
    GXSetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
    GXSetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
    GXSetZCompLoc(GX_TRUE);
    GXSetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE);
    GXSetCullMode(GX_CULL_NONE);
    GXSetColorUpdate(GX_TRUE);
    GXSetAlphaUpdate(GX_FALSE);
    GXSetFog(GX_FOG_NONE,0,1,0,1,GXColor{0,0,0,0});
    GXLoadPosMtxImm(view_model,GX_PNMTX0);
    GXSetCurrentMtx(GX_PNMTX0);
    // Opaque surfaces establish depth before the source's transparent gas,
    // fins and membranes. Translucent surfaces test depth without writing it.
    for(int pass=0;pass<2;++pass) for(std::size_t b=0;b<model_.batches().size();++b) {
        const auto& batch=model_.batches()[b]; const auto& material=pose.materials[b];
        const bool transparent=(batch.flags&1)!=0 || material.color[3]<1.f || opacity<1.f;
        if(transparent!=(pass==1)) continue;
        GXSetBlendMode(transparent?GX_BM_BLEND:GX_BM_NONE,transparent?GX_BL_SRCALPHA:GX_BL_ONE,
                       transparent?GX_BL_INVSRCALPHA:GX_BL_ZERO,GX_LO_COPY);
        GXSetZMode(GX_TRUE,GX_LEQUAL,transparent?GX_FALSE:GX_TRUE);
        GXSetAlphaCompare(GX_GREATER,1,GX_AOP_AND,GX_ALWAYS,0);
        GXSetZCompLoc(GX_FALSE);
        GXInitTexObjWrapMode(&textures_[batch.texture],
            (batch.flags&32)?GX_MIRROR:(batch.flags&2)?GX_REPEAT:GX_CLAMP,
            (batch.flags&64)?GX_MIRROR:(batch.flags&4)?GX_REPEAT:GX_CLAMP);
        GXLoadTexObj(&textures_[batch.texture],GX_TEXMAP0);
        GXBegin(GX_TRIANGLES,GX_VTXFMT0,static_cast<u16>(batch.count));
        for(std::size_t i=batch.first;i<std::size_t(batch.first)+batch.count;++i) {
            const auto& v=pose.vertices[model_.indices()[i]];
            // Stable preview light, deliberately independent of TP's room-light state.
            const float light=(batch.flags&8)?1.f:0.58f+0.42f*std::max(0.f,v.normal[0]*-0.36f+v.normal[1]*0.80f+v.normal[2]*0.48f);
            const auto color=[&](std::size_t channel){return static_cast<u8>(std::clamp(material.color[channel]*v.color[channel]*(channel==3?opacity:light),0.f,1.f)*255.f+0.5f);};
            GXPosition3f32(v.position[0],v.position[1],v.position[2]);
            GXColor4u8(color(0),color(1),color(2),color(3));
            const auto& t=material.uv;
            if(batch.flags&16) {
                // Source reflection texgen (Umbreon's rings), in view space.
                float nx=0,ny=0,length=0;
                for(int axis=0;axis<3;++axis) {
                    float n=0;for(int k=0;k<3;++k)n+=view_model[axis][k]*v.normal[k];
                    length+=n*n;if(axis==0)nx=n;if(axis==1)ny=n;
                }
                const float inverse=length>1e-10f?0.5f/std::sqrt(length):0.f;
                GXTexCoord2f32(0.5f+nx*inverse,0.5f-ny*inverse);
            } else GXTexCoord2f32(t[0]*v.uv[0]+t[1]*v.uv[1]+t[2],
                                  1.f-(t[3]*v.uv[0]+t[4]*v.uv[1]+t[5]));
        }
        GXEnd();
    }
}
}
