#include "mods/svc/config.h"
#include "mods/svc/ui.h"
#include "mods/svc/resource.h"
#include "mods/svc/log.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

ModContext* mod_ctx=nullptr;
ConfigVarHandle g_controllerStyle=1;
bool s_controllerStyleLocked=false, s_useXbox=false, s_useSwitch=false;
constexpr bool kDeveloperOptions=false;
int64_t storedDesign=0;
int writes=0;
std::vector<UiControlDesc> controls;
ModResult read_int(ModContext*,ConfigVarHandle h,int64_t* out) {
    if(h!=g_controllerStyle) return MOD_INVALID_ARGUMENT;
    *out=storedDesign; return MOD_OK;
}
ModResult write_int(ModContext*,ConfigVarHandle h,int64_t value) {
    if(h!=g_controllerStyle) return MOD_INVALID_ARGUMENT;
    storedDesign=value; ++writes; return MOD_OK;
}
ModResult section(ModContext*,UiElementHandle,const char*) { return MOD_OK; }
ModResult text_line(ModContext*,UiElementHandle,const char*,UiElementHandle*) { return MOD_OK; }
ModResult control(ModContext*,UiElementHandle,const UiControlDesc* d,UiElementHandle*) {
    controls.push_back(*d); return MOD_OK;
}
ConfigService config{};
const ConfigService* svc_config=&config;
UiService ui{};
const UiService* svc_ui=&ui;
const LogService* svc_log=nullptr;
struct ResTIMG { uint8_t data[32]; };
#include "production-controller.inc"

void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
void panel() { controls.clear();require(build_layout_panel(nullptr,1,nullptr,nullptr)==MOD_OK,"panel failed");require(controls.size()==3,"design button missing"); }
int main(int argc,char**) {
    try {
        config.get_int=read_int; config.set_int=write_int;
        ui.pane_add_section=section;ui.pane_add_text=text_line;ui.pane_add_control=control;
        std::array<std::array<ResTIMG,3>,15> textures{};
        require(std::size(s_controllerTextures)==textures.size(),"texture fixture count changed");
        for(size_t i=0;i<textures.size();++i) {
            auto& t=s_controllerTextures[i];
            t.playstation->data=&textures[i][0];t.playstation->size=32;
            t.xbox.data=&textures[i][1];t.xbox.size=32;
            t.switchTexture.data=&textures[i][2];t.switchTexture.size=32;
        }
        for(int initial=0;initial<3;++initial) {
            storedDesign=initial;s_controllerStyleLocked=false;s_useXbox=s_useSwitch=false;
            panel();
            for(int next=0;next<3;++next) {
                require(!controls[next].is_disabled||!controls[next].is_disabled(nullptr,nullptr),"prelaunch button disabled");
                controls[next].on_pressed(nullptr,nullptr);require(storedDesign==next,"prelaunch choice not saved");
            }
            storedDesign=initial;
            const ResTIMG* active=resource_timg(*s_controllerTextures[0].playstation);
            require(active==&textures[0][initial],"wrong initial texture");
            panel();
            if(argc>1) return controls[1].is_disabled&&controls[1].is_disabled(nullptr,nullptr)?23:1;
            for(int next=0;next<3;++next) {
                require(!controls[next].is_disabled||!controls[next].is_disabled(nullptr,nullptr),"button disabled during play");
                int before=writes;controls[next].on_pressed(nullptr,nullptr);
                require(storedDesign==next&&writes==before+1,"late choice not saved exactly once");
                require(controls[next].is_selected(nullptr,nullptr),"selection indicator stale");
                require(resource_timg(*s_controllerTextures[0].playstation)==active,"live texture pointer changed");
                require(switch_controller_selected_now()==(initial==2),"calibration changed before texture restart");
            }
            s_controllerStyleLocked=false;
            require(resource_timg(*s_controllerTextures[0].playstation)==&textures[0][2],"saved next launch choice lost");
        }
        puts("Controller selection lifecycle passed");return 0;
    } catch(const std::exception& e) { fprintf(stderr,"%s\n",e.what());return 1; }
}
