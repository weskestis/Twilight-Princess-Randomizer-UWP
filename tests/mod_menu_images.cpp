#include <cstring>
#include <new>
#include "production-provider.cpp"
#include <cstdio>
#include <cstdlib>

namespace dusk::ui {
std::optional<DecodedImage> decode_png(std::span<const uint8_t> data, std::string_view) {
    using F = mods::ModBundle::Failure;
    const auto failure = static_cast<F>(data[8]);
    if (failure == F::DecodeAllocation) throw std::bad_alloc{};
    if (failure == F::Unexpected) throw 17;
    if (failure == F::Decode) return {};
    uint32_t width, height;
    std::memcpy(&width, data.data(),4); std::memcpy(&height,data.data()+4,4);
    DecodedImage image{width,height,std::vector<uint8_t>(static_cast<size_t>(width)*height*4)};
    for (size_t i=0; i<image.pixels.size(); i+=4) {
        image.pixels[i]=21; image.pixels[i+1]=47; image.pixels[i+2]=81; image.pixels[i+3]=128;
    }
    return image;
}
}
static void require(bool result, const char* reason) {
    if (!result) { std::fprintf(stderr,"FAIL: %s\n",reason); std::exit(1); }
}
int main() {
    using namespace dusk::ui;
    using namespace dusk::mods;
    auto bundle=std::make_shared<ModBundle>();
    ModLoader::instance().entries.push_back({{"fixture","res/icon.png"},bundle});
#ifdef TPR_MOD_MENU_NEGATIVE_CONTROL
        bundle->failure=ModBundle::Failure::DecodeAllocation;
        try { mod_texture_provider("mod://fixture/res/icon.png?rev=fail"); }
        catch (const std::bad_alloc&) { return 42; }
        return 1;
#else
    auto icon=mod_texture_provider("mod://fixture/res/icon.png?rev=1");
    require(icon && icon->width==512 && icon->height==512,"2941px icon not reduced to 512px preview");
    require(icon->rgba8.size()==1024*1024,"Menu icon cache/GPU input exceeds 1 MiB");
    require(icon->premultipliedAlpha && icon->generateMipmaps,"Texture alpha or mipmap behavior lost");
    for (size_t i : {size_t(0), icon->rgba8.size()/2, icon->rgba8.size()-4}) {
        require(icon->rgba8[i]==std::byte{21} && icon->rgba8[i+1]==std::byte{47} &&
                icon->rgba8[i+2]==std::byte{81} && icon->rgba8[i+3]==std::byte{128},"Resampling changed constant premultiplied colors");
    }
    const auto cachedBytes=icon->rgba8.size();
    auto cached=mod_texture_provider("mod://fixture/res/icon.png?rev=1");
    require(cached && cached->rgba8.data()==icon->rgba8.data(),"Unchanged icon not reused");
    bundle->width=2048; bundle->height=877;
    auto banner=mod_texture_provider("mod://fixture/res/banner.png?rev=1");
    require(banner && banner->width==1024 && banner->height==438,"Banner preview lost aspect ratio or bound");
    bundle->width=64; bundle->height=32;
    auto small=mod_texture_provider("mod://fixture/res/icon.png?rev=small");
    require(small && small->width==64 && small->height==32,"Small icon unnecessarily enlarged");
    DecodedImage gradient{2,2,{0,0,0,0, 40,20,10,80, 80,40,20,160, 120,60,30,240}};
    fit_mod_image(gradient,1);
    require(gradient.width==1 && gradient.height==1 &&
            gradient.pixels==std::vector<uint8_t>({60,30,15,120}),
            "Thumbnail filtering lost spatial color/alpha averaging");
    for (auto failure : {ModBundle::Failure::Read,ModBundle::Failure::Decode,
                       ModBundle::Failure::DecodeAllocation,ModBundle::Failure::Unexpected,
                       ModBundle::Failure::HeaderAllocation}) {
        bundle->failure=failure;
        const std::string source="mod://fixture/res/icon.png?rev=fault"+std::to_string(static_cast<int>(failure));
        try { require(!mod_texture_provider(source),"Failed image accepted"); }
        catch (...) { require(false,"Menu image fault escaped provider"); }
        require(mod_texture_provider("mod://fixture/res/icon.png?rev=1")->rgba8.size()==cachedBytes,"Failed decode damaged working icon cache");
    }
    borealis::failLog=true;
    try { require(!mod_texture_provider("mod://fixture/res/icon.png?rev=log-fail"),"Logging failure accepted"); }
    catch (...) { require(false,"Image fault logging failure escaped provider"); }
    borealis::failLog=false;
    bundle->failure=ModBundle::Failure::None;
    bundle->width=600; bundle->height=600;
    for (int i=0; i<80; ++i) {
        require(bool(mod_texture_provider("mod://fixture/res/icon.png?rev=cache"+std::to_string(i))),"Cache eviction broke new thumbnail");
    }
    size_t bytes=0;
    for (const auto& [key,image]: image_cache()) bytes+=image.pixels.size();
    require(image_cache().size()<=64 && bytes<=64*1024*1024,"Menu preview cache exceeded limits");
    require(Rml::releases>0,"Evicted preview textures were not released");
    unregister_mod_texture_provider();
    require(image_cache().empty(),"Closing texture provider leaked decoded previews");
    std::puts("PASS: production mod thumbnails, cache eviction, alpha, read/decode/allocation/log faults; shipped .741 failure isolated");
#endif
}
