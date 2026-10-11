#include "PureItemTracker.hpp"
#include "dusk/config.hpp"
#include "dusk/settings.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

extern "C" {
bool tpr_xbox_transition_failure_pending() noexcept { return false; }
const char* tpr_xbox_transition_failure_report() noexcept { return ""; }
void tpr_xbox_transition_failure_acknowledge() noexcept {}
int tpr_xbox_transition_retry_recovery() noexcept { return 0; }
bool tpr_pure_tracker_active() noexcept { return true; }
std::size_t tpr_pure_tracker_slot_count() noexcept { return 36; }
const char* tpr_pure_tracker_slot_label(std::size_t) noexcept { return "Tracker item"; }
uint8_t tpr_pure_tracker_slot_icon(std::size_t) noexcept { return 1; }
int tpr_pure_tracker_slot_max(std::size_t) noexcept { return 1; }
int tpr_pure_tracker_slot_owned(std::size_t) noexcept { return 0; }
ImTextureID aurora_imgui_add_texture(uint32_t, uint32_t, const void*) { return {}; }
}

static constexpr const char* name = "Pure Item Tracker###PureItemTrackerV3";
static void require(bool result, const char* reason) {
    if (!result) { std::fprintf(stderr, "FAIL: %s\n", reason); std::exit(1); }
}
static bool near(float a, float b) { return std::abs(a - b) <= 1.5f; }
static ImGuiWindow* window() { return ImGui::FindWindowByName(name); }
static void frame() {
    ImGui::NewFrame();
    dusk::draw_pure_item_tracker();
    ImGui::Render();
}
static void setup(ImVec2 size) {
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.DisplaySize = size;
    io.DeltaTime = 1.0f / 60;
    io.IniFilename = nullptr;
    io.ConfigWindowsMoveFromTitleBarOnly = false;
    io.Fonts->AddFontDefault();
    unsigned char* pixels; int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    frame(); frame();
}
static void placement(float x, float y) {
    auto* w = window(); auto* vp = ImGui::GetMainViewport();
    require(w != nullptr, "Tracker window missing");
    require(near(w->Pos.x, vp->WorkPos.x + x * (vp->WorkSize.x - w->Size.x)), "Horizontal control did not move window");
    require(near(w->Pos.y, vp->WorkPos.y + y * (vp->WorkSize.y - w->Size.y)), "Vertical control did not move window");
    require(w->Pos.x >= vp->WorkPos.x && w->Pos.y >= vp->WorkPos.y, "Tracker above or left of screen");
    require(w->Pos.x + w->Size.x <= vp->WorkPos.x + vp->WorkSize.x + 1.5f, "Tracker beyond right edge");
    require(w->Pos.y + w->Size.y <= vp->WorkPos.y + vp->WorkSize.y + 1.5f, "Tracker beyond bottom edge");
    require((w->Flags & ImGuiWindowFlags_NoBackground) != 0, "Transparent background lost");
}
int main() {
    for (ImVec2 size : {ImVec2(1280,720), ImVec2(1920,1080), ImVec2(3840,2160)}) {
        dusk::settings = {};
        setup(size);
        auto& s = dusk::getSettings().game;
        placement(0.03f, 0.03f);
        for (ImVec2 pos : {ImVec2(1,1), ImVec2(0,0), ImVec2(0.5f,0.5f), ImVec2(0.8f,0.25f)}) {
            s.pureItemTrackerPositionX.setValue(pos.x);
            s.pureItemTrackerPositionY.setValue(pos.y);
            frame(); placement(pos.x, pos.y);
        }
        s.pureItemTrackerScale.setValue(1.5f);
        s.pureItemTrackerIconSize.setValue(112);
        s.pureItemTrackerColumns.setValue(9);
        frame(); placement(0.8f,0.25f);
        const int saved = dusk::config::saves;
        for (int i=0; i<120; ++i) frame();
        require(dusk::config::saves == saved, "Idle rendering repeatedly writes configuration");
        s.showPureItemTracker.setValue(false); frame();
        s.showPureItemTracker.setValue(true); frame(); placement(0.8f,0.25f);
        ImGui::GetIO().DisplaySize = ImVec2(640,360);
        frame(); frame(); placement(0.8f,0.25f);
        s.pureItemTrackerPositionX.setValue(std::numeric_limits<float>::quiet_NaN());
        s.pureItemTrackerPositionY.setValue(2);
        frame(); placement(0.03f,1);
        s.pureItemTrackerResetLayoutRequested.setValue(true);
        frame(); placement(0.03f,0.03f);
        require(!s.pureItemTrackerResetLayoutRequested.getValue(), "Layout reset flag not consumed");
        s.pureItemTrackerPositionX.setValue(0.2f); s.pureItemTrackerPositionY.setValue(0.6f);
        frame();
        ImGui::DestroyContext();
        setup(size); placement(0.2f,0.6f);
        ImGui::DestroyContext();
    }
    dusk::settings = {};
    setup(ImVec2(1920,1080));
    auto& s = dusk::getSettings().game;
    auto& io = ImGui::GetIO();
    auto* w = window();
    const ImVec2 start = w->Pos;
    io.AddMousePosEvent(start.x + 15, start.y + 15); frame();
    io.AddMouseButtonEvent(0,true); frame();
    const int saved = dusk::config::saves;
    io.AddMousePosEvent(start.x + 175, start.y + 125); frame();
    require(window()->Pos.x > start.x + 100, "Mouse drag is overwritten by saved placement");
    require(dusk::config::saves == saved, "Drag writes settings before release");
    io.AddMouseButtonEvent(0,false); frame();
    require(dusk::config::saves == saved + 1, "Completed mouse drag not saved once");
    const ImVec2 moved = window()->Pos;
    s.pureItemTrackerLock.setValue(true); frame();
    io.AddMousePosEvent(moved.x + 15, moved.y + 15); frame();
    io.AddMouseButtonEvent(0,true); frame();
    io.AddMousePosEvent(moved.x + 175, moved.y + 125); frame();
    io.AddMouseButtonEvent(0,false); frame();
    require(near(window()->Pos.x,moved.x) && near(window()->Pos.y,moved.y), "Locked tracker moved");
    ImGui::DestroyContext();
    std::puts("PASS: production tracker controller positions at 720p/1080p/4K, scale, bounds, reset, saved placement, mouse drag and lock");
}
