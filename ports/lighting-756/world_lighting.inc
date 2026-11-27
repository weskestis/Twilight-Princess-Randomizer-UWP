// Included in the Xbox world painter so both launch modes use the same render pass.
#include "d/d_bg_s_wtr_chk.h"
#include "d/d_bg_s_lin_chk.h"
#include "SSystem/SComponent/c_m3d_g_pla.h"
#include "d/d_kankyo.h"
#include <aurora/gfx.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>

namespace dusk::world_lighting {
using Vec = std::array<float, 4>;
struct Lamp { Vec positionRadius, color; };
struct Bounce { Vec positionRadius, color, normal; };
struct alignas(16) Frame {
    Vec projection, depth, upEye, sun, fog, sky, amounts, atmosphere;
    Vec worldX, worldZ, grid, quality;
    std::array<Lamp,8> lamps;
    std::array<Vec,4> water;
    std::array<Bounce,8> bounces;
};
static_assert(sizeof(Frame) == 896);
struct Payload {
    WGPUTextureView scene, depth, light;
    aurora::gfx::Range uniform;
};
static_assert(sizeof(Payload) <= aurora::gfx::InlineDrawPayloadSize);

constexpr char shaderSource[] =
#include "world_lighting_shader.inc"
;
struct Slot { uint64_t key = 0; wgpu::RenderPipeline pipeline; };
struct Pipelines {
    wgpu::Device device;
    wgpu::ShaderModule shader;
    wgpu::BindGroupLayout bindings;
    wgpu::PipelineLayout layout;
    Slot gather, composite;
};

// Only touched by the render worker. All borrowed texture handles in Payload
// belong to Aurora's frame packet and survive until it finishes replay.
static bool prepare(Pipelines& gpu, const wgpu::Device& device) {
    if (gpu.device.Get() == device.Get() && gpu.layout) return true;
    gpu = {};
    gpu.device = device;
    WGPUShaderSourceWGSL code = WGPU_SHADER_SOURCE_WGSL_INIT;
    code.code = {shaderSource, sizeof(shaderSource)-1};
    WGPUShaderModuleDescriptor module = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    module.nextInChain = &code.chain;
    gpu.shader = wgpu::ShaderModule::Acquire(wgpuDeviceCreateShaderModule(device.Get(), &module));
    if (!gpu.shader) return false;
    WGPUBindGroupLayoutEntry entries[] = {WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT,
        WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT, WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT, WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT};
    for (unsigned i=0;i<4;++i) {
        auto& e = entries[i]; e.binding=i; e.visibility=WGPUShaderStage_Fragment;
        if (i == 0) {
            e.buffer.type=WGPUBufferBindingType_Uniform;
            e.buffer.minBindingSize=sizeof(Frame);
        } else {
            e.texture.sampleType=WGPUTextureSampleType_UnfilterableFloat;
            e.texture.viewDimension=WGPUTextureViewDimension_2D;
        }
    }
    WGPUBindGroupLayoutDescriptor bind=WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    bind.entries=entries; bind.entryCount=4;
    gpu.bindings=wgpu::BindGroupLayout::Acquire(wgpuDeviceCreateBindGroupLayout(device.Get(),&bind));
    if (!gpu.bindings) return false;
    const auto raw=gpu.bindings.Get();
    WGPUPipelineLayoutDescriptor pl=WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    pl.bindGroupLayouts=&raw; pl.bindGroupLayoutCount=1;
    gpu.layout=wgpu::PipelineLayout::Acquire(wgpuDeviceCreatePipelineLayout(device.Get(),&pl));
    return bool(gpu.layout);
}

static bool pipelineFor(Pipelines& gpu, Slot& slot, const aurora::gfx::RenderTargetLayout& rt, bool composite) {
    if (slot.pipeline && slot.key==rt.key) return true;
    WGPUColorTargetState colors[aurora::gfx::MaxColorAttachments]{};
    if (!rt.colorAttachmentCount || rt.colorAttachmentCount>aurora::gfx::MaxColorAttachments) return false;
    for (uint32_t i=0;i<rt.colorAttachmentCount;++i) {
        colors[i] = WGPU_COLOR_TARGET_STATE_INIT;
        colors[i].format=static_cast<WGPUTextureFormat>(rt.colorAttachments[i].format);
        colors[i].writeMask=i ? WGPUColorWriteMask_None : (composite
            ? WGPUColorWriteMask_Red|WGPUColorWriteMask_Green|WGPUColorWriteMask_Blue : WGPUColorWriteMask_All);
    }
    WGPUDepthStencilState depth=WGPU_DEPTH_STENCIL_STATE_INIT;
    depth.format=static_cast<WGPUTextureFormat>(rt.depthStencilFormat);
    depth.depthWriteEnabled=WGPUOptionalBool_False;
    depth.depthCompare=WGPUCompareFunction_Always;
    WGPUFragmentState fs=WGPU_FRAGMENT_STATE_INIT;
    fs.module=gpu.shader.Get(); fs.entryPoint={composite ? "composite" : "gather",WGPU_STRLEN};
    fs.targets=colors; fs.targetCount=rt.colorAttachmentCount;
    WGPURenderPipelineDescriptor desc=WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    desc.label={composite ? "World Lighting Composite" : "World Lighting Gather", WGPU_STRLEN};
    desc.layout=gpu.layout.Get(); desc.vertex.module=gpu.shader.Get(); desc.vertex.entryPoint={"vs",2};
    desc.fragment=&fs; desc.primitive.topology=WGPUPrimitiveTopology_TriangleList;
    desc.multisample.count=rt.sampleCount;
    if (depth.format!=WGPUTextureFormat_Undefined) desc.depthStencil=&depth;
    slot.pipeline=wgpu::RenderPipeline::Acquire(wgpuDeviceCreateRenderPipeline(gpu.device.Get(),&desc));
    slot.key=rt.key;
    return bool(slot.pipeline);
}

template<bool Composite>
static void draw(const aurora::gfx::DrawContext& ctx,const wgpu::RenderPassEncoder& pass,
                 const void* bytes,size_t size,void*) {
    if (size!=sizeof(Payload) || !ctx.uniformBuffer) return;
    static Pipelines gpu;
    if (!prepare(gpu, ctx.device)) return;
    auto& slot=Composite ? gpu.composite : gpu.gather;
    if (!pipelineFor(gpu,slot,ctx.layout,Composite)) return;
    Payload p{};
    std::memcpy(&p,bytes,sizeof(p));
    WGPUBindGroupEntry entries[] = {WGPU_BIND_GROUP_ENTRY_INIT,WGPU_BIND_GROUP_ENTRY_INIT,
                                    WGPU_BIND_GROUP_ENTRY_INIT,WGPU_BIND_GROUP_ENTRY_INIT};
    entries[0].binding=0; entries[0].buffer=ctx.uniformBuffer.Get();
    entries[0].offset=p.uniform.offset; entries[0].size=sizeof(Frame);
    entries[1].binding=1; entries[1].textureView=p.scene;
    entries[2].binding=2; entries[2].textureView=p.depth;
    entries[3].binding=3; entries[3].textureView=p.light;
    WGPUBindGroupDescriptor desc=WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    desc.layout=gpu.bindings.Get(); desc.entries=entries; desc.entryCount=4;
    auto group=wgpu::BindGroup::Acquire(wgpuDeviceCreateBindGroup(ctx.device.Get(),&desc));
    if (!group) return;
    wgpuRenderPassEncoderSetPipeline(pass.Get(),slot.pipeline.Get());
    wgpuRenderPassEncoderSetBindGroup(pass.Get(),0,group.Get(),0,nullptr);
    wgpuRenderPassEncoderDraw(pass.Get(),3,1,0,0);
}

static Vec inView(const view_class& v,const cXyz& p,float radius) {
    Vec result{0,0,0,radius};
    for(int i=0;i<3;++i)
        result[i]=v.viewMtx[i][0]*p.x+v.viewMtx[i][1]*p.y+v.viewMtx[i][2]*p.z+v.viewMtx[i][3];
    return result;
}
static Vec rgb(const GXColorS10& c,float w) {
    auto channel=[](int value){return std::clamp(value/255.f,0.f,1.f);};
    return {channel(c.r),channel(c.g),channel(c.b),w};
}

struct SelectedLamp { const void* id=nullptr; cXyz position; };
struct SurfaceHit { cXyz position,normal; float gain=0.f; };
struct Probe {
    const void* id=nullptr;
    cXyz origin;
    float radius=0.f;
    uint32_t tick=0;
    std::array<SurfaceHit,6> hits{};
};

// Trace one emitter per rendered frame against real world collision. A hit becomes
// a diffuse secondary source on the lit side of the surface, even off camera.
static void traceProbe(Probe& probe,const SelectedLamp& source,float radius,uint32_t tick) {
    static constexpr int axes[6][3]={{0,-1,0},{1,0,0},{-1,0,0},{0,0,1},{0,0,-1},{0,1,0}};
    probe.id=source.id; probe.origin=source.position; probe.radius=radius; probe.tick=tick;
    for (int i=0;i<6;++i) {
        auto& hit=probe.hits[i];
        hit.gain=0.f;
        const cXyz dir{float(axes[i][0]),float(axes[i][1]),float(axes[i][2])};
        const cXyz end=source.position+dir*radius;
        dBgS_LinChk query;
        query.Set(&source.position,&end,nullptr);
        cM3dGPla plane;
        if (!dComIfG_Bgsp().LineCross(&query) || !dComIfG_Bgsp().GetTriPla(query,&plane)) continue;
        cXyz n=plane.mNormal;
        const float len=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
        if (!(len>.001f)) continue;
        n=n*(1.f/len);
        const float facing=-(n.x*dir.x+n.y*dir.y+n.z*dir.z);
        if (facing<.05f) continue;
        const cXyz contact=query.GetCross();
        const float travel=contact.abs(source.position);
        const float incoming=std::max(0.f,1.f-travel/radius);
        hit.position=contact+n*4.f;
        hit.normal=n;
        hit.gain=incoming*incoming*facing;
    }
}

static void collectBounces(Frame& f,const view_class& v,const std::array<SelectedLamp,8>& sources) {
    static std::array<Probe,8> cache{};
    static std::string stage;
    static int previousRoom=-999;
    static unsigned cursor=0;
    const char* current=dComIfGp_getStartStageName();
    const int room=dComIfGp_roomControl_getStayNo();
    if (!current || room<0) return;
    if (previousRoom!=room || stage!=current) {
        cache={}; cursor=0; stage=current; previousRoom=room;
    }
    const unsigned count=static_cast<unsigned>(f.grid[3]);
    if (!count) return;
    const uint32_t tick=aurora::gfx::current_frame();
    bool traced=false;
    for (unsigned step=0;step<count;++step) {
        const unsigned i=(cursor+step)%count;
        auto& probe=cache[i];
        const auto& source=sources[i];
        const float radius=f.lamps[i].positionRadius[3];
        if (probe.id!=source.id || probe.origin.abs2(source.position)>32.f*32.f ||
            std::abs(radius-probe.radius)>32.f) probe.id=nullptr;
        if ((!probe.id || tick-probe.tick>=24) && !traced) {
            traceProbe(probe,source,radius,tick); traced=true;
        }
    }
    cursor=(cursor+1)%count;
    const Vec tint=rgb(g_env_light.bg_amb_col[0],0);
    std::array<float,8> scores{};
    for (unsigned i=0;i<count;++i) {
        if (!cache[i].id) continue;
        for (const auto& hit:cache[i].hits) {
            if (!(hit.gain>.01f)) continue;
            const Vec pos=inView(v,hit.position,cache[i].radius*.85f);
            const float distanceSquared=pos[0]*pos[0]+pos[1]*pos[1]+pos[2]*pos[2];
            const float score=hit.gain*pos[3]*pos[3]/(1600.f+distanceSquared);
            const auto slot=std::min_element(scores.begin(),scores.end())-scores.begin();
            if (score<=scores[slot]) continue;
            scores[slot]=score;
            auto& bounce=f.bounces[slot];
            bounce.positionRadius=pos;
            for (int j=0;j<3;++j) {
                bounce.normal[j]=v.viewMtx[j][0]*hit.normal.x+v.viewMtx[j][1]*hit.normal.y+
                                 v.viewMtx[j][2]*hit.normal.z;
                bounce.color[j]=hit.gain*f.lamps[i].color[j]*(.45f+.55f*tint[j]);
            }
        }
    }
}

static void collectLamps(Frame& f,const view_class& v) {
    std::array<float,8> ranks{};
    std::array<SelectedLamp,8> sources{};
    unsigned count=0;
    auto add=[&](const void* id,const cXyz& pos,float radius,Vec color) {
        if (!(radius>1.f) || !(std::max({color[0],color[1],color[2]})>0.f)) return;
        auto point=inView(v,pos,std::min(radius,2400.f));
        const float d2=point[0]*point[0]+point[1]*point[1]+point[2]*point[2];
        const float rank=point[3]*point[3]/(400.f+d2)*std::max({color[0],color[1],color[2]});
        if (!std::isfinite(rank) || rank<=0.f) return;
        auto idx=static_cast<unsigned>(std::min_element(ranks.begin(),ranks.end())-ranks.begin());
        if (rank<=ranks[idx]) return;
        sources[idx]={id,pos}; ranks[idx]=rank; f.lamps[idx]={point,color}; count=std::min(count+1,8u);
    };
    for (auto* light:g_env_light.pointlight)
        if (light) add(light,light->mPosition,light->mPow,rgb(light->mColor,0));
    for (auto* light:g_env_light.efplight)
        if (light) add(light,light->mPosition,light->mPow,rgb(light->mColor,0));
    if (auto* player=dComIfGp_getLinkPlayer()) {
        if (auto* flame=player->getKandelaarFlamePos()) add(flame,*flame,300.f,{1.f,.46f,.14f,0});
    }
    f.grid[3]=static_cast<float>(count);
    if (f.amounts[0]>0.f) collectBounces(f,v,sources);
}

// Sample native water collision using our own query object. Never overwrite the
// shared actor water-check state. A stage or room change invalidates all samples.
static void collectWater(Frame& f,const view_class& v) {
    constexpr float spacing=640.f, missing=-10000000.f;
    const auto& eye=v.lookat.eye;
    const float x=std::floor(eye.x/spacing-1.f)*spacing;
    const float z=std::floor(eye.z/spacing-1.f)*spacing;
    const float band=std::floor(eye.y/256.f)*256.f;
    const int room=dComIfGp_roomControl_getStayNo();
    const uint32_t tick=aurora::gfx::current_frame();
    static struct { std::array<Vec,4> water; std::string stage; int room=-999;
        float x=0,z=0,y=0,height=0; uint32_t tick=0; } cache;
    const char* stage=dComIfGp_getStartStageName();
    if (!stage) return;
    if (cache.stage!=stage || cache.room!=room || cache.x!=x || cache.z!=z ||
        cache.y!=band || tick-cache.tick>=12) {
        cache.stage=stage; cache.room=room; cache.x=x; cache.z=z; cache.y=band; cache.tick=tick;
        float closest=1e20f; cache.height=missing;
        for (int j=0;j<4;++j) for (int i=0;i<4;++i) {
            cXyz point(x+i*spacing,band-1600.f,z+j*spacing);
            dBgS_WtrChk check;
            check.Set(point,eye.y+48.f);
            float h=missing;
            if (dComIfG_Bgsp().WaterChk(&check) && dComIfG_Bgsp().GetPolyAtt0(check)!=6)
                h=check.GetHeight();
            cache.water[j][i]=h;
            const float dist=(point.x-eye.x)*(point.x-eye.x)+(point.z-eye.z)*(point.z-eye.z);
            if (h>missing && dist<closest) { closest=dist; cache.height=h; }
        }
    }
    f.water=cache.water; f.grid[0]=x; f.grid[1]=z; f.grid[2]=spacing;
    f.quality[2]=cache.height>missing ? 1.f:0.f; f.quality[3]=cache.height;
}

static void record(const view_class& v) {
    const auto& settings=getSettings().game;
    if (!settings.worldLighting.getValue() || !(v.near_>0.f && v.far_>v.near_) ||
        aurora::gfx::is_offscreen()) return;
    Frame f{};
    auto strength=[](const auto& value){return .01f*std::clamp(value.getValue(),0,100);};
    f.amounts={strength(settings.worldBounce),strength(settings.worldLamps),
               strength(settings.worldReflections),strength(settings.worldFilmic)};
    f.atmosphere={strength(settings.worldFog),0,strength(settings.worldShafts),strength(settings.worldShadows)};
    if (f.amounts==Vec{} && f.atmosphere==Vec{}) return;
    const auto& p=v.projMtx;
    if (!(p[0][0]>0 && p[1][1]>0) || p[3][2]!=-1.f) return;
    f.projection={p[0][0],p[1][1],p[0][2],p[1][2]};
    const bool reversed=aurora::gfx::uses_reversed_z();
    f.depth={reversed ? -p[2][2] : p[2][2]-1.f,reversed ? -p[2][3] : p[2][3],v.near_,v.far_};
    for(int i=0;i<4;++i) {
        f.worldX[i]=v.invViewMtx[0][i]; f.upEye[i]=v.invViewMtx[1][i]; f.worldZ[i]=v.invViewMtx[2][i];
    }
    f.fog=rgb(g_env_light.fog_col,std::max(g_env_light.mFogNear,0.f));
    f.atmosphere[1]=1.f/std::max(g_env_light.mFogFar-f.fog[3],400.f);
    f.sky=rgb(g_env_light.vrbox_sky_col,std::clamp(g_env_light.raincnt/120.f,0.f,1.f));
    if (const auto* sun=g_env_light.mpSunPacket) {
        f.sun=inView(v,g_env_light.sun_light_pos,std::clamp(sun->mSunAlpha,0.f,1.f));
        const float length=std::sqrt(f.sun[0]*f.sun[0]+f.sun[1]*f.sun[1]+f.sun[2]*f.sun[2]);
        if (length>1.f) for(unsigned i=0;i<3;++i) f.sun[i]/=length;
        else f.sun={};
    }
    const int limit=std::clamp(settings.worldLightingHeight.getValue(),180,1080);
    f.quality={limit<=540 ? 4.f:8.f,limit<=540 ? 10.f:16.f,0,0};
    if (f.amounts[0]>0.f || f.amounts[1]>0.f) collectLamps(f,v);
    if (f.amounts[2]>0.f && !g_env_light.camera_water_in_status) collectWater(f,v);
    using namespace aurora::gfx;
    static const auto gatherId=register_draw_type({"World Lighting Gather",draw<false>,nullptr});
    static const auto compositeId=register_draw_type({"World Lighting Composite",draw<true>,nullptr});
    if (!gatherId || !compositeId) return;
    const auto uniforms=push_uniform(reinterpret_cast<const uint8_t*>(&f),sizeof(f));
    if (uniforms.size<sizeof(f)) return;
    ResolvedTargets source;
    if (!resolve_pass({true,true,false},source) || !source.color || !source.depth || !source.height) return;
    const uint32_t height=std::min(source.height,static_cast<uint32_t>(limit));
    const uint32_t width=std::max(1u,static_cast<uint32_t>((uint64_t(source.width)*height)/source.height));
    if (!create_pass(width,height)) return;
    Payload payload{source.color.Get(),source.depth.Get(),source.color.Get(),uniforms};
    const bool queued=push_custom_draw(gatherId,&payload,sizeof(payload));
    ResolvedTargets light;
    if (!resolve_pass({queued,false,false},light) || !queued || !light.color) return;
    payload.light=light.color.Get();
    push_custom_draw(compositeId,&payload,sizeof(payload));
}
} // namespace dusk::world_lighting
