#include "mod_texture_provider.hpp"

#include "dusk/mod_loader.hpp"
#include "dusk/startup_guard.hpp"
#include "runtime_image.hpp"

#include <fmt/format.h>

namespace dusk::ui {

std::string mod_image_source(const mods::LoadedMod& mod, std::string_view bundlePath) {
    return fmt::format("mod://{}/{}?rev={}", mod.metadata.id, bundlePath, mod.cacheGeneration);
}

}  // namespace dusk::ui

#ifdef AURORA_ENABLE_RMLUI

#include <RmlUi/Core.h>
#include <aurora/rmlui.hpp>
#include <borealis/log.hpp>

#include <algorithm>
#include <cmath>
#include <chrono>
#include <ranges>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "dusk/mods/loader/loader.hpp"
#include "dusk/utilities.hpp"

namespace dusk::ui {
namespace {

constexpr borealis::Log Log{"dusk::ui::modTexture"};
constexpr std::string_view kScheme = "mod";
constexpr std::string_view kSourcePrefix = "mod://";
// 2.0.3's short eviction grace can temporarily double these soft limits.
// Keep the existing Xbox hard ceiling of 256 images / 16 MiB.
constexpr size_t kMaxCachedImages = 128;
constexpr size_t kMaxCachedImageBytes = 8 * 1024 * 1024;
constexpr size_t kMaxImageFileSize = 16 * 1024 * 1024;
constexpr uint32_t kMaxModIconDimension = 128;
constexpr uint32_t kMaxModPreviewDimension = 768;

// Bound the temporary decoded image before libpng allocates the full RGBA buffer.
// The author's 2941px icons fit, while a 4096px preview cannot consume 64 MiB
// in addition to cached CPU images and the renderer's GPU textures.
constexpr uint64_t kMaxDecodedModImageBytes = 36ull * 1024 * 1024;
bool bounded_mod_png(std::span<const u8> data) noexcept {
    constexpr uint8_t signature[] = {137,80,78,71,13,10,26,10};
    if (data.size() < 33 || !std::equal(std::begin(signature), std::end(signature), data.begin()) ||
        data[8] != 0 || data[9] != 0 || data[10] != 0 || data[11] != 13 ||
        data[12] != 'I' || data[13] != 'H' || data[14] != 'D' || data[15] != 'R') {
        return false;
    }
    const auto read32 = [&](size_t offset) {
        return (uint32_t(data[offset]) << 24) | (uint32_t(data[offset+1]) << 16) |
               (uint32_t(data[offset+2]) << 8) | uint32_t(data[offset+3]);
    };
    const uint32_t width = read32(16), height = read32(20);
    return width != 0 && height != 0 && width <= 4096 && height <= 4096 &&
           uint64_t(width) * height * 4 <= kMaxDecodedModImageBytes;
}

void fit_mod_image(DecodedImage& image, uint32_t limit) {
    const uint32_t largest = std::max(image.width, image.height);
    if (largest <= limit) {
        return;
    }
    const uint32_t width = std::max(1u, static_cast<uint32_t>(
        static_cast<uint64_t>(image.width) * limit / largest));
    const uint32_t height = std::max(1u, static_cast<uint32_t>(
        static_cast<uint64_t>(image.height) * limit / largest));
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
    for (uint32_t y = 0; y < height; ++y) {
        const float sy = std::clamp((y + 0.5f) * image.height / height - 0.5f,
                                   0.0f, static_cast<float>(image.height - 1));
        const uint32_t y0 = static_cast<uint32_t>(sy);
        const uint32_t y1 = std::min(y0 + 1, image.height - 1);
        const float fy = sy - y0;
        for (uint32_t x = 0; x < width; ++x) {
            const float sx = std::clamp((x + 0.5f) * image.width / width - 0.5f,
                                       0.0f, static_cast<float>(image.width - 1));
            const uint32_t x0 = static_cast<uint32_t>(sx);
            const uint32_t x1 = std::min(x0 + 1, image.width - 1);
            const float fx = sx - x0;
            for (size_t channel = 0; channel < 4; ++channel) {
                const auto sample = [&](uint32_t row, uint32_t column) {
                    return image.pixels[(static_cast<size_t>(row) * image.width + column) * 4 + channel];
                };
                const float top = sample(y0, x0) * (1.0f - fx) + sample(y0, x1) * fx;
                const float bottom = sample(y1, x0) * (1.0f - fx) + sample(y1, x1) * fx;
                pixels[(static_cast<size_t>(y) * width + x) * 4 + channel] =
                    static_cast<uint8_t>(std::lround(top * (1.0f - fy) + bottom * fy));
            }
        }
    }
    image.width = width;
    image.height = height;
    image.pixels = std::move(pixels);
}
constexpr auto kEvictionGrace = std::chrono::seconds{2};

struct CachedImage : DecodedImage {
    std::chrono::steady_clock::time_point lastLoaded;
};

std::unordered_map<std::string, CachedImage>& image_cache() {
    static std::unordered_map<std::string, CachedImage> cache;
    return cache;
}

struct ImageUse { uint64_t age = 0; bool icon = false; };
std::unordered_map<std::string, ImageUse>& image_use() {
    static std::unordered_map<std::string, ImageUse> uses;
    return uses;
}
uint64_t& image_age() { static uint64_t age = 0; return age; }

std::string prepared_image_path(std::string_view path) {
    // A deterministic, flat UTF-8 encoding avoids filename and slash collisions.
    constexpr char hex[] = "0123456789abcdef";
    std::string result = "res/uwp-ui-744/";
    for (const unsigned char value : path) {
        result += hex[value >> 4]; result += hex[value & 15];
    }
    return result + ".png";
}

// TODO: move cache logic into a shared location
void make_cache_room(size_t incomingBytes) {
    auto& cache = image_cache();
    size_t cachedBytes = 0;
    for (const auto& cached : cache | std::views::values) {
        cachedBytes += cached.pixels.size();
    }
    const auto overLimit = [&](size_t factor) {
        return cache.size() >= kMaxCachedImages * factor ||
               cachedBytes + incomingBytes > kMaxCachedImageBytes * factor;
    };
    const auto graceStart = std::chrono::steady_clock::now() - kEvictionGrace;
    while (!cache.empty() && overLimit(1)) {
        const auto victim = std::ranges::min_element(cache, [](const auto& a, const auto& b) {
            const auto& uses = image_use();
            const auto ua = uses.find(a.first), ub = uses.find(b.first);
            const bool aIcon = ua != uses.end() && ua->second.icon;
            const bool bIcon = ub != uses.end() && ub->second.icon;
            return aIcon != bIcon ? !aIcon : a.second.lastLoaded < b.second.lastLoaded;
        });
        if (victim->second.lastLoaded > graceStart && !overLimit(2)) {
            break;
        }
        cachedBytes -= victim->second.pixels.size();
        Rml::ReleaseTexture(victim->first);
        image_use().erase(victim->first);
        cache.erase(victim);
    }
}

std::string_view strip_query(std::string_view path) noexcept {
    const auto queryPos = path.find_first_of("?#");
    if (queryPos != std::string_view::npos) {
        path = path.substr(0, queryPos);
    }
    return path;
}

std::optional<DecodedImage> load_mod_image(std::string_view idAndPath, std::string_view source,
                                        bool* isIcon = nullptr) {
    const auto slash = idAndPath.find('/');
    if (slash == std::string_view::npos || slash == 0 || slash + 1 >= idAndPath.size()) {
        Log.warn("Malformed mod image source '{}'", source);
        return std::nullopt;
    }
    const std::string modId{idAndPath.substr(0, slash)};
    const std::string path{idAndPath.substr(slash + 1)};
    if (!utils::is_safe_resource_path(path)) {
        Log.warn("Unsafe path in mod image source '{}'", source);
        return std::nullopt;
    }

    std::shared_ptr<mods::ModBundle> bundle;
    bool icon = false;
    for (const auto& mod : mods::ModLoader::instance().mods()) {
        if (mod.metadata.id == modId) {
            bundle = mod.bundle;
            icon = path == mod.metadata.iconPath;
            break;
        }
    }
    if (bundle == nullptr) {
        Log.warn("Unknown mod in image source '{}'", source);
        return std::nullopt;
    }
    if (isIcon != nullptr) *isIcon = icon;

    std::vector<u8> data;
    try {
        startup_guard::stage(fmt::format("menu.mods.image:{}:{}", modId, path));
        std::string readPath = path;
        const auto prepared = prepared_image_path(path);
        try { if (bundle->file_exists(prepared)) readPath = prepared; } catch (...) {}
        if (bundle->getFileSize(readPath) > kMaxImageFileSize) {
            Log.warn("Image '{}' exceeds the {} MiB limit", source, kMaxImageFileSize >> 20);
            return std::nullopt;
        }
        data = bundle->readFile(readPath);
        if (!bounded_mod_png(std::span<const u8>{data.data(), data.size()})) {
            Log.warn("Mod menu image '{}' exceeds the temporary decode budget or has an invalid PNG header", source);
            return std::nullopt;
        }
        auto image = decode_png(std::span{data.data(), data.size()}, source);
        if (image) {
            // These are UI previews. Keep original gameplay textures and bundle
            // bytes intact while bounding decoded cache and GPU thumbnail sizes.
            fit_mod_image(*image, icon ? kMaxModIconDimension : kMaxModPreviewDimension);
        }
        return image;
    } catch (const std::exception& e) {
        Log.warn("Failed to load mod menu image '{}': {}", source, e.what());
        return std::nullopt;
    }
}

std::optional<aurora::rmlui::RuntimeTexture> mod_texture_provider_impl(std::string_view source) {
    if (!source.starts_with(kSourcePrefix)) {
        return std::nullopt;
    }

    auto& cache = image_cache();
    const std::string key{source};
    auto it = cache.find(key);
    auto& uses = image_use();
    if (it == cache.end()) {
        bool icon = false;
        auto image = load_mod_image(strip_query(source.substr(kSourcePrefix.size())), source, &icon);
        if (!image) {
            return std::nullopt;
        }
        if (image->pixels.size() > kMaxCachedImageBytes) {
            return std::nullopt;
        }
        make_cache_room(image->pixels.size());
        it = cache.emplace(key, CachedImage{std::move(*image), {}}).first;
        uses[key] = {++image_age(), icon};
    } else {
        uses[key].age = ++image_age();
    }
    it->second.lastLoaded = std::chrono::steady_clock::now();

    const auto& image = it->second;
    return aurora::rmlui::RuntimeTexture{
        .width = image.width,
        .height = image.height,
        .rgba8 =
            std::span{reinterpret_cast<const std::byte*>(image.pixels.data()), image.pixels.size()},
        .premultipliedAlpha = true,
        .generateMipmaps = false,
    };
}

std::optional<aurora::rmlui::RuntimeTexture> mod_texture_provider(std::string_view source) {
    try {
        return mod_texture_provider_impl(source);
    } catch (const std::exception& e) {
        try { Log.warn("Mod menu image '{}' is unavailable: {}", source, e.what()); } catch (...) {}
    } catch (...) {
        try { Log.warn("Mod menu image '{}' failed unexpectedly", source); } catch (...) {}
    }
    return std::nullopt;
}

}  // namespace

void register_mod_texture_provider() noexcept {
    aurora::rmlui::register_texture_provider(std::string{kScheme}, mod_texture_provider);
}

void unregister_mod_texture_provider() noexcept {
    aurora::rmlui::unregister_texture_provider(kScheme);
    for (const auto& [source, image] : image_cache()) {
        Rml::ReleaseTexture(source);
    }
    image_cache().clear();
    image_use().clear();
}

}  // namespace dusk::ui

#else

namespace dusk::ui {

void register_mod_texture_provider() noexcept {}

void unregister_mod_texture_provider() noexcept {}

}  // namespace dusk::ui

#endif
