#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

using s32 = int32_t;
using BOOL = int;
constexpr BOOL TRUE = 1, FALSE = 0;
constexpr s32 CARD_RESULT_READY = 0, CARD_RESULT_NOCARD = -3, CARD_RESULT_FATAL_ERROR = -128;
enum CARDFileType { CARD_RAWIMAGE, CARD_GCIFOLDER };
CARDFileType SelectedFileType = CARD_GCIFOLDER;
bool Initialized = false;
bool failSdlDirectories = false;
int directoryCalls = 0, fallbackWarnings = 0;
std::array<std::filesystem::path, 2> cardPaths;
struct Logger {
    template<class... T> void info(const char*, const T&...) {}
    template<class... T> void warn(const char* text, const T&...) {
        if (std::string(text).find("with native filesystem") != std::string::npos) ++fallbackWarnings;
    }
    template<class... T> void error(const char*, const T&...) {}
    template<class... T> void fatal(const char*, const T&...) { throw std::runtime_error("fatal"); }
} Log;
const char* SDL_GetError() { return "injected SDL directory failure"; }
namespace aurora {
const char* g_gameName = "GZ2E";
struct { const char* userPath = nullptr; } g_config;
namespace io {
std::filesystem::path fs_path_from_string(const char* p) {
    const auto* first = reinterpret_cast<const char8_t*>(p);
    return std::filesystem::path{std::u8string(first, first + std::strlen(p))};
}
std::string fs_path_to_string(const std::filesystem::path& path) {
    const auto utf8 = path.u8string();
    return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
}
bool create_directories(const std::filesystem::path& path) {
    ++directoryCalls;
    if (failSdlDirectories) return false;
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    return !ec && std::filesystem::is_directory(path);
}
}
namespace card {
constexpr uint32_t BlockSize = 8192;
enum class ECardResult { READY = 0, NOCARD = -3, IOERROR = -5 };
enum class ECardSlot { SlotA, SlotB };
enum class ECardSize { Card2043Mb = 128 };
enum class EEncoding { ASCII };
struct ProbeResults { ECardResult x0_error; uint32_t x4_cardSize, x8_sectorSize; };
struct CardGciFolder {
    std::filesystem::path m_folderPath;
    ECardResult m_error = ECardResult::NOCARD;
    EEncoding m_encoding = EEncoding::ASCII;
    void InitCard(const char*, const char*) {}
    bool open(const std::filesystem::path& path) {
        m_folderPath = path;
        m_error = std::filesystem::is_directory(path) ? ECardResult::READY : ECardResult::NOCARD;
        return m_error == ECardResult::READY;
    }
    void close() { m_folderPath.clear(); m_error = ECardResult::NOCARD; }
    ECardResult getError() const { return m_error; }
    ECardResult format(ECardSlot, ECardSize = ECardSize::Card2043Mb, EEncoding = EEncoding::ASCII);
    ProbeResults probeCardFile(const std::filesystem::path&);
};
struct CardRawFile : CardGciFolder {};
}
}
std::array<std::unique_ptr<aurora::card::CardGciFolder>, 2> CardChannels;
#define GET_CARD(slot) CardChannels[slot]
#define CARD_READY(slot) true
CARDFileType selected_card_type() { return SelectedFileType; }
std::filesystem::path get_card_full_path(const std::filesystem::path& root, const char*, CARDFileType,
                                       aurora::card::ECardSlot slot) {
    return root / "USA" / (slot == aurora::card::ECardSlot::SlotA ? "Card A" : "Card B");
}
#include "production-slot-a.inc"

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
static void initialize(const std::filesystem::path& root, bool failure) {
    Initialized = false;
    cardPaths = {};
    failSdlDirectories = failure;
    directoryCalls = fallbackWarnings = 0;
    std::filesystem::create_directories(root);
    const auto utf8 = root.u8string();
    CARDSetBasePath(reinterpret_cast<const char*>(utf8.c_str()), -1);
    CARDInit("GZ2E", "01");
}
static bool game_sees_slot_a() {
    s32 size = 0, sector = 0;
    return CARDProbe(0) && CARDProbeEx(0, &size, &sector) == CARD_RESULT_READY &&
           size > 0 && sector == 8192 && CARDCheck(0) == CARD_RESULT_READY;
}
int main(int argc, char** argv) {
    try {
        require(argc >= 2, "workdir missing");
        const std::filesystem::path root = argv[1];
        initialize(root / "ordinary", false);
        require(game_sees_slot_a() && directoryCalls == 1 && fallbackWarnings == 0,
                "Normal directory backend failed to create a card the game can probe");
        const auto unicode = root / std::filesystem::path(u8"saves_雪") / "nested";
        initialize(unicode, true);
        if (argc > 2) {
            s32 size = -1, sector = -1;
            return !CARDProbe(0) && CARDProbeEx(0, &size, &sector) == CARD_RESULT_NOCARD &&
                   CARDCheck(0) == CARD_RESULT_NOCARD && size == 0 && sector == 0 ? 42 : 1;
        }
        require(game_sees_slot_a() && fallbackWarnings == 1,
                "SDL directory failure leaves the game's actual Slot A probe unavailable");
        const auto a = unicode / "USA/Card A";
        { std::ofstream file(a / "existing-save.gci", std::ios::binary); file << "KEEP_VANILLA_SAVE"; }
        initialize(unicode, true);
        require(game_sees_slot_a() && directoryCalls == 0, "Existing card was provisioned again");
        std::string saved;
        { std::ifstream file(a / "existing-save.gci", std::ios::binary); file >> saved; }
        require(saved == "KEEP_VANILLA_SAVE", "Existing vanilla save was changed");
        std::filesystem::create_directories(root / "onlyB/USA/Card B");
        initialize(root / "onlyB", true);
        require(game_sees_slot_a(), "Existing Slot B still prevents provisioning Slot A");
        const auto obstruction = root / "blocked";
        { std::ofstream file(obstruction, std::ios::binary); file << "KEEP_OBSTRUCTION"; }
        aurora::card::CardGciFolder card;
        card.open(obstruction);
        require(card.format(aurora::card::ECardSlot::SlotA) == aurora::card::ECardResult::IOERROR &&
                card.getError() == aurora::card::ECardResult::IOERROR,
                "Failed directory fallback pretends the card is usable");
        { std::ifstream file(obstruction, std::ios::binary); file >> saved; }
        require(saved == "KEEP_OBSTRUCTION", "Fallback replaced an existing file");
        std::puts("PASS: actual CARDInit/GCI format/CARDProbe/CARDProbeEx/CARDCheck; directory failure recovery, Unicode, Slot B, existing save and failed fallback preservation");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
