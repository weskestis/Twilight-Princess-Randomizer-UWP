#include <memory>
#include <vector>
#include <optional>
#include <string>
#include <cstdio>
#include <stdexcept>
enum {LOG_LEVEL_INFO};namespace log {template<class...A>void write(A&&...) {}}
struct Config {bool enabled=true;void setValue(bool v){enabled=v;}bool getValue(){return enabled;}};
struct LoadedMod {struct{std::string id,exclusiveGroup;}metadata;bool active=false,enabledApplied=false;void* native=nullptr;std::optional<int>runtime;std::unique_ptr<Config>cvarIsEnabled=std::make_unique<Config>();};
struct ModLoader {std::vector<std::unique_ptr<LoadedMod>>m_mods;int deactivations=0;bool activate_mod(LoadedMod&);void deactivate_mod(LoadedMod&m){m.active=false;m.enabledApplied=false;++deactivations;}};
#include "production-group.inc"
void require(bool v,const char*s){if(!v)throw std::runtime_error(s);}
int main(int argc,char**) {try{
    ModLoader loader;
    for(auto [id,group]:{std::pair{"fierce","tunic"},{"classic","tunic"},{"shield","shield"},{"native","tunic"},{"script","tunic"},{"plain",""}}){auto m=std::make_unique<LoadedMod>();m->metadata={id,group};loader.m_mods.push_back(std::move(m));}
    auto& a=*loader.m_mods[0];auto& b=*loader.m_mods[1];auto& shield=*loader.m_mods[2];auto& native=*loader.m_mods[3];auto& script=*loader.m_mods[4];native.native=&native;script.runtime=1;
    loader.activate_mod(a);b.cvarIsEnabled->setValue(true);loader.activate_mod(b);
    if(argc>1)return a.active&&b.active?42:1;
    require(!a.active&&!a.cvarIsEnabled->getValue()&&b.active&&b.cvarIsEnabled->getValue(),"Selecting alternate outfit did not persist exclusive choice");
    require(shield.cvarIsEnabled->getValue()&&native.cvarIsEnabled->getValue()&&script.cvarIsEnabled->getValue(),"Unrelated shield/service/script disabled");
    a.cvarIsEnabled->setValue(true);loader.activate_mod(a);require(a.active&&!b.active&&!b.cvarIsEnabled->getValue(),"Switching back failed");
    loader.activate_mod(shield);require(a.active&&shield.active,"Different asset groups conflict");
    loader.activate_mod(*loader.m_mods[5]);require(a.active&&shield.active,"Ungrouped mod changed variant selection");
    require(loader.deactivations==2,"Inactive variant unnecessarily deactivated");puts("PASS: actual asset activation keeps one persisted outfit variant without disabling other groups or native providers");return 0;
}catch(const std::exception&e){fprintf(stderr,"%s\n",e.what());return 1;}}
