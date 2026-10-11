#!/usr/bin/env python3
"""Compile the shipped tracker renderer against the pinned, real ImGui core."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


STUBS = {
    "dusk/config.hpp": """#pragma once
#include <filesystem>
namespace dusk {
inline std::filesystem::path ConfigPath;
namespace config { inline int saves = 0; inline void save() { ++saves; } }
}
""",
    "dusk/main.h": "#pragma once\n",
    "dusk/settings.h": """#pragma once
namespace dusk {
template<class T> struct TestVar {
    T value; T getValue() const { return value; }
    void setValue(T next) { value = next; }
};
struct TestSettings {
    struct {
        TestVar<bool> showPureItemTracker{true};
        TestVar<float> pureItemTrackerScale{1.0f};
        TestVar<int> pureItemTrackerIconSize{56};
        TestVar<int> pureItemTrackerColumns{6};
        TestVar<float> pureItemTrackerPositionX{0.03f};
        TestVar<float> pureItemTrackerPositionY{0.03f};
        TestVar<bool> pureItemTrackerLock{false};
        TestVar<bool> pureItemTrackerResetLayoutRequested{false};
    } game;
};
inline TestSettings settings;
inline TestSettings& getSettings() { return settings; }
}
""",
    "dusk/ui/icon_provider.hpp": """#pragma once
#include <cstdint>
#include <optional>
#include <vector>
namespace dusk::ui {
struct RenderedItemIcon { std::vector<uint8_t> rgba8; uint32_t width, height; };
inline std::optional<RenderedItemIcon> render_item_icon_pixels(uint8_t) { return {}; }
}
""",
    "m_Do/m_Do_controller_pad.h": """#pragma once
enum { PAD_1 };
struct mDoCPd_c {
    static float getSubStickX(int) { return 0; }
    static float getSubStickY(int) { return 0; }
    static int getHoldA(int) { return 0; }
    static int getTrigA(int) { return 0; }
    static int getTrigB(int) { return 0; }
};
""",
    "aurora/imgui.h": """#pragma once
#include <cstdint>
#include <imgui.h>
extern "C" ImTextureID aurora_imgui_add_texture(uint32_t, uint32_t, const void*);
""",
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path, help="Build controls or prepared Dusklight source")
    parser.add_argument("--imgui", type=Path)
    parser.add_argument("--feed", type=Path)
    args = parser.parse_args()
    args.source = args.source.resolve()
    control = Path(__file__).resolve().parent.parent
    production = args.source / "src/dusk/imgui/PureItemTracker.cpp"
    if not production.is_file():
        production = args.source / "payloads/PureItemTracker.cpp"
    if not production.is_file():
        raise SystemExit("Production tracker source missing")
    imgui = args.imgui or (args.feed / "_deps/imgui-src" if args.feed else None)
    if imgui is None or not (imgui / "imgui.cpp").is_file():
        raise SystemExit("Provide the build's pinned ImGui source with --imgui or --feed")
    imgui = imgui.resolve()
    if "v1.91.9b" not in (imgui / "imgui.h").read_text(encoding="utf-8")[:150]:
        raise SystemExit("Unexpected ImGui version")
    fixture = control / "tests/pure_tracker_position.cpp"
    with tempfile.TemporaryDirectory(prefix="pure-tracker-position-") as temp:
        temp = Path(temp)
        for relative, content in STUBS.items():
            path = temp / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8")
        binary = temp / ("tracker-position.exe" if os.name == "nt" else "tracker-position")
        sources = [str(production), str(fixture)] + [
            str(imgui / name) for name in
            ("imgui.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp")
        ]
        includes = [temp, imgui, production.parent]
        if os.name == "nt":
            compiler = shutil.which("cl")
            command = [compiler or "cl", "/nologo", "/EHsc", "/std:c++20", "/MD",
                       "/D_UWP=1", "/Fe:" + str(binary)]
            command += ["/I" + str(path) for path in includes] + sources
        else:
            compiler = shutil.which("c++")
            command = [compiler or "c++", "-std=c++20", "-D_UWP=1", "-O0", "-g"]
            command += ["-I" + str(path) for path in includes] + sources + ["-o", str(binary)]
        subprocess.run(command, cwd=temp, check=True)
        subprocess.run([str(binary)], cwd=temp, check=True)


if __name__ == "__main__":
    main()
