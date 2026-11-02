#pragma once

#include "dusk/xbox_session_diagnostics.hpp"

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>

namespace dusk::startup_guard {

#if defined(_UWP)
namespace detail {
inline std::filesystem::path markerPath;
inline std::unique_ptr<std::fstream> markerStream;
inline std::string previousStage;
inline bool initialized = false;
inline bool startupActive = false;
inline bool activityActive = false;
inline bool activityAwaitingFrame = false;
inline unsigned int activityFramesRemaining = 0;
inline bool recoveryMode = false;
inline std::atomic_bool traceEnabled{false};
inline std::mutex mutex;

// Randomizer activation can complete and present one game frame before an Xbox-only hook is first
// exercised. Keep the journal alive across a small, bounded set of subsequent frames so a freeze
// in early game execution is not mistaken for a successful handoff. The fixed stream keeps this
// bounded trace cheap compared with reopening the LocalState file for every checkpoint.
inline constexpr unsigned int kActivityTraceFrames = 240;

inline std::string clean_stage(std::string_view stage) {
    std::string value{stage.substr(0, 768)};
    for (char& ch : value) {
        if (ch == '\r' || ch == '\n') {
            ch = ' ';
        }
    }
    return value;
}

inline void write_locked(std::string_view stage) noexcept {
    if (markerPath.empty()) {
        return;
    }
    try {
        if (!markerStream) {
            markerStream = std::make_unique<std::fstream>(
                markerPath, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
        }
        if (!*markerStream) {
            markerStream.reset();
            return;
        }

        // Hook activation records hundreds of stages. Reopening the LocalState file for every
        // stage can stall Xbox for minutes, so keep one handle and overwrite a fixed-size record.
        // The newline keeps the existing getline reader simple; padding removes stale text from a
        // longer previous stage without another truncate/open cycle.
        constexpr std::size_t kRecordSize = 1024;
        const std::string value = clean_stage(stage);
        markerStream->clear();
        markerStream->seekp(0);
        markerStream->write(value.data(), static_cast<std::streamsize>(value.size()));
        markerStream->put('\n');
        const std::string padding(kRecordSize - value.size() - 1, ' ');
        markerStream->write(padding.data(), static_cast<std::streamsize>(padding.size()));
        markerStream->flush();
    } catch (...) {
        // The guard must never turn a diagnostic write failure into a startup failure.
        markerStream.reset();
    }
}

inline void clear_locked() noexcept {
    if (markerPath.empty()) {
        return;
    }
    markerStream.reset();
    std::error_code ec;
    std::filesystem::remove(markerPath, ec);
}
}  // namespace detail

inline void begin_startup(const std::filesystem::path& userPath) noexcept {
    std::scoped_lock lock(detail::mutex);
    if (detail::initialized) {
        return;
    }
    detail::initialized = true;
    // Use a build-specific journal so a marker from an older build never changes this launch.
    detail::markerPath = userPath / "xbox-startup-stage-749.txt";
    try {
        std::ifstream input(detail::markerPath, std::ios::binary);
        if (input) {
            std::getline(input, detail::previousStage);
        } else {
            // Carry forward the latest pre-upgrade crash checkpoint.
            for (const char* journal : {"xbox-startup-stage-748.txt", "xbox-startup-stage-747.txt", "xbox-startup-stage-746.txt", "xbox-startup-stage-745.txt", "xbox-startup-stage-744.txt", "xbox-startup-stage-743.txt", "xbox-startup-stage-742.txt", "xbox-startup-stage-741.txt", "xbox-startup-stage-740.txt", "xbox-startup-stage-739.txt", "xbox-startup-stage-738.txt", "xbox-startup-stage-737.txt", "xbox-startup-stage-736.txt", "xbox-startup-stage-735.txt", "xbox-startup-stage-734.txt", "xbox-startup-stage-733.txt", "xbox-startup-stage-732.txt"}) {
                std::ifstream prior(userPath / journal, std::ios::binary);
                if (prior) {
                    std::getline(prior, detail::previousStage);
                    break;
                }
            }
        }
    } catch (...) {
        detail::previousStage.clear();
    }
    xbox_session_diagnostics::capture_previous_session(userPath, detail::previousStage);
    // A recovery UI is not safe on Xbox: both the modal and toast paths can terminate before the
    // first stable frame. Preserve the previous stage for the log, but always continue through the
    // instrumented startup path so the next marker identifies the real failing boundary.
    detail::recoveryMode = false;
    detail::startupActive = true;
    detail::traceEnabled.store(true, std::memory_order_relaxed);
    detail::write_locked("startup.data-ready");
}

inline bool recovery_mode() noexcept {
    std::scoped_lock lock(detail::mutex);
    return detail::recoveryMode;
}

inline bool startup_active() noexcept {
    std::scoped_lock lock(detail::mutex);
    return detail::startupActive;
}

inline bool activity_pending() noexcept {
    std::scoped_lock lock(detail::mutex);
    return detail::activityActive;
}

inline std::string previous_stage() noexcept {
    std::scoped_lock lock(detail::mutex);
    return detail::previousStage;
}
inline void stage(std::string_view value) noexcept {
    if (!detail::traceEnabled.load(std::memory_order_relaxed)) {
        return;
    }
    std::scoped_lock lock(detail::mutex);
    if ((detail::startupActive || detail::activityActive || detail::activityAwaitingFrame) &&
        !detail::recoveryMode)
    {
        detail::write_locked(value);
    }
}

inline void begin_activity(std::string_view value) noexcept {
    std::scoped_lock lock(detail::mutex);
    detail::traceEnabled.store(true, std::memory_order_relaxed);
    detail::activityActive = true;
    detail::activityAwaitingFrame = false;
    detail::activityFramesRemaining = 0;
    if (!detail::recoveryMode) {
        detail::write_locked(value);
    }
}

inline void end_activity(std::string_view resumeStage = "activity.complete") noexcept {
    std::scoped_lock lock(detail::mutex);
    detail::activityActive = false;
    detail::activityAwaitingFrame = true;
    detail::activityFramesRemaining = detail::kActivityTraceFrames;
    if (!detail::recoveryMode) {
        detail::write_locked(resumeStage);
    }
}

inline void keep_activity_alive() noexcept {
    std::scoped_lock lock(detail::mutex);
    detail::traceEnabled.store(true, std::memory_order_relaxed);
    detail::activityAwaitingFrame = true;
    detail::activityFramesRemaining = detail::kActivityTraceFrames;
}

inline void complete_startup() noexcept {
    std::scoped_lock lock(detail::mutex);
    if (!detail::startupActive && !detail::activityAwaitingFrame) {
        return;
    }
    detail::startupActive = false;
    detail::recoveryMode = false;
    if (!detail::activityActive) {
        if (detail::activityAwaitingFrame && detail::activityFramesRemaining > 1) {
            --detail::activityFramesRemaining;
            return;
        }
        detail::activityAwaitingFrame = false;
        detail::activityFramesRemaining = 0;
        detail::clear_locked();
        detail::traceEnabled.store(false, std::memory_order_relaxed);
    }
}

#else

inline void begin_startup(const std::filesystem::path&) noexcept {}
inline bool recovery_mode() noexcept { return false; }
inline bool startup_active() noexcept { return false; }
inline bool activity_pending() noexcept { return false; }
inline std::string previous_stage() noexcept { return {}; }
inline void stage(std::string_view) noexcept {}
inline void begin_activity(std::string_view) noexcept {}
inline void end_activity(std::string_view = {}) noexcept {}
inline void keep_activity_alive() noexcept {}
inline void complete_startup() noexcept {}

#endif

}  // namespace dusk::startup_guard
