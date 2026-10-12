#include "appearance_runtime.hpp"
#include <cassert>
#include <set>
#include <cstdio>
using namespace dusk_xbox_appearance;
int main() {
    const std::vector<Target> targets{{1,"herosTunicCapColor"}, {2,"heartColor"},
        {3,"aButtonColor"}, {4,"midnaHairBaseColor",true}, {5,"linkHairColor"}};
    Runtime runtime;
    runtime.sync(targets,Mode::Manual,1,"seed-a",0,1);
    assert(!runtime.string(1) && !runtime.integer(4));
    std::set<std::string> sceneColors;
    std::size_t previousFamily=8; bool previousRainbow=false;
    for (unsigned revision=1; revision<2000; ++revision) {
        runtime.sync(targets,Mode::Load,revision,"seed-a",0,1);
        const auto color=*runtime.string(1), heart=*runtime.string(2);
        const auto hair=*runtime.integer(4);
        assert(hair>=0 && hair<=8);
        sceneColors.insert(color);
        for(unsigned render=0; render<120; ++render) {
            runtime.sync(targets,Mode::Load,revision,"seed-b",0,1);
            assert(*runtime.string(1)==color && *runtime.string(2)==heart);
        }
        std::size_t family=8;
        for(std::size_t i=0;i<8;++i) for(const auto shade:HeartPalettes[i]) if(shade==heart) family=i;
        if(family==8) { assert(!previousRainbow); previousRainbow=true; }
        else {
            if(previousFamily<8) { const auto d=family>previousFamily ? family-previousFamily : previousFamily-family; assert(std::min(d,8-d)>=2); }
            previousFamily=family;previousRainbow=false;
        }
    }
    assert(sceneColors.size()>1000);
    runtime.sync(targets,Mode::Seed,1,"seed-a",0,1);const auto seedColor=*runtime.string(1);
    runtime.sync(targets,Mode::Seed,123456,"seed-a",0,1);assert(*runtime.string(1)==seedColor);
    Runtime other;other.sync(targets,Mode::Seed,2,"seed-a",0,1);assert(*other.string(1)==seedColor);
    std::set<std::string> seeds;
    for(unsigned i=0;i<100;++i) { other.sync(targets,Mode::Seed,i,std::to_string(i),0,1);seeds.insert(*other.string(1)); }
    assert(seeds.size()>80);
    runtime.sync(targets,Mode::Rainbow,1,"",0,1);assert(*runtime.string(1)=="ff0000");
    runtime.sync(targets,Mode::Rainbow,1,"",1000,1);assert(*runtime.string(1)=="ff4000");
    assert(rainbow(2000,1)=="ff8000");
    assert(rainbow(4000,0)=="ff8000" && rainbow(1000,2)=="ff8000");
    runtime.sync(targets,Mode::Manual,1,"seed-a",0,1);assert(!runtime.string(1) && !runtime.integer(4));
    runtime.sync(targets,Mode::Seed,1,"",0,1);assert(!runtime.string(1));
    runtime.sync(targets,Mode::Load,99,"",0,1);runtime.clear();assert(!runtime.string(1));
    assert(!runtime.string(0));
    puts("PASS: 1999 scene rolls, stable per-scene overrides, distinct heart families, Rainbow candidates/fade/speed, deterministic seeds, manual restoration and shutdown");
}
