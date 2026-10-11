#include <algorithm>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include "production-provider.cpp"
namespace dusk::ui {
inline int decodeCalls=0;
std::optional<DecodedImage> decode_png(std::span<const uint8_t> data,std::string_view) {
    ++decodeCalls;
    const auto read=[&](int n){return (uint32_t(data[n])<<24)|(uint32_t(data[n+1])<<16)|(uint32_t(data[n+2])<<8)|data[n+3];};
    const auto width=read(16),height=read(20);
    if(width==4096&&height==4096)return {}; // Record dispatch without allocating 64 MiB in the negative fixture.
    using F=mods::ModBundle::Failure;const auto failure=static_cast<F>(data[32]);
    if(failure==F::DecodeAllocation)throw std::bad_alloc{};
    if(failure==F::Unexpected)throw 17;
    if(failure==F::Decode)return {};
    return DecodedImage{width,height,std::vector<uint8_t>(static_cast<size_t>(width)*height*4,42)};
}
}
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(int argc,char**) {try{
    using namespace dusk::ui;using namespace dusk::mods;
    auto bundle=std::make_shared<ModBundle>();ModLoader::instance().entries.push_back({{"fixture","res/icon.png"},bundle});
    bundle->width=4096;bundle->height=4096;
    require(!mod_texture_provider("mod://fixture/res/banner.png?rev=large"),"Over-budget image accepted");
    if(argc>1)return decodeCalls==1?42:1;
    require(decodeCalls==0,"Oversized image reached the decoder before memory budget check");
    bundle->width=2941;bundle->height=2941;
    auto icon=mod_texture_provider("mod://fixture/res/icon.png?rev=1");
    require(icon&&icon->width==256&&icon->height==256,"Author icon failed to fit the bounded thumbnail");
    const auto* cached=icon->rgba8.data();const auto calls=decodeCalls;
    require(mod_texture_provider("mod://fixture/res/icon.png?rev=1")->rgba8.data()==cached&&decodeCalls==calls,"Cached image decoded again");
    bundle->width=2048;bundle->height=877;
    auto banner=mod_texture_provider("mod://fixture/res/banner.png?rev=1");
    require(banner&&banner->width==768&&banner->height==328,"Banner aspect ratio or memory bound lost");
    bundle->width=32;bundle->height=16;
    auto small=mod_texture_provider("mod://fixture/res/icon.png?rev=small");require(small&&small->width==32&&small->height==16,"Small icon enlarged");
    bundle->width=0;require(!mod_texture_provider("mod://fixture/res/icon.png?rev=zero"),"Zero dimension accepted");
    bundle->width=0xffffffff;bundle->height=0xffffffff;require(!mod_texture_provider("mod://fixture/res/icon.png?rev=overflow"),"Dimension overflow accepted");
    bundle->width=64;bundle->height=64;
    for(auto f:{ModBundle::Failure::Read,ModBundle::Failure::Decode,ModBundle::Failure::DecodeAllocation,ModBundle::Failure::Unexpected,ModBundle::Failure::HeaderAllocation}){
        bundle->failure=f;require(!mod_texture_provider("mod://fixture/res/icon.png?rev=fault"+std::to_string(int(f))),"Fault escaped or produced image");
    }
    bundle->failure=ModBundle::Failure::None;bundle->width=600;bundle->height=600;
    for(int i=0;i<160;++i)require(bool(mod_texture_provider("mod://fixture/res/icon.png?rev=cache"+std::to_string(i))),"Repeated navigation failed");
    size_t bytes=0;for(const auto& [key,image]:image_cache())bytes+=image.pixels.size();
    require(bytes<=16*1024*1024&&image_cache().size()<=64&&Rml::releases>0,"Preview CPU/GPU eviction failed");
    unregister_mod_texture_provider();require(image_cache().empty(),"Provider retained previews after shutdown");
    puts("PASS: production pre-decode budget, bounded previews, repeated cache eviction and decode faults");return 0;
}catch(const std::exception&e){fprintf(stderr,"%s\n",e.what());return 1;}}
