#include "runtime_fixture.hpp"
#include <cmath>
#include <iostream>
#include <limits>
using namespace pokemon_battle::runtime;
using fixture::require;
int main(int argc,char** argv) {
    auto good=fixture::make(); Model model; std::string error; Pose pose;
    require(model.load(good,error),error.c_str());
    require(model.sample(0,1.0/120,false,pose),"sample");
    require(std::abs(pose.vertices[0].position[0]-.5f)<1e-5f,"halfway interpolation");
    require(pose.materials[0].uv[2]==0,"atlas must step, not interpolate");
    require(!pose.finished,"early completion");
    require(model.sample(0,1,false,pose) && pose.finished && pose.vertices[0].position[0]==1,"last frame held");
    require(model.sample(0,2.0/60,true,pose) && !pose.finished && pose.vertices[0].position[0]==0,"loop period");
    require(model.sample(0,1e300,true,pose),"huge finite time");
    require(!model.sample(1,0,false,pose),"invalid clip accepted");
    require(!model.sample(0,-1,false,pose),"negative time accepted");
    require(!model.sample(0,std::numeric_limits<double>::quiet_NaN(),false,pose),"NaN time accepted");
    for(std::size_t n=0;n<good.size();++n) {
        auto truncated=good;truncated.resize(n);
        if(n>=16)fixture::checksum(truncated);
        require(!model.load(truncated,error),"truncation accepted");
        require(model.vertex_count()==3 && model.bytes()==good.size(),"failed load destroyed current model");
    }
    for(auto offset:{16u,20u,24u,28u,32u,84u,96u,100u,104u,108u,112u,116u,120u,252u}) {
        auto bad=good; fixture::overwrite(bad,offset,0xffffffffu);fixture::checksum(bad);
        require(!model.load(bad,error),"malformed count/index/batch/texture/clip accepted");
    }
    auto bad=good;bad.back()^=1;require(!model.load(bad,error),"corrupt CRC accepted");
    bad=good;bad.push_back(0);fixture::checksum(bad);require(!model.load(bad,error),"trailing data accepted");
    bad=good;fixture::overwrite(bad,36,0x7fc00000u);fixture::checksum(bad);require(!model.load(bad,error),"NaN accepted");
    bad=good;for(std::size_t i=266;i<272;++i)bad[i]=0;fixture::checksum(bad);require(!model.load(bad,error),"zero normal accepted");
    for(int i=1;i<argc;++i) {
        auto bytes=fixture::read(argv[i]);require(model.load(bytes,error),error.c_str());
        require((model.clips().size()==15 || model.clips().size()==17) && model.indices().size()/3==3749,"unexpected Pikachu counts");
        std::size_t samples=0;
        for(std::size_t c=0;c<model.clips().size();++c) {
            const auto& clip=model.clips()[c];
            for(std::uint32_t f=0;f<clip.frames;++f) {
                require(model.sample(c,double(f)/clip.fps,false,pose),"real clip sample");
                for(const auto& v:pose.vertices) for(float x:v.position) require(std::isfinite(x),"nonfinite pose");
                ++samples;
            }
        }
        std::cout<<"PASS real pack: "<<model.vertex_count()<<" vertices, "<<samples<<" sampled poses, "<<model.bytes()<<" bytes\n";
    }
    std::cout<<"PASS malformed packs, transactional loading, interpolation, atlas steps, timing and all real poses\n";
}
