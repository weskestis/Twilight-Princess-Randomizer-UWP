#include "PureItemTracker.hpp"

#include "dusk/config.hpp"
#include "dusk/main.h"
#include "dusk/settings.h"
#include "dusk/ui/icon_provider.hpp"
#include "m_Do/m_Do_controller_pad.h"

#include <aurora/imgui.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>

namespace dusk {

#if defined(_UWP)
extern "C" bool tpr_xbox_transition_failure_pending() noexcept;
extern "C" const char* tpr_xbox_transition_failure_report() noexcept;
extern "C" void tpr_xbox_transition_failure_acknowledge() noexcept;
extern "C" int tpr_xbox_transition_retry_recovery() noexcept;
extern "C" bool tpr_pure_tracker_active() noexcept;
extern "C" std::size_t tpr_pure_tracker_slot_count() noexcept;
extern "C" const char* tpr_pure_tracker_slot_label(std::size_t index) noexcept;
extern "C" uint8_t tpr_pure_tracker_slot_icon(std::size_t index) noexcept;
extern "C" int tpr_pure_tracker_slot_max(std::size_t index) noexcept;
extern "C" int tpr_pure_tracker_slot_owned(std::size_t index) noexcept;

namespace {
ImTextureID tracker_texture(uint8_t itemNo) {
    static std::unordered_map<uint8_t, ImTextureID> textures;
    static int lastUploadFrame = -1;
    if (const auto found = textures.find(itemNo); found != textures.end()) {
        return found->second;
    }

    // Upload at most one previously unseen tracker icon per rendered frame.
    // The old all-at-once path could hammer Xbox during the opening scene.
    const int frame = ImGui::GetFrameCount();
    if (lastUploadFrame == frame) {
        return {};
    }
    lastUploadFrame = frame;

    auto pixels = ui::render_item_icon_pixels(itemNo);
    if (!pixels || pixels->rgba8.empty() || pixels->width == 0 || pixels->height == 0) {
        return {};
    }
    const ImTextureID texture =
        aurora_imgui_add_texture(pixels->width, pixels->height, pixels->rgba8.data());
    if (texture != ImTextureID{}) {
        textures.emplace(itemNo, texture);
    }
    return texture;
}
std::string runtime_report_field(std::string_view report, std::string_view key) {
    const std::string prefix = std::string(key) + "=";
    const std::size_t start = report.find(prefix);
    if (start == std::string_view::npos) {
        return {};
    }
    const std::size_t valueStart = start + prefix.size();
    const std::size_t end = report.find('\n', valueStart);
    return std::string(report.substr(
        valueStart,
        end == std::string_view::npos ? report.size() - valueStart : end - valueStart));
}

std::string persist_transition_failure_report(std::string_view report) {
    if (report.empty()) {
        return {};
    }

    try {
        std::filesystem::create_directories(ConfigPath);
        const auto latestPath = ConfigPath / "xbox-runtime-failure.txt";
        const auto historyPath = ConfigPath / "xbox-runtime-failures.log";

        {
            std::ofstream latest(latestPath, std::ios::out | std::ios::trunc);
            latest << report;
        }
        {
            std::ofstream history(historyPath, std::ios::out | std::ios::app);
            history << "\n===== runtime failure =====\n" << report;
        }
        return latestPath.string();
    } catch (...) {
        return {};
    }
}
}  // namespace
#endif

void draw_xbox_failure_report() {
#if !defined(_UWP)
    return;
#else
    static bool cursorOverrideActive = false;
    static bool previousMouseDrawCursor = false;
    static bool popupPointerInitialized = false;
    static float popupPointerX = 0.0f;
    static float popupPointerY = 0.0f;
    static int lastRecoveryResult = -1;

    if (!tpr_xbox_transition_failure_pending()) {
        if (cursorOverrideActive) {
            ImGui::GetIO().MouseDrawCursor = previousMouseDrawCursor;
            cursorOverrideActive = false;
        }
        return;
    }

    if (!cursorOverrideActive) {
        previousMouseDrawCursor = ImGui::GetIO().MouseDrawCursor;
        ImGui::GetIO().MouseDrawCursor = true;
        cursorOverrideActive = true;
    }

    const char* rawReport = tpr_xbox_transition_failure_report();
    const std::string report = rawReport != nullptr ? rawReport : "";
    static std::string lastPersistedReport;
    static std::string persistedPath;
    if (report != lastPersistedReport) {
        persistedPath = persist_transition_failure_report(report);
        lastPersistedReport = report;
    }

    // This modal is ImGui, while the normal controller cursor targets RmlUi.
    // Drive an ImGui pointer directly from the right stick while the report is open.
    auto& io = ImGui::GetIO();
    const ImVec2 displaySize = io.DisplaySize;
    if (!popupPointerInitialized) {
        popupPointerX = displaySize.x * 0.5f;
        popupPointerY = displaySize.y * 0.5f;
        popupPointerInitialized = true;
    }
    const float pointerDt = std::clamp(io.DeltaTime, 0.0f, 0.05f);
    const float pointerSpeed = 1050.0f;
    const float stickX = mDoCPd_c::getSubStickX(PAD_1);
    const float stickY = mDoCPd_c::getSubStickY(PAD_1);
    if (std::abs(stickX) > 0.15f) {
        popupPointerX += stickX * pointerSpeed * pointerDt;
    }
    if (std::abs(stickY) > 0.15f) {
        popupPointerY -= stickY * pointerSpeed * pointerDt;
    }
    popupPointerX = std::clamp(popupPointerX, 0.0f, std::max(0.0f, displaySize.x - 1.0f));
    popupPointerY = std::clamp(popupPointerY, 0.0f, std::max(0.0f, displaySize.y - 1.0f));
    io.MousePos = ImVec2(popupPointerX, popupPointerY);
    io.MouseDown[0] = mDoCPd_c::getHoldA(PAD_1) != 0;
    io.MouseDrawCursor = true;

    const bool retryPressed = mDoCPd_c::getTrigA(PAD_1) != 0;
    const bool dismissPressed = mDoCPd_c::getTrigB(PAD_1) != 0;

    ImGui::OpenPopup("Xbox Runtime Failure###XboxTransitionFailure");
    ImGui::SetNextWindowSize(ImVec2(720.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(
            "Xbox Runtime Failure###XboxTransitionFailure",
            nullptr,
            ImGuiWindowFlags_AlwaysAutoResize))
    {
        const std::string failureClass = runtime_report_field(report, "failure_class");
        const std::string failureSummary = runtime_report_field(report, "failure_summary");
        const std::string failureHint = runtime_report_field(report, "failure_hint");

        ImGui::TextWrapped(
            "Xbox detected a Randomizer runtime failure. The state below was captured before "
            "recovery so the next fix can target the actual blocker.");
        if (!failureClass.empty()) {
            ImGui::TextWrapped("Failure class: %s", failureClass.c_str());
        }
        if (!failureSummary.empty()) {
            ImGui::TextWrapped("Detected: %s", failureSummary.c_str());
        }
        if (!failureHint.empty()) {
            ImGui::TextWrapped("Hint: %s", failureHint.c_str());
        }
        ImGui::Spacing();
        ImGui::TextUnformatted("Controller: A = Retry Recovery    B = Dismiss");

        if (!persistedPath.empty()) {
            ImGui::TextWrapped("Saved report: %s", persistedPath.c_str());
        } else {
            ImGui::TextWrapped(
                "The report could not be written to disk, but the captured data is shown below.");
        }

        ImGui::Separator();
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 660.0f);
        ImGui::TextUnformatted(report.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Separator();

        const bool retryButton = ImGui::Button("Retry Recovery");
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        const bool dismissButton = ImGui::Button("Dismiss");
        const bool dismissHovered = ImGui::IsItemHovered();

        if (lastRecoveryResult >= 0) {
            ImGui::SameLine();
            ImGui::TextUnformatted(
                lastRecoveryResult != 0 ?
                    "Recovery state cleared - press B to close after verifying the scene" :
                    "Recovery still pending - report remains armed");
        }

        if (retryButton || (retryPressed && !dismissHovered)) {
            lastRecoveryResult = tpr_xbox_transition_retry_recovery();
            // Deliberately keep the report open even after an internally successful
            // recovery. The user can verify that gameplay is actually visible before
            // dismissing it, and a still-black result remains diagnosable.
        }
        if (dismissButton || dismissPressed || (retryPressed && dismissHovered)) {
            tpr_xbox_transition_failure_acknowledge();
            ImGui::CloseCurrentPopup();
            ImGui::GetIO().MouseDrawCursor = previousMouseDrawCursor;
            cursorOverrideActive = false;
        }

        ImGui::EndPopup();
    }
#endif
}

void draw_pure_item_tracker() {
#if !defined(_UWP)
    return;
#else
    auto& trackerSettings = getSettings().game;
    if (!trackerSettings.showPureItemTracker.getValue() || !tpr_pure_tracker_active()) {
        return;
    }

    const std::size_t slotCount = tpr_pure_tracker_slot_count();
    if (slotCount == 0) {
        return;
    }

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoScrollWithMouse |
                             ImGuiWindowFlags_NoBackground;
    if (trackerSettings.pureItemTrackerLock.getValue()) {
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
    }

    const ImVec2 trackerDesignSize(440.0f, 500.0f);
    const ImVec2 workSize = ImGui::GetMainViewport()->WorkSize;
    const ImVec2 workPosition = ImGui::GetMainViewport()->WorkPos;
    const ImVec2 trackerMinimumSize(
        std::min(300.0f, workSize.x * 0.90f),
        std::min(300.0f, workSize.y * 0.90f));
    const ImVec2 trackerDefaultSize(
        std::min(trackerDesignSize.x, workSize.x * 0.92f),
        std::min(trackerDesignSize.y, workSize.y * 0.92f));
    const ImVec2 trackerDefaultPosition(
        workPosition.x + std::max(12.0f, workSize.x * 0.025f),
        workPosition.y + std::max(12.0f, workSize.y * 0.025f));

    if (trackerSettings.pureItemTrackerResetLayoutRequested.getValue()) {
        trackerSettings.pureItemTrackerResetLayoutRequested.setValue(false);
        ImGui::SetNextWindowPos(trackerDefaultPosition, ImGuiCond_Always);
        ImGui::SetNextWindowSize(trackerDefaultSize, ImGuiCond_Always);
        config::save();
    } else {
        ImGui::SetNextWindowPos(trackerDefaultPosition, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(trackerDefaultSize, ImGuiCond_FirstUseEver);
    }

    ImGui::SetNextWindowSizeConstraints(
        trackerMinimumSize,
        ImVec2(std::max(trackerMinimumSize.x, workSize.x),
               std::max(trackerMinimumSize.y, workSize.y)));
    ImGui::SetNextWindowBgAlpha(0.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ResizeGrip, ImVec4(1.0f, 1.0f, 1.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ResizeGripHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.45f));
    ImGui::PushStyleColor(ImGuiCol_ResizeGripActive, ImVec4(1.0f, 1.0f, 1.0f, 0.75f));

    if (!ImGui::Begin("Pure Item Tracker###PureItemTrackerV3", nullptr, flags)) {
        ImGui::End();
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(3);
        return;
    }

    const float trackerScale =
        std::clamp(trackerSettings.pureItemTrackerScale.getValue(), 0.75f, 1.50f);
    ImGui::SetWindowFontScale(trackerScale);

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float availableWidth = std::max(1.0f, available.x);
    const float availableHeight = std::max(1.0f, available.y);
    const int columns = std::clamp(trackerSettings.pureItemTrackerColumns.getValue(), 3, 9);
    const int rows = static_cast<int>(
        (slotCount + static_cast<std::size_t>(columns) - 1) /
        static_cast<std::size_t>(columns));
    const float configuredIconSize = static_cast<float>(
        std::clamp(trackerSettings.pureItemTrackerIconSize.getValue(), 32, 112));
    const float spacing = std::max(2.0f, 7.0f * trackerScale);
    const float countLineHeight = ImGui::GetTextLineHeight();
    const float desiredIconSize = configuredIconSize * trackerScale;
    const float widthFit =
        (availableWidth - spacing * static_cast<float>(columns - 1)) /
        static_cast<float>(columns);
    const float heightFit =
        ((availableHeight - spacing * static_cast<float>(rows - 1)) /
         static_cast<float>(rows)) - countLineHeight;
    const float iconSize =
        std::max(24.0f, std::min({desiredIconSize, widthFit, heightFit}));
    const float gridWidth =
        iconSize * static_cast<float>(columns) +
        spacing * static_cast<float>(columns - 1);
    const float rowStartX =
        ImGui::GetCursorPosX() + std::max(0.0f, (availableWidth - gridWidth) * 0.5f);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, spacing));
    for (std::size_t index = 0; index < slotCount; ++index) {
        const int maximum = std::max(1, tpr_pure_tracker_slot_max(index));
        const int count = std::clamp(tpr_pure_tracker_slot_owned(index), 0, maximum);
        const bool owned = count > 0;

        ImGui::PushID(static_cast<int>(index));
        if (index % static_cast<std::size_t>(columns) == 0) {
            ImGui::SetCursorPosX(rowStartX);
        }

        ImGui::BeginGroup();
        const ImVec2 topLeft = ImGui::GetCursorScreenPos();
        const ImTextureID texture = tracker_texture(tpr_pure_tracker_slot_icon(index));
        const ImVec4 tint =
            owned ? ImVec4(1, 1, 1, 1) : ImVec4(1.0f, 1.0f, 1.0f, 0.20f);
        auto* drawList = ImGui::GetWindowDrawList();
        if (texture != ImTextureID{}) {
            drawList->AddImage(
                texture, topLeft,
                ImVec2(topLeft.x + iconSize, topLeft.y + iconSize),
                ImVec2(0, 0), ImVec2(1, 1),
                ImGui::ColorConvertFloat4ToU32(tint));
        }

        ImGui::Dummy(ImVec2(iconSize, iconSize));
        const bool hovered = ImGui::IsItemHovered();
        ImGui::SetCursorScreenPos(ImVec2(topLeft.x, topLeft.y + iconSize));

        if (maximum > 1 && count > 0) {
            const std::string countText = std::to_string(count);
            const ImVec2 textSize = ImGui::CalcTextSize(countText.c_str());
            const ImVec2 textPosition(
                topLeft.x + std::max(0.0f, (iconSize - textSize.x) * 0.5f),
                topLeft.y + iconSize - 1.0f);
            drawList->AddText(
                ImVec2(textPosition.x + 1.0f, textPosition.y + 1.0f),
                IM_COL32(0, 0, 0, 235), countText.c_str());
            drawList->AddText(
                textPosition, IM_COL32(255, 255, 255, 255), countText.c_str());
        }

        ImGui::Dummy(ImVec2(iconSize, countLineHeight));
        ImGui::EndGroup();

        if (hovered) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(tpr_pure_tracker_slot_label(index));
            ImGui::EndTooltip();
        }

        ImGui::PopID();
        if ((index + 1) % static_cast<std::size_t>(columns) != 0 &&
            index + 1 < slotCount)
        {
            ImGui::SameLine(0.0f, spacing);
        }
    }
    ImGui::PopStyleVar();

    ImGui::End();
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(3);
#endif
}

}  // namespace dusk
