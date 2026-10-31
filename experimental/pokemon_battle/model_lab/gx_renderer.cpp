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
void GxAsset::draw(const Pose& pose, const Mtx view_model) {
    if(pose.vertices.size()!=model_.vertex_count() || pose.materials.size()!=model_.batches().size()) return;
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
    for(std::size_t b=0;b<model_.batches().size();++b) {
        const auto& batch=model_.batches()[b]; const auto& material=pose.materials[b];
        GXLoadTexObj(&textures_[batch.texture],GX_TEXMAP0);
        GXBegin(GX_TRIANGLES,GX_VTXFMT0,static_cast<u16>(batch.count));
        for(std::size_t i=batch.first;i<std::size_t(batch.first)+batch.count;++i) {
            const auto& v=pose.vertices[model_.indices()[i]];
            // Stable preview light, deliberately independent of TP's room-light state.
            const float light=0.58f+0.42f*std::max(0.f,v.normal[0]*-0.36f+v.normal[1]*0.80f+v.normal[2]*0.48f);
            const auto color=[&](std::size_t channel){return static_cast<u8>(std::clamp(material.color[channel]*light,0.f,1.f)*255.f+0.5f);};
            GXPosition3f32(v.position[0],v.position[1],v.position[2]);
            GXColor4u8(color(0),color(1),color(2),255);
            const auto& t=material.uv;
            GXTexCoord2f32(t[0]*v.uv[0]+t[1]*v.uv[1]+t[2],
                          1.f-(t[3]*v.uv[0]+t[4]*v.uv[1]+t[5]));
        }
        GXEnd();
    }
}
}
