#!/usr/bin/env python3
"""Exercise the production mod texture provider under memory and decode faults."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

STUBS = {
    "mod_texture_provider.hpp": "#pragma once\n",
    "runtime_image.hpp": """#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>
using u8 = uint8_t;
namespace dusk::ui {
struct DecodedImage { uint32_t width, height; std::vector<uint8_t> pixels; };
std::optional<DecodedImage> decode_png(std::span<const uint8_t>, std::string_view);
}
""",
    "dusk/mod_loader.hpp": """#pragma once
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace dusk::mods {
struct ModBundle {
    enum class Failure { None, Read, Decode, DecodeAllocation, Unexpected, HeaderAllocation };
    Failure failure = Failure::None;
    uint32_t width = 2941, height = 2941;
    size_t getFileSize(const std::string&) {
        if (failure == Failure::HeaderAllocation) throw std::bad_alloc{};
        return 12;
    }
    std::vector<uint8_t> readFile(const std::string&) {
        if (failure == Failure::Read) throw std::runtime_error{"injected read fault"};
        std::vector<uint8_t> data(12);
        std::memcpy(data.data(), &width, 4);
        std::memcpy(data.data()+4, &height, 4);
        data[8] = static_cast<uint8_t>(failure);
        return data;
    }
};
struct LoadedMod {
    struct { std::string id, iconPath; } metadata;
    std::shared_ptr<ModBundle> bundle;
    uint64_t cacheGeneration = 0;
};
struct ModLoader {
    std::vector<LoadedMod> entries;
    static ModLoader& instance() { static ModLoader value; return value; }
    auto& mods() { return entries; }
};
}
""",
    "dusk/mods/loader/loader.hpp": "#pragma once\n",
    "dusk/startup_guard.hpp": """#pragma once
#include <string>
namespace dusk::startup_guard { inline void stage(const std::string&) {} }
""",
    "dusk/utilities.hpp": """#pragma once
#include <string>
namespace dusk::utils { inline bool is_safe_resource_path(const std::string& path) {
    return !path.empty() && path[0] != '/' && path.find("..") == std::string::npos;
} }
""",
    "fmt/format.h": """#pragma once
#include <string>
namespace fmt { template<class... T> std::string format(const char* text, const T&...) { return text; } }
""",
    "borealis/log.hpp": """#pragma once
#include <new>
namespace borealis {
inline bool failLog = false;
struct Log {
    constexpr Log(const char*) {}
    template<class... T> void warn(const char*, const T&...) const {
        if (failLog) throw std::bad_alloc{};
    }
}; }
""",
    "RmlUi/Core.h": """#pragma once
#include <string>
namespace Rml { inline int releases = 0; inline void ReleaseTexture(const std::string&) { ++releases; } }
""",
    "aurora/rmlui.hpp": """#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
namespace aurora::rmlui {
struct RuntimeTexture { uint32_t width, height; std::span<const std::byte> rgba8; bool premultipliedAlpha, generateMipmaps; };
inline void register_texture_provider(std::string, std::optional<RuntimeTexture>(*)(std::string_view)) {}
inline void unregister_texture_provider(std::string_view) {}
}
""",
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("--negative-control", action="store_true")
    args = parser.parse_args()
    source = (args.source / "src/dusk/ui/mod_texture_provider.cpp").resolve()
    control = Path(__file__).resolve().parent.parent
    with tempfile.TemporaryDirectory(prefix="mod-menu-images-") as temp:
        temp = Path(temp)
        for name, content in STUBS.items():
            p = temp / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(content, encoding="utf-8")
        # Copy the real body into the fixture directory so only game/backend
        # dependencies are stubbed; the provider and resize code are unchanged.
        production = source.read_bytes().replace(b"\r\n", b"\n")
        if args.negative_control and b"mod_texture_provider_impl" in production:
            original_root = temp / "negative-source"
            original = original_root / "src/dusk/ui/mod_texture_provider.cpp"
            original.parent.mkdir(parents=True)
            original.write_bytes(production)
            patch = temp / "reverse-mods.patch"
            patch.write_bytes((control / "patches/xbox-mod-menu-742-host.patch").read_bytes().replace(b"\r\n", b"\n"))
            command = ["git", "-C", str(original_root), "apply", "--reverse",
                       "--include=src/dusk/ui/mod_texture_provider.cpp"]
            subprocess.run(command + ["--check", str(patch)], check=True)
            subprocess.run(command + [str(patch)], check=True)
            production = original.read_bytes()
        (temp / "production-provider.cpp").write_bytes(production)
        fixture = control / "tests/mod_menu_images.cpp"
        binary = temp / ("mod-menu-images.exe" if os.name == "nt" else "mod-menu-images")
        if os.name == "nt":
            command = [shutil.which("cl") or "cl", "/nologo", "/EHsc", "/std:c++20", "/MD",
                       "/DAURORA_ENABLE_RMLUI=1", "/I"+str(temp), str(fixture), "/Fe:"+str(binary)]
        else:
            command = [shutil.which("c++") or "c++", "-std=c++20", "-O0", "-g",
                       "-DAURORA_ENABLE_RMLUI=1", "-I"+str(temp), str(fixture), "-o", str(binary)]
        if args.negative_control:
            command.append(("/D" if os.name == "nt" else "-D") + "TPR_MOD_MENU_NEGATIVE_CONTROL=1")
        subprocess.run(command, cwd=temp, check=True)
        result = subprocess.run([str(binary)] + (["negative"] if args.negative_control else []), cwd=temp)
        if args.negative_control:
            if result.returncode != 42:
                raise SystemExit("Old provider did not reproduce the uncaught image allocation fault")
            print("PASS: shipped .741 provider lets injected image allocation failure escape")
        elif result.returncode:
            raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
