#include "m_Do/m_Do_controller_pad.h"

#include "imgui.h"
#include "ImGuiMenuTools.hpp"
#include "dusk/settings.h"

#include <dolphin/pad.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace dusk::config {
void save();
}

namespace dusk {
void SetOverlayWindowLocation(int corner);
float ImGuiScale();

namespace {

// TPR UWP: primary layered input viewer. A compact simple viewer is retained
// below only as an automatic compatibility fallback for invalid drawing bounds.
constexpr float kViewerDesignWidth = 600.0f;
constexpr float kViewerDesignHeight = 310.0f;

struct InputViewerState {
    float leftX = 0.0f;
    float leftY = 0.0f;
    float cX = 0.0f;
    float cY = 0.0f;
    float leftTrigger = 0.0f;
    float rightTrigger = 0.0f;
    std::array<float, 3> gyro{};
    bool gyroAvailable = false;
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool start = false;
    bool a = false;
    bool b = false;
    bool x = false;
    bool y = false;
    bool z = false;
    bool l = false;
    bool r = false;
};

float clamp_axis(float value) {
    return std::clamp(value, -1.0f, 1.0f);
}

float clamp_trigger(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

InputViewerState sample_input_viewer_state(bool sampleGyro) {
    InputViewerState state;
    state.leftX = clamp_axis(mDoCPd_c::getStickX(PAD_1));
    state.leftY = clamp_axis(mDoCPd_c::getStickY(PAD_1));
    state.cX = clamp_axis(mDoCPd_c::getSubStickX(PAD_1));
    state.cY = clamp_axis(mDoCPd_c::getSubStickY(PAD_1));
    state.leftTrigger = clamp_trigger(mDoCPd_c::getAnalogL(PAD_1));
    state.rightTrigger = clamp_trigger(mDoCPd_c::getAnalogR(PAD_1));
    state.up = mDoCPd_c::getHoldUp(PAD_1);
    state.down = mDoCPd_c::getHoldDown(PAD_1);
    state.left = mDoCPd_c::getHoldLeft(PAD_1);
    state.right = mDoCPd_c::getHoldRight(PAD_1);
    state.start = mDoCPd_c::getHoldStart(PAD_1);
    state.a = mDoCPd_c::getHoldA(PAD_1);
    state.b = mDoCPd_c::getHoldB(PAD_1);
    state.x = mDoCPd_c::getHoldX(PAD_1);
    state.y = mDoCPd_c::getHoldY(PAD_1);
    state.z = mDoCPd_c::getHoldZ(PAD_1);
    state.l = mDoCPd_c::getHoldL(PAD_1);
    state.r = mDoCPd_c::getHoldR(PAD_1);

    if (sampleGyro && PADSetSensorEnabled(PAD_1, PAD_SENSOR_GYRO, TRUE) == TRUE) {
        f32 gyro[3]{};
        if (PADGetSensorData(PAD_1, PAD_SENSOR_GYRO, gyro, 3) == TRUE) {
            state.gyro = {gyro[0], gyro[1], gyro[2]};
            state.gyroAvailable = true;
        }
    }
    return state;
}

bool input_viewer_menu_item(const char* label, ConfigVar<bool>& setting) {
    bool selected = setting.getValue();
    if (!ImGui::MenuItem(label, nullptr, &selected)) {
        return false;
    }
    setting.setValue(selected);
    config::save();
    return true;
}

ImVec2 viewer_point(const ImVec2& origin, float scale, float x, float y) {
    return {origin.x + x * scale, origin.y + y * scale};
}

ImVec2 viewer_radius(float scale, float x, float y) {
    return {x * scale, y * scale};
}

void draw_centered_text(
    ImDrawList* drawList, const ImVec2& center, float fontSize, ImU32 color, const char* text)
{
    const float safeFontSize = std::max(8.0f, fontSize);
    const ImVec2 size = ImGui::GetFont()->CalcTextSizeA(safeFontSize, 10000.0f, 0.0f, text);
    drawList->AddText(
        ImGui::GetFont(), safeFontSize,
        {center.x - size.x * 0.5f, center.y - size.y * 0.5f}, color, text);
}

void draw_controller_shell(
    ImDrawList* drawList, const ImVec2& origin, float scale, float expansion, ImU32 color,
    float yOffset = 0.0f)
{
    const auto point = [&](float x, float y) {
        return viewer_point(origin, scale, x, y + yOffset);
    };
    const auto radius = [&](float x, float y) {
        return viewer_radius(scale, x + expansion, y + expansion);
    };

    drawList->AddEllipseFilled(point(300.0f, 145.0f), radius(218.0f, 105.0f), color, 64);
    drawList->AddEllipseFilled(point(112.0f, 196.0f), radius(77.0f, 103.0f), color, 48);
    drawList->AddEllipseFilled(point(488.0f, 196.0f), radius(77.0f, 103.0f), color, 48);
    drawList->AddRectFilled(
        point(116.0f - expansion, 72.0f - expansion),
        point(484.0f + expansion, 190.0f + expansion), color,
        (58.0f + expansion) * scale);
}

void draw_trigger(
    ImDrawList* drawList, const ImVec2& origin, float scale, float x, float value,
    bool clicked, const char* label, bool drawOutline)
{
    constexpr ImU32 kOutline = IM_COL32(214, 178, 74, 255);
    constexpr ImU32 kBase = IM_COL32(54, 57, 63, 255);
    constexpr ImU32 kTravel = IM_COL32(225, 229, 235, 255);
    constexpr ImU32 kClick = IM_COL32(255, 205, 70, 255);
    const float width = 118.0f;
    const float height = 27.0f;
    const ImVec2 min = viewer_point(origin, scale, x, 53.0f);
    const ImVec2 max = viewer_point(origin, scale, x + width, 53.0f + height);

    if (drawOutline || clicked) {
        const float pad = (clicked ? 3.0f : 2.0f) * scale;
        drawList->AddRectFilled(
            {min.x - pad, min.y - pad}, {max.x + pad, max.y + pad},
            clicked ? kClick : kOutline, 10.0f * scale);
    }
    drawList->AddRectFilled(min, max, kBase, 8.0f * scale);

    const float fillWidth = (width - 6.0f) * value;
    if (fillWidth > 0.5f) {
        const ImVec2 fillMin = viewer_point(origin, scale, x + 3.0f, 56.0f);
        const ImVec2 fillMax = viewer_point(origin, scale, x + 3.0f + fillWidth, 77.0f);
        drawList->AddRectFilled(fillMin, fillMax, clicked ? kClick : kTravel, 6.0f * scale);
    }
    draw_centered_text(
        drawList, viewer_point(origin, scale, x + width * 0.5f, 66.5f),
        14.0f * scale, clicked ? IM_COL32(43, 36, 18, 255) : IM_COL32(18, 20, 24, 255), label);
}

void draw_stick(
    ImDrawList* drawList, const ImVec2& origin, float scale, const ImVec2& designCenter,
    float gateRadius, float knobRadius, float x, float y, ImU32 knobColor, bool drawOutline,
    int gateSegments)
{
    constexpr ImU32 kOutline = IM_COL32(214, 178, 74, 255);
    constexpr ImU32 kGate = IM_COL32(43, 46, 52, 255);
    constexpr ImU32 kGateInner = IM_COL32(20, 22, 26, 255);
    const ImVec2 center = viewer_point(origin, scale, designCenter.x, designCenter.y);
    const float magnitude = std::sqrt(x * x + y * y);
    if (drawOutline) {
        drawList->AddCircleFilled(center, (gateRadius + 3.0f) * scale, kOutline, gateSegments);
    }
    drawList->AddCircleFilled(center, gateRadius * scale, kGate, gateSegments);
    drawList->AddCircleFilled(center, (gateRadius - 7.0f) * scale, kGateInner, gateSegments);
    if (magnitude > 0.08f) {
        drawList->AddCircle(
            center, (gateRadius - 4.0f) * scale, IM_COL32(255, 218, 92, 220),
            gateSegments, 2.0f * scale);
    }

    const float travel = (gateRadius - knobRadius - 5.0f) * scale;
    const ImVec2 knobCenter = {center.x + x * travel, center.y - y * travel};
    drawList->AddCircleFilled(
        {knobCenter.x + 2.0f * scale, knobCenter.y + 3.0f * scale},
        knobRadius * scale, IM_COL32(0, 0, 0, 100), 32);
    drawList->AddCircleFilled(knobCenter, knobRadius * scale, knobColor, 32);
    drawList->AddCircle(
        knobCenter, (knobRadius - 5.0f) * scale, IM_COL32(255, 255, 255, 55),
        24, 1.5f * scale);
    drawList->AddCircleFilled(knobCenter, 3.0f * scale, IM_COL32(35, 36, 38, 190), 16);
}

void draw_dpad(
    ImDrawList* drawList, const ImVec2& origin, float scale, const InputViewerState& state,
    bool drawOutline)
{
    constexpr ImU32 kOutline = IM_COL32(214, 178, 74, 255);
    constexpr ImU32 kBase = IM_COL32(48, 51, 57, 255);
    constexpr ImU32 kActive = IM_COL32(242, 244, 248, 255);
    const float cx = 232.0f;
    const float cy = 218.0f;
    const float arm = 31.0f;
    const float half = 9.0f;
    const float outlinePad = drawOutline ? 2.0f : 0.0f;

    if (drawOutline) {
        drawList->AddRectFilled(
            viewer_point(origin, scale, cx - half - outlinePad, cy - arm - outlinePad),
            viewer_point(origin, scale, cx + half + outlinePad, cy + arm + outlinePad),
            kOutline, 4.0f * scale);
        drawList->AddRectFilled(
            viewer_point(origin, scale, cx - arm - outlinePad, cy - half - outlinePad),
            viewer_point(origin, scale, cx + arm + outlinePad, cy + half + outlinePad),
            kOutline, 4.0f * scale);
    }
    drawList->AddRectFilled(
        viewer_point(origin, scale, cx - half, cy - arm),
        viewer_point(origin, scale, cx + half, cy + arm), kBase, 3.0f * scale);
    drawList->AddRectFilled(
        viewer_point(origin, scale, cx - arm, cy - half),
        viewer_point(origin, scale, cx + arm, cy + half), kBase, 3.0f * scale);

    if (state.up) {
        drawList->AddRectFilled(
            viewer_point(origin, scale, cx - half + 2.0f, cy - arm + 2.0f),
            viewer_point(origin, scale, cx + half - 2.0f, cy - 4.0f), kActive, 2.0f * scale);
    }
    if (state.down) {
        drawList->AddRectFilled(
            viewer_point(origin, scale, cx - half + 2.0f, cy + 4.0f),
            viewer_point(origin, scale, cx + half - 2.0f, cy + arm - 2.0f), kActive, 2.0f * scale);
    }
    if (state.left) {
        drawList->AddRectFilled(
            viewer_point(origin, scale, cx - arm + 2.0f, cy - half + 2.0f),
            viewer_point(origin, scale, cx - 4.0f, cy + half - 2.0f), kActive, 2.0f * scale);
    }
    if (state.right) {
        drawList->AddRectFilled(
            viewer_point(origin, scale, cx + 4.0f, cy - half + 2.0f),
            viewer_point(origin, scale, cx + arm - 2.0f, cy + half - 2.0f), kActive, 2.0f * scale);
    }
    drawList->AddCircleFilled(
        viewer_point(origin, scale, cx, cy), 5.0f * scale,
        (state.up || state.down || state.left || state.right) ? kActive : kBase, 16);
}

void draw_round_button(
    ImDrawList* drawList, const ImVec2& origin, float scale, const ImVec2& designCenter,
    float radius, bool pressed, ImU32 neutralColor, ImU32 activeColor, const char* label,
    bool drawOutline)
{
    constexpr ImU32 kOutline = IM_COL32(214, 178, 74, 255);
    const ImVec2 center = viewer_point(origin, scale, designCenter.x, designCenter.y);
    if (drawOutline || pressed) {
        drawList->AddCircleFilled(
            center, (radius + (pressed ? 3.0f : 2.0f)) * scale,
            pressed ? IM_COL32(255, 229, 135, 255) : kOutline, 32);
    }
    drawList->AddCircleFilled(center, radius * scale, pressed ? activeColor : neutralColor, 32);
    draw_centered_text(
        drawList, center, std::max(10.0f, radius * 0.9f) * scale,
        pressed ? IM_COL32(255, 255, 255, 255) : IM_COL32(205, 207, 212, 230), label);
}

void draw_ellipse_button(
    ImDrawList* drawList, const ImVec2& origin, float scale, const ImVec2& designCenter,
    const ImVec2& designRadius, bool pressed, const char* label, bool drawOutline)
{
    constexpr ImU32 kOutline = IM_COL32(214, 178, 74, 255);
    const ImVec2 center = viewer_point(origin, scale, designCenter.x, designCenter.y);
    if (drawOutline || pressed) {
        drawList->AddEllipseFilled(
            center, viewer_radius(scale, designRadius.x + (pressed ? 3.0f : 2.0f),
                                  designRadius.y + (pressed ? 3.0f : 2.0f)),
            pressed ? IM_COL32(255, 229, 135, 255) : kOutline, 36);
    }
    drawList->AddEllipseFilled(
        center, viewer_radius(scale, designRadius.x, designRadius.y),
        pressed ? IM_COL32(225, 239, 249, 255) : IM_COL32(103, 109, 117, 255), 36);
    draw_centered_text(
        drawList, center, 13.0f * scale,
        pressed ? IM_COL32(32, 35, 40, 255) : IM_COL32(218, 221, 226, 235), label);
}

void draw_triforce(ImDrawList* drawList, const ImVec2& origin, float scale) {
    constexpr ImU32 kGold = IM_COL32(218, 178, 67, 170);
    const auto triangle = [&](float cx, float cy) {
        drawList->AddTriangleFilled(
            viewer_point(origin, scale, cx, cy - 8.0f),
            viewer_point(origin, scale, cx - 8.0f, cy + 6.0f),
            viewer_point(origin, scale, cx + 8.0f, cy + 6.0f), kGold);
    };
    triangle(300.0f, 164.0f);
    triangle(291.0f, 180.0f);
    triangle(309.0f, 180.0f);
}

bool draw_primary_input_viewer(
    ImDrawList* drawList, const ImVec2& origin, float scale, const InputViewerState& state,
    const char* controllerName, bool drawBackground, bool drawOutline, bool drawSticks)
{
    if (drawList == nullptr || !std::isfinite(scale) || scale < 0.25f) {
        return false;
    }

    constexpr ImU32 kPanel = IM_COL32(9, 12, 18, 218);
    constexpr ImU32 kPanelTop = IM_COL32(36, 31, 22, 210);
    constexpr ImU32 kGold = IM_COL32(214, 178, 74, 255);
    constexpr ImU32 kShadow = IM_COL32(0, 0, 0, 105);
    constexpr ImU32 kShellOutline = IM_COL32(68, 56, 31, 255);
    constexpr ImU32 kShell = IM_COL32(103, 107, 116, 255);
    constexpr ImU32 kShellLight = IM_COL32(127, 131, 140, 155);

    if (drawBackground) {
        drawList->AddRectFilled(
            origin, viewer_point(origin, scale, kViewerDesignWidth, kViewerDesignHeight),
            kPanel, 18.0f * scale);
        drawList->AddRectFilled(
            origin, viewer_point(origin, scale, kViewerDesignWidth, 34.0f),
            kPanelTop, 18.0f * scale);
        drawList->AddLine(
            viewer_point(origin, scale, 18.0f, 34.0f),
            viewer_point(origin, scale, 582.0f, 34.0f), kGold, 1.5f * scale);
    }

    draw_controller_shell(drawList, origin, scale, 5.0f, kShadow, 7.0f);
    if (drawOutline) {
        draw_controller_shell(drawList, origin, scale, 4.0f, kShellOutline);
    }
    draw_controller_shell(drawList, origin, scale, 0.0f, kShell);
    drawList->AddEllipseFilled(
        viewer_point(origin, scale, 300.0f, 132.0f), viewer_radius(scale, 187.0f, 70.0f),
        kShellLight, 64);
    drawList->AddRectFilled(
        viewer_point(origin, scale, 145.0f, 84.0f), viewer_point(origin, scale, 455.0f, 103.0f),
        IM_COL32(255, 255, 255, 16), 10.0f * scale);

    draw_trigger(
        drawList, origin, scale, 105.0f, state.leftTrigger, state.l, "L", drawOutline);
    draw_trigger(
        drawList, origin, scale, 377.0f, state.rightTrigger, state.r, "R", drawOutline);

    draw_ellipse_button(
        drawList, origin, scale, {498.0f, 91.0f}, {29.0f, 11.0f}, state.z, "Z", drawOutline);

    if (drawSticks) {
        draw_stick(
            drawList, origin, scale, {151.0f, 143.0f}, 47.0f, 24.0f,
            state.leftX, state.leftY, IM_COL32(196, 199, 204, 255), drawOutline, 8);
        draw_stick(
            drawList, origin, scale, {360.0f, 220.0f}, 35.0f, 18.0f,
            state.cX, state.cY, IM_COL32(236, 198, 67, 255), drawOutline, 24);
    }

    draw_dpad(drawList, origin, scale, state, drawOutline);

    draw_round_button(
        drawList, origin, scale, {444.0f, 145.0f}, 30.0f, state.a,
        IM_COL32(39, 91, 61, 255), IM_COL32(48, 210, 102, 255), "A", drawOutline);
    draw_round_button(
        drawList, origin, scale, {397.0f, 181.0f}, 17.0f, state.b,
        IM_COL32(100, 42, 47, 255), IM_COL32(228, 59, 65, 255), "B", drawOutline);
    draw_ellipse_button(
        drawList, origin, scale, {493.0f, 150.0f}, {13.0f, 23.0f}, state.x, "X", drawOutline);
    draw_ellipse_button(
        drawList, origin, scale, {445.0f, 100.0f}, {23.0f, 13.0f}, state.y, "Y", drawOutline);

    const ImVec2 startCenter = viewer_point(origin, scale, 300.0f, 126.0f);
    if (drawOutline || state.start) {
        drawList->AddEllipseFilled(
            startCenter, viewer_radius(scale, state.start ? 28.0f : 27.0f, state.start ? 11.0f : 10.0f),
            state.start ? IM_COL32(255, 226, 120, 255) : kGold, 32);
    }
    drawList->AddEllipseFilled(
        startCenter, viewer_radius(scale, 24.0f, 7.0f),
        state.start ? IM_COL32(246, 247, 249, 255) : IM_COL32(75, 78, 84, 255), 32);
    draw_centered_text(
        drawList, startCenter, 9.0f * scale, IM_COL32(35, 37, 42, 230), "START");

    draw_triforce(drawList, origin, scale);

    const char* name = controllerName != nullptr && controllerName[0] != '\0'
        ? controllerName
        : "Controller Port 1";
    if (drawBackground) {
        draw_centered_text(
            drawList, viewer_point(origin, scale, 300.0f, 17.0f), 13.0f * scale,
            IM_COL32(236, 222, 174, 245), name);
    }
    return true;
}

void draw_input_viewer_fallback(const InputViewerState& state) {
    ImGui::TextUnformatted("Input Viewer - compatibility view");
    ImGui::Text(
        "L Stick  X %+.2f  Y %+.2f    C Stick  X %+.2f  Y %+.2f",
        state.leftX, state.leftY, state.cX, state.cY);
    ImGui::Text(
        "Buttons  %s%s%s%s%s%s%s",
        state.a ? "A " : "", state.b ? "B " : "", state.x ? "X " : "",
        state.y ? "Y " : "", state.z ? "Z " : "", state.l ? "L " : "",
        state.r ? "R" : "");
    ImGui::ProgressBar(state.leftTrigger, ImVec2(-1.0f, 0.0f), "L trigger");
    ImGui::ProgressBar(state.rightTrigger, ImVec2(-1.0f, 0.0f), "R trigger");
}

void draw_gyro_bar(const char* label, float value) {
    constexpr float kBarScale = 4.0f;
    const float amount = std::min(1.0f, std::fabs(value) / kBarScale);
    char overlay[32];
    std::snprintf(overlay, sizeof(overlay), "%s %+.3f", label, value);
    ImGui::ProgressBar(amount, ImVec2(-1.0f, 0.0f), overlay);
}

}  // namespace

void ImGuiMenuTools::ShowInputViewer() {
    auto& settings = getSettings().game;
    if (!settings.showInputViewer) {
        return;
    }

    const char* controllerName = PADGetName(PAD_1);
    if (controllerName != nullptr) {
        m_controllerName = controllerName;
    }

    const float uiScale = ImGuiScale();
    const bool resetSize = settings.inputViewerResetSizeRequested.getValue();
    if (resetSize) {
        settings.inputViewerResetSizeRequested.setValue(false);
        config::save();
    }

    const float minimumHeight =
        (settings.showInputViewerGyro ? 365.0f :
         settings.inputViewerAnalogValues ? 260.0f : 220.0f) * uiScale;
    const ImVec2 minimumSize = {360.0f * uiScale, minimumHeight};
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 maximumSize = {
        std::max(minimumSize.x, viewport->WorkSize.x),
        std::max(minimumSize.y, viewport->WorkSize.y),
    };
    ImGui::SetNextWindowSizeConstraints(minimumSize, maximumSize);
    ImGui::SetNextWindowSize(
        {610.0f * uiScale, 350.0f * uiScale},
        resetSize ? ImGuiCond_Always : ImGuiCond_FirstUseEver);

    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (m_inputOverlayCorner != -1) {
        SetOverlayWindowLocation(m_inputOverlayCorner);
        windowFlags |= ImGuiWindowFlags_NoMove;
    }
    if (settings.inputViewerLock) {
        windowFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
    }

    ImGui::SetNextWindowBgAlpha(settings.inputViewerBackground ? 0.82f : 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10.0f * uiScale, 10.0f * uiScale});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f * uiScale);
    ImGui::PushStyleVar(
        ImGuiStyleVar_WindowBorderSize, settings.inputViewerOutline ? 1.0f * uiScale : 0.0f);
    const bool drawWindow = ImGui::Begin("Input Viewer", nullptr, windowFlags);
    ImGui::PopStyleVar(3);

    if (drawWindow) {
        const InputViewerState state =
            sample_input_viewer_state(settings.showInputViewerGyro.getValue());
        const bool showAnalog = settings.inputViewerAnalogValues.getValue();
        const bool showGyro = settings.showInputViewerGyro.getValue();
        const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
        const float frameHeight = ImGui::GetFrameHeightWithSpacing();
        float detailsHeight = 0.0f;
        if (showAnalog) {
            detailsHeight += lineHeight * 2.0f + ImGui::GetStyle().ItemSpacing.y * 2.0f;
        }
        if (showGyro) {
            detailsHeight += lineHeight + frameHeight * 3.0f + ImGui::GetStyle().ItemSpacing.y * 2.0f;
        }
        if (showAnalog || showGyro) {
            detailsHeight += ImGui::GetStyle().ItemSpacing.y + 2.0f;
        }

        const ImVec2 available = ImGui::GetContentRegionAvail();
        const float artHeight = std::max(1.0f, available.y - detailsHeight);
        const float scale = std::min(available.x / kViewerDesignWidth,
                                     artHeight / kViewerDesignHeight);
        const ImVec2 artSize = {kViewerDesignWidth * scale, kViewerDesignHeight * scale};
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const ImVec2 origin = {
            cursor.x + std::max(0.0f, (available.x - artSize.x) * 0.5f),
            cursor.y + std::max(0.0f, (artHeight - artSize.y) * 0.5f),
        };

        if (!draw_primary_input_viewer(
                ImGui::GetWindowDrawList(), origin, scale, state, m_controllerName.c_str(),
                settings.inputViewerBackground.getValue(),
                settings.inputViewerOutline.getValue(),
                settings.inputViewerSticks.getValue()))
        {
            draw_input_viewer_fallback(state);
        } else {
            ImGui::Dummy({std::max(1.0f, available.x), artHeight});
        }

        if (showAnalog) {
            ImGui::Separator();
            ImGui::Text(
                "L Stick  X %+.2f  Y %+.2f     C Stick  X %+.2f  Y %+.2f",
                state.leftX, state.leftY, state.cX, state.cY);
            ImGui::Text(
                "Triggers  L %.2f%s     R %.2f%s",
                state.leftTrigger, state.l ? " (click)" : "",
                state.rightTrigger, state.r ? " (click)" : "");
        }

        if (showGyro) {
            ImGui::Separator();
            ImGui::TextUnformatted(state.gyroAvailable ? "Gyro" : "Gyro (not available)");
            draw_gyro_bar("X", state.gyro[0]);
            draw_gyro_bar("Y", state.gyro[1]);
            draw_gyro_bar("Z", state.gyro[2]);
        }

        if (ImGui::BeginPopupContextWindow()) {
            ImGui::TextDisabled("Input Viewer Layout");
            if (ImGui::BeginMenu("Position")) {
                if (ImGui::MenuItem("Custom", nullptr, m_inputOverlayCorner == -1)) {
                    m_inputOverlayCorner = -1;
                }
                if (ImGui::MenuItem("Top-left", nullptr, m_inputOverlayCorner == 0)) {
                    m_inputOverlayCorner = 0;
                }
                if (ImGui::MenuItem("Top-right", nullptr, m_inputOverlayCorner == 1)) {
                    m_inputOverlayCorner = 1;
                }
                if (ImGui::MenuItem("Bottom-left", nullptr, m_inputOverlayCorner == 2)) {
                    m_inputOverlayCorner = 2;
                }
                if (ImGui::MenuItem("Bottom-right", nullptr, m_inputOverlayCorner == 3)) {
                    m_inputOverlayCorner = 3;
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            input_viewer_menu_item("Background", settings.inputViewerBackground);
            input_viewer_menu_item("Gold outline", settings.inputViewerOutline);
            input_viewer_menu_item("Show sticks", settings.inputViewerSticks);
            input_viewer_menu_item("Show analog values", settings.inputViewerAnalogValues);
            input_viewer_menu_item("Show gyro", settings.showInputViewerGyro);
            input_viewer_menu_item("Lock position and size", settings.inputViewerLock);
            if (ImGui::MenuItem("Reset size")) {
                settings.inputViewerResetSizeRequested.setValue(true);
                config::save();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Hide input viewer")) {
                settings.showInputViewer.setValue(false);
                config::save();
            }
            ImGui::EndPopup();
        }
    }

    ImGui::End();
}

}  // namespace dusk
