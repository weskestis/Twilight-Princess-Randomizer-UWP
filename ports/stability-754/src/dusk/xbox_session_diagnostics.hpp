#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace dusk::xbox_session_diagnostics {
inline constexpr const char* kMarker = "xbox-session-active.txt";
inline constexpr const char* kReport = "xbox-last-session.txt";
inline constexpr const char* kHistory = "xbox-session-history.log";

inline std::string read_bounded(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    std::string text(16 * 1024, '\0');
    input.read(text.data(), static_cast<std::streamsize>(text.size()));
    text.resize(static_cast<size_t>(input.gcount()));
    return text;
}

inline void clear_session(const std::filesystem::path& root) noexcept {
    std::error_code ec;
    std::filesystem::remove(root / kMarker, ec);
}

inline void arm_session(const std::filesystem::path& root) noexcept {
    try {
        std::filesystem::create_directories(root);
        std::ofstream marker(root / kMarker, std::ios::binary | std::ios::trunc);
        marker << "session_state=gameplay_active\nbuild=1.4.1.754\n";
    } catch (...) {}
}

// Called once during process startup, before this process can arm its own session.
// A leftover marker proves only that cleanup was missed, not that startup crashed.
inline void capture_previous_session(const std::filesystem::path& root,
                                     std::string_view lastStage) noexcept {
    try {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(root / kMarker, ec)) return;
        std::string report = "The previous session ended without a recorded clean exit.\n"
            "This can happen when the app is closed or interrupted; it does not establish a crash.\n\n"
            "Last recorded checkpoint: ";
        report += lastStage.empty() ? "Unavailable" : std::string(lastStage.substr(0, 768));
        report += "\nBuild recording this report: 1.4.1.754\n";
        {
            std::ofstream latest(root / kReport, std::ios::binary | std::ios::trunc);
            latest << report;
            latest.flush();
            if (!latest.good()) return;
        }
        // Bounded history, retaining one previous chunk for diagnosis.
        const auto history = root / kHistory;
        const auto bytes = std::filesystem::file_size(history, ec);
        if (!ec && bytes > 256 * 1024) {
            auto old = history; old += ".old";
            std::filesystem::remove(old, ec);
            ec.clear();
            std::filesystem::rename(history, old, ec);
        }
        std::ofstream log(history, std::ios::binary | std::ios::app);
        log << "\n=== Unclean exit ===\n" << report;
        clear_session(root);
    } catch (...) {}
}

inline std::string report_for_ui(const std::filesystem::path& root) noexcept {
    try {
        auto text = read_bounded(root / kReport);
        if (text.empty()) text = "No previous unclean exit has been recorded.\n";
        const auto failure = read_bounded(root / "xbox-runtime-failure.txt");
        if (!failure.empty()) text += "\nLast saved runtime report:\n" + failure;
        return text;
    } catch (...) {
        return "The diagnostic report could not be read.";
    }
}
} // namespace dusk::xbox_session_diagnostics
