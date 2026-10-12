#include <algorithm>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include "production-provider.cpp"
namespace dusk::ui {
int decodeCalls=0;
std::optional<DecodedImage> decode_png(std::span<const uint8_t> data,std::string_view) {
    ++decodeCalls;
    const auto read=[&](int n){return (uint32_t(data[n])<<24)|(uint32_t(data[n+1])<<16)|(uint32_t(data[n+2])<<8)|data[n+3];};
    const auto width=read(16),height=read(20);
    using F=mods::ModBundle::Failure;const auto failure=static_cast<F>(data[32]);
    if(failure==F::DecodeAllocation)throw std::bad_alloc{};
    if(failure==F::Unexpected)throw 17;
    if(failure==F::Decode)return {};
    return DecodedImage{width,height,std::vector<uint8_t>(size_t(width)*height*4,42)};
}
}
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(int argc,char**) {try{
    using namespace dusk::ui;using namespace dusk::mods;
    auto& loader=ModLoader::instance();
    for(int i=0;i<88;++i){auto b=std::make_shared<ModBundle>();b->width=600;b->height=600;loader.entries.push_back({{"fixture"+std::to_string(i),"res/icon.png"},b});}
    for(int i=0;i<88;++i)require(bool(mod_texture_provider("mod://fixture"+std::to_string(i)+"/res/icon.png?rev=1")),"Icon load failed");
    if(argc>1)return image_cache().size()!=88?42:1;
    const auto firstReads=decodeCalls;
    for(int i=0;i<100;++i){
        const auto id="fixture"+std::to_string(i%88);
        loader.entries[i%88].bundle->width=1024;loader.entries[i%88].bundle->height=1024;
        require(bool(mod_texture_provider("mod://"+id+"/res/banner.png?rev="+std::to_string(i))),"Preview load failed");
        const auto before=decodeCalls;
        auto icon=mod_texture_provider("mod://"+id+"/res/icon.png?rev=1");
        require(icon&&icon->width==128&&decodeCalls==before,"Scrolling evicted a visible list icon");
        require(!icon->generateMipmaps,"UI icon scheduled mipmap generation");
    }
    require(decodeCalls==firstReads+100,"Warm icons decoded again during preview navigation");
    size_t bytes=0;for(const auto& [key,image]:image_cache())bytes+=image.pixels.size();
    require(bytes<=16*1024*1024&&image_cache().size()<=256&&Rml::releases>0,"Cache byte/count/GPU bounds failed");
    unregister_mod_texture_provider();require(image_cache().empty(),"Cache shutdown failed");
    auto b=loader.entries.front().bundle;b->prepared=true;b->width=2941;b->height=2941;
    auto prepared=mod_texture_provider("mod://fixture0/res/icon.png?rev=prepared");
    require(prepared&&prepared->width==128&&b->lastRead==prepared_image_path("res/icon.png"),"Prepared thumbnail not used");
    require(prepared_image_path("res/a/b.png")!=prepared_image_path("res/a~b.png"),"Prepared paths collide");
    b->prepared=false;b->width=4096;b->height=4096;auto before=decodeCalls;
    require(!mod_texture_provider("mod://fixture0/res/banner.png?rev=oversize")&&decodeCalls==before,"Decode budget lost");
    b->width=64;b->height=64;
    for(auto f:{ModBundle::Failure::Read,ModBundle::Failure::Decode,ModBundle::Failure::DecodeAllocation,ModBundle::Failure::Unexpected,ModBundle::Failure::HeaderAllocation}){
        b->failure=f;require(!mod_texture_provider("mod://fixture0/res/icon.png?rev=fault"+std::to_string(int(f))),"Load/decode fault escaped");
    }
    unregister_mod_texture_provider();require(image_cache().empty()&&image_use().empty(),"Cache metadata retained after shutdown");
    puts("PASS .744: 88 resident icons, 100 preview changes, zero warm icon decodes, prepared thumbnails, 16 MiB bound and fault cleanup");return 0;
}catch(const std::exception&e){fprintf(stderr,"%s\n",e.what());return 1;}}
