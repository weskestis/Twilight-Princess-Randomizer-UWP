#define main tracker_regressions
#include <pure_tracker_position.cpp>
#undef main
#include <string>
static bool runActive=false;
static uint64_t centiseconds=372345;
extern "C" bool tpr_run_overlay_active() noexcept { return runActive; }
extern "C" uint64_t tpr_run_overlay_centiseconds() noexcept { return centiseconds; }
static void overlay_frame() { ImGui::NewFrame();dusk::draw_run_overlays();ImGui::Render(); }
static ImGuiWindow* timer() { return ImGui::FindWindowByName("###GameTimerOverlayV1"); }
static ImGuiWindow* deaths() { return ImGui::FindWindowByName("###DeathCounterOverlayV1"); }
static void expect_position(ImGuiWindow* w,float x,float y) {
    require(w && w->Active,"Enabled run overlay absent");auto* v=ImGui::GetMainViewport();
    require(near(w->Pos.x,v->WorkPos.x+x*(v->WorkSize.x-w->Size.x)),"Overlay horizontal placement failed");
    require(near(w->Pos.y,v->WorkPos.y+y*(v->WorkSize.y-w->Size.y)),"Overlay vertical placement failed");
    require((w->Flags&ImGuiWindowFlags_NoBackground)!=0,"Overlay gained a background");
    require((w->Flags&ImGuiWindowFlags_NoInputs)!=0,"Overlay steals gameplay inputs");
}
int main() {
    tracker_regressions();
    for(const auto resolution:{ImVec2(1280,720),ImVec2(1920,1080),ImVec2(3840,2160)}) {
        dusk::settings={};setup(resolution);auto& s=dusk::settings.game;
        s.showGameTimerOverlay.setValue(true);s.showDeathCounterOverlay.setValue(true);
        overlay_frame();require(!timer()&&!deaths(),"Overlays appear before an active save");
        runActive=true;overlay_frame();overlay_frame();
        expect_position(timer(),0.5f,0.015f);expect_position(deaths(),0.5f,0.054f);
        bool red=false;for(const auto vertex:deaths()->DrawList->VtxBuffer) if(vertex.col==IM_COL32(255,51,51,255))red=true;
        require(red,"Death counter is not red");
        const float small=timer()->Size.x;
        s.gameTimerOverlayScale.setValue(2);s.gameTimerOverlayPositionX.setValue(1);s.gameTimerOverlayPositionY.setValue(1);
        s.deathCounterOverlayScale.setValue(0.75f);s.deathCounterOverlayPositionX.setValue(0);s.deathCounterOverlayPositionY.setValue(0);
        overlay_frame();overlay_frame();expect_position(timer(),1,1);expect_position(deaths(),0,0);
        require(timer()->Size.x>small,"Timer scale did not resize text");
        const int saved=dusk::config::saves;for(int i=0;i<120;++i)overlay_frame();
        require(dusk::config::saves==saved,"Rendering run overlays repeatedly saves configuration");
        s.showDeathCounterOverlay.setValue(false);overlay_frame();require(!deaths()->Active&&timer()->Active,"Independent overlay toggles failed");
        s.gameTimerOverlayResetLayoutRequested.setValue(true);overlay_frame();overlay_frame();
        require(!s.gameTimerOverlayResetLayoutRequested.getValue()&&dusk::config::saves==saved+1,"Reset layout not consumed exactly once");
        s.gameTimerOverlayScale.setValue(std::numeric_limits<float>::quiet_NaN());
        s.gameTimerOverlayPositionX.setValue(std::numeric_limits<float>::quiet_NaN());s.gameTimerOverlayPositionY.setValue(2);
        overlay_frame();overlay_frame();expect_position(timer(),0.5f,1);
        runActive=false;overlay_frame();require(!timer()->Active,"Timer remains visible after save unload");
        ImGui::DestroyContext();
    }
    std::puts("PASS: real ImGui run overlays at 720p/1080p/4K, red death text, scale, controller placement, independent toggles, transparency, no input capture, idle writes, reset and unload");
}
