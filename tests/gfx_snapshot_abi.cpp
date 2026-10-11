#include <mods/svc/gfx.h>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <string>
struct LoadedMod {struct {std::string id="fixture";} metadata;} mod;
struct {template<class... T>void error(T&&...) {}} Log;
bool s_modOffscreenOpen=false;
#define OH_FUCK_SYNC
LoadedMod* mod_from_context(ModContext* c) {return c==reinterpret_cast<ModContext*>(1)?&mod:nullptr;}
namespace aurora::gfx {
bool fail=false,offscreen=false;
bool is_offscreen(){return offscreen;}
struct ResolveDesc{bool color,depth,normal;};
template<class T>struct Handle {T pointer=nullptr;T Get()const{return pointer;}};
struct ResolvedTargets {
    Handle<WGPUTextureView> color,depth,normal;
    WGPUTextureFormat colorFormat=WGPUTextureFormat_RGBA8Unorm;
    uint32_t width=1280,height=720;
    Handle<WGPUTexture> colorTexture,depthTexture,normalTexture;
};
bool resolve_pass(ResolveDesc d,ResolvedTargets& r) {
    if(fail)return false;
    r={};
    if(d.color){r.color.pointer=reinterpret_cast<WGPUTextureView>(10);r.colorTexture.pointer=reinterpret_cast<WGPUTexture>(11);}
    if(d.depth){r.depth.pointer=reinterpret_cast<WGPUTextureView>(20);r.depthTexture.pointer=reinterpret_cast<WGPUTexture>(21);}
    if(d.normal){r.normal.pointer=reinterpret_cast<WGPUTextureView>(30);r.normalTexture.pointer=reinterpret_cast<WGPUTexture>(31);}
    return true;
}
}
#include "production-gfx.inc"
void require(bool v,const char* s){if(!v)throw std::runtime_error(s);}
int main(){try{
    auto* context=reinterpret_cast<ModContext*>(1);
    GfxResolveDesc desc=GFX_RESOLVE_DESC_INIT;desc.depth=true;desc.normal=1;
    GfxResolvedTargets out=GFX_RESOLVED_TARGETS_INIT;
    require(gfx_resolve_pass_impl(context,&desc,&out)==MOD_OK,"New graphics snapshot rejected");
    require(out.colorTexture==reinterpret_cast<WGPUTexture>(11)&&out.depthTexture==reinterpret_cast<WGPUTexture>(21)&&out.normalTexture==reinterpret_cast<WGPUTexture>(31),"Borrowed snapshot handles do not match their views");
    require(out.width==1280&&out.height==720&&out.struct_size==sizeof(out),"Snapshot metadata changed");
    for(size_t size:{offsetof(GfxResolvedTargets,colorTexture),offsetof(GfxResolvedTargets,normal)}){
        out={};out.struct_size=static_cast<uint32_t>(size);
        std::memset(reinterpret_cast<unsigned char*>(&out)+size,0xa5,sizeof(out)-size);
        desc.normal=size==offsetof(GfxResolvedTargets,colorTexture)?1:0;
        require(gfx_resolve_pass_impl(context,&desc,&out)==MOD_OK,"Older snapshot buffer rejected");
        const auto* bytes=reinterpret_cast<const unsigned char*>(&out);
        for(size_t i=size;i<sizeof(out);++i)require(bytes[i]==0xa5,"Snapshot writes beyond old caller buffer");
        require(out.color==reinterpret_cast<WGPUTextureView>(10)&&out.depth==reinterpret_cast<WGPUTextureView>(20),"Old snapshot views lost");
        if(desc.normal)require(out.normal==reinterpret_cast<WGPUTextureView>(30),"SDK 1.3 normal snapshot lost");
    }
    desc={};desc.struct_size=offsetof(GfxResolveDesc,normal);desc.color=true;
    out=GfxResolvedTargets GFX_RESOLVED_TARGETS_INIT;
    require(gfx_resolve_pass_impl(context,&desc,&out)==MOD_OK&&out.colorTexture&&out.normalTexture==nullptr,"Legacy input reads newer fields");
    aurora::gfx::fail=true;out=GfxResolvedTargets GFX_RESOLVED_TARGETS_INIT;
    require(gfx_resolve_pass_impl(context,&desc,&out)==MOD_UNAVAILABLE&&out.color==nullptr&&out.colorTexture==nullptr,"Failed snapshot retains handles");
    aurora::gfx::fail=false;aurora::gfx::offscreen=true;
    require(gfx_resolve_pass_impl(context,&desc,&out)==MOD_UNAVAILABLE,"Game-owned offscreen pass exposed");
    s_modOffscreenOpen=true;require(gfx_resolve_pass_impl(context,&desc,&out)==MOD_OK&&!s_modOffscreenOpen,"Mod-owned offscreen pass does not close");
    out.struct_size=sizeof(out);require(gfx_resolve_pass_impl(nullptr,&desc,&out)==MOD_INVALID_ARGUMENT,"Invalid context accepted");
    require(gfx_resolve_pass_impl(context,&desc,nullptr)==MOD_INVALID_ARGUMENT,"Null output accepted");
    puts("PASS: production snapshot handles, SDK 1.0/1.3 output bounds, legacy descriptors, unavailable snapshots and offscreen ownership");return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
