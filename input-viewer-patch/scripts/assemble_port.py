#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
import re
import shutil
import subprocess

RANDO_REPO = "https://github.com/TwilitRealm/dusklight.git"
RANDO_SHA = "07a4d1ec6c7d80794f6ea865774ff9f45bd7da0e"
UWP_REPO = "https://github.com/SternXD/dusklight-uwp.git"
UWP_SHA = "48d193dfebe4b20b4984af1d27d0e99f6b6722a7"
AURORA_UWP_REPO = "https://github.com/SternXD/aurora.git"
AURORA_UWP_SHA = "aaf5b8b092ff547a33a9d199dd2c52572bff6682"
EXPECTED_AURORA_BASE = "6c4c27f9e8e40f584d27726655d80ec85a5a7d2c"

PACKAGE_NAME = "TwilightPrincessRandomizer"
DISPLAY_NAME = "Twilight Princess Randomizer"
PUBLISHER = "CN=TwilightPrincessRandomizer"
PUBLISHER_DISPLAY = "Twilight Princess Randomizer"
DESCRIPTION = "Twilight Princess Randomizer for Xbox/UWP"
PHONE_PRODUCT_ID = "e3b5e8e8-c7e7-5c3d-affa-0500675ffa0d"
PACKAGE_VERSION = "1.4.1.634"
TARGET_NAME = "TwilightPrincessRandomizer"

KIT_ROOT = Path(__file__).resolve().parents[1]
BRANDING_ASSETS = KIT_ROOT / "branding" / "Assets"


def run(cmd: list[str], cwd: Path | None = None, check: bool = True) -> subprocess.CompletedProcess:
    print("+", " ".join(cmd), flush=True)
    return subprocess.run(cmd, cwd=cwd, check=check, text=True)


def capture(cmd: list[str], cwd: Path) -> str:
    return subprocess.check_output(cmd, cwd=cwd, text=True).strip()


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"Required source anchor missing while patching {label}: {old[:120]!r}")
    return text.replace(old, new, 1)


def ensure_main_uwp_cmake(source: Path) -> None:
    path = source / "CMakeLists.txt"
    text = path.read_text(encoding="utf-8")

    if 'option(DUSK_UWP "Enable UWP compat support"' not in text:
        anchor = 'option(DUSK_ENABLE_CODE_MODS "Enable code mods" OFF)\n'
        text = replace_once(
            text,
            anchor,
            anchor
            + 'option(DUSK_UWP "Enable UWP compat support" OFF)\n\n'
            + 'if (MSVC AND DUSK_UWP)\n'
            + '    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "" FORCE)\n'
            + 'endif ()\n',
            "DUSK_UWP option",
        )

    # UWP file browser source list integration.  The pinned randomizer moved its
    # DUSK_FILES definition out to files.cmake, so do not anchor this port on a
    # neighboring filename in that list.  Instead append the UWP-only sources to
    # DUSK_FILES immediately before the target is created.  This remains valid
    # whether the list itself lives in CMakeLists.txt, files.cmake, or another
    # included CMake fragment.
    if "src/dusk/ui/file_browser.cpp" not in text or "src/dusk/ui/file_browser.hpp" not in text:
        target_match = re.search(
            r"(?m)^if\(ANDROID\)\r?$\n[ \t]+add_library\(dusklight SHARED \${DUSK_FILES}\)",
            text,
        )
        if target_match is None:
            raise RuntimeError("Could not locate Dusklight target creation while adding UWP FileBrowser sources")
        addition = (
            "# UWP file browser source list\n"
            "if (DUSK_UWP)\n"
            "    list(APPEND DUSK_FILES\n"
            "        src/dusk/ui/file_browser.cpp\n"
            "        src/dusk/ui/file_browser.hpp\n"
            "    )\n"
            "endif ()\n\n"
        )
        text = text[: target_match.start()] + addition + text[target_match.start() :]

    if "elseif (DUSK_UWP)" not in text:
        old = (
            'if(ANDROID)\n'
            '    add_library(dusklight SHARED ${DUSK_FILES})\n'
            '    set_target_properties(dusklight PROPERTIES OUTPUT_NAME main)\n'
            'else ()\n'
            '    add_executable(dusklight ${DUSK_FILES})\n'
            'endif ()'
        )
        new = (
            'if(ANDROID)\n'
            '    add_library(dusklight SHARED ${DUSK_FILES})\n'
            '    set_target_properties(dusklight PROPERTIES OUTPUT_NAME main)\n'
            'elseif (DUSK_UWP)\n'
            '    add_library(dusklight STATIC ${DUSK_FILES})\n'
            '    target_compile_definitions(dusklight PRIVATE _UWP)\n'
            'else ()\n'
            '    add_executable(dusklight ${DUSK_FILES})\n'
            'endif ()'
        )
        text = replace_once(text, old, new, "static UWP target")

    # UWP wrapper owns runtime deployment and package resources.
    text = text.replace(
        "if (NOT APPLE)\n    add_custom_command(TARGET dusklight POST_BUILD",
        "if (NOT APPLE AND NOT DUSK_UWP)\n    add_custom_command(TARGET dusklight POST_BUILD",
    )
    text = text.replace(
        "if (WIN32)\n    set(DUSK_WINDOWS_RESOURCE_DIR",
        "if (WIN32 AND NOT DUSK_UWP)\n    set(DUSK_WINDOWS_RESOURCE_DIR",
    )
    # The legacy UWP fork deliberately omits Aurora runtime-DLL deployment because
    # the UWP wrapper owns package runtime files. The newer randomizer added a new
    # aurora_install_runtime_dlls() call later in the file. After the source merge
    # that call can survive outside the UWP guard while the helper include remains
    # inside it, causing CMake to fail with "Unknown CMake command". Keep both
    # runtime copy/install paths desktop-only.
    install_call = "aurora_install_runtime_dlls(dusklight ${CMAKE_INSTALL_PREFIX})"
    guarded_install = "if (NOT DUSK_UWP)\n    " + install_call + "\nendif ()"
    if install_call in text and guarded_install not in text:
        text = text.replace(install_call, guarded_install, 1)

    # Psapi is desktop crash/symbol support; do not force it into the Store/UWP link.
    text = text.replace(
        "if (WIN32)\n    target_link_libraries(dusklight PRIVATE Psapi)\nendif ()",
        "if (WIN32 AND NOT DUSK_UWP)\n    target_link_libraries(dusklight PRIVATE Psapi)\nendif ()",
    )

    path.write_text(text, encoding="utf-8")


def ensure_main_cpp_uwp(source: Path) -> None:
    path = source / "src/dusk/main.cpp"
    text = path.read_text(encoding="utf-8")

    text = text.replace(
        '(defined(TARGET_OS_TV) && TARGET_OS_TV)\n    (void)argc;',
        '(defined(TARGET_OS_TV) && TARGET_OS_TV) || defined(_UWP)\n    (void)argc;',
        1,
    )
    # Console allocation is desktop-only even though the static feed is compiled with _WIN32.
    text = text.replace(
        "int DuskMain(int argc, char* argv[]) {\n    WindowsSetupConsole(ShouldShowWindowsConsole(argc, argv));",
        "int DuskMain(int argc, char* argv[]) {\n#if !defined(_UWP)\n    WindowsSetupConsole(ShouldShowWindowsConsole(argc, argv));\n#endif",
        1,
    )
    text = text.replace("#if _WIN32\nint WINAPI wWinMain", "#if _WIN32 && !defined(_UWP)\nint WINAPI wWinMain", 1)

    if "SDL_MAIN_EXPORTED" not in text:
        main_anchor = 'int main(int argc, char* argv[]) {\n    return DuskMain(argc, argv);\n}\n'
        main_repl = (
            '#ifdef _UWP\n'
            '#define SDL_MAIN_EXPORTED\n'
            '#include "SDL3/SDL_main.h"\n'
            '#endif\n'
            + main_anchor
        )
        text = replace_once(text, main_anchor, main_repl, "SDL UWP main export")

    path.write_text(text, encoding="utf-8")



def patch_io_uwp_local_folder(source: Path) -> None:
    """Guarantee the UWP LocalState helper has a linked implementation.

    SternXD's FileBrowser declares dusk::io::uwp_local_folder_path() in io.hpp.
    On the newer randomizer tree the corresponding io.cpp hunk can be lost while
    merging with -X ours, leaving a declaration that compiles but an unresolved
    external at the final UWP link. Install the implementation explicitly in
    io.cpp so the static feed always exports the symbol.
    """
    path = source / "src/dusk/io.cpp"
    text = path.read_text(encoding="utf-8")

    if "uwp_local_folder_path()" in text:
        return

    include_anchor = '#include "dusk/io.hpp"\n'
    include_block = (
        '#include "dusk/io.hpp"\n'
        '#include "dusk/app_info.hpp"\n'
        '\n'
        '#include <SDL3/SDL_filesystem.h>\n'
    )
    text = replace_once(text, include_anchor, include_block, "UWP LocalState includes")

    namespace_close = "\n}  // namespace dusk::io\n"
    if namespace_close not in text:
        raise RuntimeError("Could not locate dusk::io namespace close while adding UWP LocalState helper")

    impl = (
        '\n#if defined(_UWP)\n'
        'std::optional<std::filesystem::path> uwp_local_folder_path() {\n'
        '    char* pref = SDL_GetPrefPath(dusk::OrgName, dusk::AppName);\n'
        '    if (pref == nullptr) {\n'
        '        return std::nullopt;\n'
        '    }\n'
        '\n'
        '    const std::filesystem::path p = path_from_utf8(pref);\n'
        '    SDL_free(pref);\n'
        '    if (p.empty()) {\n'
        '        return std::nullopt;\n'
        '    }\n'
        '\n'
        '    std::error_code ec;\n'
        '    const std::filesystem::path out = std::filesystem::absolute(p, ec).lexically_normal();\n'
        '    if (ec || out.empty()) {\n'
        '        return std::nullopt;\n'
        '    }\n'
        '    if (!std::filesystem::exists(out, ec) || !std::filesystem::is_directory(out, ec)) {\n'
        '        return std::nullopt;\n'
        '    }\n'
        '    return out;\n'
        '}\n'
        '#endif\n'
    )
    head, tail = text.rsplit(namespace_close, 1)
    text = head + impl + namespace_close + tail
    path.write_text(text, encoding="utf-8")


def patch_file_browser_current_pane(source: Path) -> None:
    # Forward-port SternXD's UWP FileBrowser to the pinned randomizer Pane API.
    # Keep compatibility local to the browser and use Component::children().
    path = source / "src/dusk/ui/file_browser.cpp"
    if not path.exists():
        raise RuntimeError("UWP FileBrowser source is missing")
    text = path.read_text(encoding="utf-8")

    old_last = '''bool try_focus_file_pane_last(Pane* pane, Rml::Event& event) {
    if (pane != nullptr && pane->focus_last()) {
        mDoAud_seStartMenu(kSoundItemFocus);
        event.StopImmediatePropagation();
        return true;
    }
    return false;
}
'''
    new_last = '''bool try_focus_file_pane_last(Pane* pane, Rml::Event& event) {
    if (pane == nullptr) {
        return false;
    }
    auto& children = pane->children();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        if (*it != nullptr && (*it)->focus()) {
            mDoAud_seStartMenu(kSoundItemFocus);
            event.StopImmediatePropagation();
            return true;
        }
    }
    return false;
}

int file_pane_row_count(Pane* pane) {
    return pane == nullptr ? 0 : static_cast<int>(pane->children().size());
}

int file_pane_child_index_containing(Pane* pane, Rml::Element* target) {
    if (pane == nullptr || target == nullptr) {
        return -1;
    }
    auto& children = pane->children();
    for (int i = 0; i < static_cast<int>(children.size()); ++i) {
        if (children[static_cast<std::size_t>(i)] != nullptr &&
            children[static_cast<std::size_t>(i)]->contains(target))
        {
            return i;
        }
    }
    return -1;
}
'''
    if old_last in text:
        text = text.replace(old_last, new_last, 1)
    elif "int file_pane_row_count(Pane* pane)" not in text:
        raise RuntimeError("Could not locate legacy FileBrowser Pane navigation helper")

    text = text.replace("mFilePane->row_count()", "file_pane_row_count(mFilePane.get())")
    text = text.replace(
        "mFilePane->child_index_containing(target)",
        "file_pane_child_index_containing(mFilePane.get(), target)",
    )

    stale = ("->row_count()", "->child_index_containing(", "->focus_last()")
    for marker in stale:
        if marker in text:
            raise RuntimeError(f"Legacy Pane API still referenced by UWP FileBrowser: {marker}")

    for marker in (
        "int file_pane_row_count(Pane* pane)",
        "int file_pane_child_index_containing(Pane* pane, Rml::Element* target)",
        "pane->children()",
    ):
        if marker not in text:
            raise RuntimeError(f"Current-Pane FileBrowser forward-port marker missing: {marker}")

    path.write_text(text, encoding="utf-8")


def patch_pane_uwp_compat(source: Path) -> None:
    """Restore the small Pane navigation surface used by the UWP FileBrowser.

    The pinned randomizer refactored Pane after SternXD's UWP FileBrowser was
    written, removing focus_last(), child_index_containing(), and row_count().
    FileBrowser still needs those navigation primitives. Re-add them as thin
    compatibility helpers over the current Pane child list without changing
    current Pane behavior or its newer bottom-spacer/focus-closeness logic.
    """
    hpp_path = source / "src/dusk/ui/pane.hpp"
    cpp_path = source / "src/dusk/ui/pane.cpp"
    hpp = hpp_path.read_text(encoding="utf-8")
    cpp = cpp_path.read_text(encoding="utf-8")

    if "bool focus_last();" not in hpp:
        hpp = replace_once(
            hpp,
            "    bool focus() override;\n",
            "    bool focus() override;\n    bool focus_last();\n",
            "Pane UWP focus_last declaration",
        )

    if "child_index_containing(Rml::Element* target) const" not in hpp:
        anchor = "    bool focus_closest_child(float posY);\n\nprivate:\n"
        addition = (
            "    bool focus_closest_child(float posY);\n\n"
            "    // Compatibility helpers used by the UWP in-app FileBrowser.\n"
            "    int child_index_containing(Rml::Element* target) const;\n"
            "    int row_count() const noexcept;\n\n"
            "private:\n"
        )
        hpp = replace_once(hpp, anchor, addition, "Pane UWP navigation declarations")

    if "bool Pane::focus_last()" not in cpp:
        anchor = "Rml::Element* Pane::add_section(const Rml::String& text) {\n"
        impl = (
            "bool Pane::focus_last() {\n"
            "    for (int i = static_cast<int>(mChildren.size()) - 1; i >= 0; --i) {\n"
            "        if (mChildren[static_cast<std::size_t>(i)]->focus()) {\n"
            "            return true;\n"
            "        }\n"
            "    }\n"
            "    return false;\n"
            "}\n\n"
        )
        cpp = replace_once(cpp, anchor, impl + anchor, "Pane UWP focus_last implementation")

    if "int Pane::child_index_containing(Rml::Element* target) const" not in cpp:
        anchor = "float Pane::get_focused_child_y() {\n"
        impl = (
            "int Pane::child_index_containing(Rml::Element* target) const {\n"
            "    if (target == nullptr) {\n"
            "        return -1;\n"
            "    }\n"
            "    for (int i = 0; i < static_cast<int>(mChildren.size()); ++i) {\n"
            "        if (mChildren[static_cast<std::size_t>(i)]->contains(target)) {\n"
            "            return i;\n"
            "        }\n"
            "    }\n"
            "    return -1;\n"
            "}\n\n"
            "int Pane::row_count() const noexcept {\n"
            "    return static_cast<int>(mChildren.size());\n"
            "}\n\n"
        )
        cpp = replace_once(cpp, anchor, impl + anchor, "Pane UWP child navigation implementation")

    hpp_path.write_text(hpp, encoding="utf-8")
    cpp_path.write_text(cpp, encoding="utf-8")


def patch_heart_color_cosmetic(source: Path) -> None:
    """Restore the tested CP15 persistent HUD heart-color cosmetic only.

    Keep the physically working .627 launch path and the isolated direct-ZIP
    loader unchanged. This is the one requested CP20 feature being restored.
    An empty/invalid value leaves the stock red heart texture unchanged.
    """
    settings_h_path = source / "src/dusk/settings.h"
    settings_cpp_path = source / "src/dusk/settings.cpp"
    cosmetics_cpp_path = source / "src/dusk/ui/cosmetics.cpp"
    meter_path = source / "src/d/d_meter2_draw.cpp"

    settings_h = settings_h_path.read_text(encoding="utf-8")
    if "ConfigVar<std::string> heartColor;" not in settings_h:
        settings_h = replace_once(
            settings_h,
            "        ConfigVar<std::string> eponaColor;\n",
            "        ConfigVar<std::string> eponaColor;\n"
            "        ConfigVar<std::string> heartColor;\n",
            "heart color setting declaration",
        )
    settings_h_path.write_text(settings_h, encoding="utf-8")

    settings_cpp = settings_cpp_path.read_text(encoding="utf-8")
    if '.heartColor = {"cosmetics.heartColor", ""}' not in settings_cpp:
        settings_cpp = replace_once(
            settings_cpp,
            '        .eponaColor = {"cosmetics.eponaColor", ""},\n',
            '        .eponaColor = {"cosmetics.eponaColor", ""},\n'
            '        .heartColor = {"cosmetics.heartColor", ""},\n',
            "heart color default",
        )
    if "Register(g_userSettings.cosmetics.heartColor);" not in settings_cpp:
        settings_cpp = replace_once(
            settings_cpp,
            "    Register(g_userSettings.cosmetics.eponaColor);\n",
            "    Register(g_userSettings.cosmetics.eponaColor);\n"
            "    Register(g_userSettings.cosmetics.heartColor);\n",
            "heart color registration",
        )
    settings_cpp_path.write_text(settings_cpp, encoding="utf-8")

    cosmetics_cpp = cosmetics_cpp_path.read_text(encoding="utf-8")
    if 'add_cosmetic_option(leftPane, rightPane, "Heart Color", cosmetics.heartColor);' not in cosmetics_cpp:
        anchor = '    add_tab("Misc Colors", [this, &cosmetics](Rml::Element* content) {\n'
        hud_tab = (
            '    add_tab("HUD Colors", [this, &cosmetics](Rml::Element* content) {\n'
            '        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);\n'
            '        auto& rightPane = add_child<Pane>(content, Pane::Type::Controlled);\n\n'
            '        add_cosmetic_option(leftPane, rightPane, "Heart Color", cosmetics.heartColor);\n'
            '    });\n\n'
        )
        cosmetics_cpp = replace_once(cosmetics_cpp, anchor, hud_tab + anchor, "HUD heart color cosmetics tab")
    cosmetics_cpp_path.write_text(cosmetics_cpp, encoding="utf-8")

    meter = meter_path.read_text(encoding="utf-8")
    marker = "// TPR UWP: live HUD heart-color cosmetic"
    if marker not in meter:
        anchor = "void dMeter2Draw_c::drawLife(s16 i_maxLife, s16 i_life, f32 i_posX, f32 i_posY) {\n"
        block = r'''void dMeter2Draw_c::drawLife(s16 i_maxLife, s16 i_life, f32 i_posX, f32 i_posY) {
#if TARGET_PC
    // TPR UWP: live HUD heart-color cosmetic. The original console randomizer
    // colors mpLifeTexture[][1] plus the four big/partial-heart pictures.
    JUtility::TColor heartVertexColor(255, 255, 255, 255);
    const auto& heartColorStr = dusk::getSettings().cosmetics.heartColor.getValue();
    if (dusk::cosmetics::is_valid_hex_color_str(heartColorStr)) {
        const GXColor color = dusk::cosmetics::hex_color_str_to_gx_color(heartColorStr);
        heartVertexColor = JUtility::TColor(color.r, color.g, color.b, 255);
    }

    for (int i = 0; i < 20; ++i) {
        if (mpLifeTexture[i][1] != nullptr && mpLifeTexture[i][1]->getPanePtr() != nullptr) {
            static_cast<J2DPicture*>(mpLifeTexture[i][1]->getPanePtr())->setCornerColor(heartVertexColor);
        }
    }

    static u64 const tprHeartQuarterTags[] = {
        MULTI_CHAR('bigh_00'), MULTI_CHAR('bigh_01'), MULTI_CHAR('bigh_02'), MULTI_CHAR('bigh_03')};
    for (u64 tag : tprHeartQuarterTags) {
        if (auto* pane = mpScreen->search(tag)) {
            static_cast<J2DPicture*>(pane)->setCornerColor(heartVertexColor);
        }
    }
#endif
'''
        meter = replace_once(meter, anchor, block, "live HUD heart-color application")
    meter_path.write_text(meter, encoding="utf-8")


def patch_menu_and_per_seed_cosmetics(source: Path) -> None:
    """Add live menu colors and deterministic per-seed cosmetic randomization."""
    settings_h_path = source / "src/dusk/settings.h"
    settings_cpp_path = source / "src/dusk/settings.cpp"
    cosmetics_hpp_path = source / "src/dusk/ui/cosmetics.hpp"
    cosmetics_cpp_path = source / "src/dusk/ui/cosmetics.cpp"
    ui_cpp_path = source / "src/dusk/ui/ui.cpp"

    settings_h = settings_h_path.read_text(encoding="utf-8")
    if "ConfigVar<std::string> menuAccentColor;" not in settings_h:
        settings_h = replace_once(
            settings_h,
            "        ConfigVar<std::string> heartColor;\n",
            "        ConfigVar<std::string> heartColor;\n"
            "        ConfigVar<std::string> menuAccentColor;\n"
            "        ConfigVar<std::string> menuBackgroundColor;\n"
            "        ConfigVar<std::string> menuTextColor;\n"
            "        ConfigVar<std::string> menuBorderColor;\n"
            "        ConfigVar<bool> randomizeAllColorsPerSeed;\n"
            "        ConfigVar<std::string> perSeedRandomizedColors;\n",
            "menu and per-seed cosmetic settings declarations",
        )
    settings_h_path.write_text(settings_h, encoding="utf-8")

    settings_cpp = settings_cpp_path.read_text(encoding="utf-8")
    if '.menuAccentColor = {"cosmetics.menuAccentColor", ""}' not in settings_cpp:
        settings_cpp = replace_once(
            settings_cpp,
            '        .heartColor = {"cosmetics.heartColor", ""},\n',
            '        .heartColor = {"cosmetics.heartColor", ""},\n'
            '        .menuAccentColor = {"cosmetics.menuAccentColor", ""},\n'
            '        .menuBackgroundColor = {"cosmetics.menuBackgroundColor", ""},\n'
            '        .menuTextColor = {"cosmetics.menuTextColor", ""},\n'
            '        .menuBorderColor = {"cosmetics.menuBorderColor", ""},\n'
            '        .randomizeAllColorsPerSeed = {"cosmetics.randomizeAllColorsPerSeed", false},\n'
            '        .perSeedRandomizedColors = {"cosmetics.perSeedRandomizedColors", ""},\n',
            "menu and per-seed cosmetic settings defaults",
        )
    if "Register(g_userSettings.cosmetics.menuAccentColor);" not in settings_cpp:
        settings_cpp = replace_once(
            settings_cpp,
            "    Register(g_userSettings.cosmetics.heartColor);\n",
            "    Register(g_userSettings.cosmetics.heartColor);\n"
            "    Register(g_userSettings.cosmetics.menuAccentColor);\n"
            "    Register(g_userSettings.cosmetics.menuBackgroundColor);\n"
            "    Register(g_userSettings.cosmetics.menuTextColor);\n"
            "    Register(g_userSettings.cosmetics.menuBorderColor);\n"
            "    Register(g_userSettings.cosmetics.randomizeAllColorsPerSeed);\n"
            "    Register(g_userSettings.cosmetics.perSeedRandomizedColors);\n",
            "menu and per-seed cosmetic settings registrations",
        )
    settings_cpp_path.write_text(settings_cpp, encoding="utf-8")

    cosmetics_hpp = cosmetics_hpp_path.read_text(encoding="utf-8")
    if "apply_per_seed_cosmetics" not in cosmetics_hpp:
        if "#include <string>" not in cosmetics_hpp:
            cosmetics_hpp = replace_once(
                cosmetics_hpp,
                '#include "window.hpp"\n',
                '#include "window.hpp"\n\n#include <string>\n',
                "per-seed cosmetic string declaration include",
            )
        cosmetics_hpp = replace_once(
            cosmetics_hpp,
            "namespace dusk::ui {\n",
            "namespace dusk::ui {\n"
            "// Applies the live RmlUi palette and deterministic seed-bound colors.\n"
            "void apply_menu_color_theme() noexcept;\n"
            "void apply_per_seed_cosmetics(const std::string& seedHash) noexcept;\n\n",
            "menu and per-seed cosmetic API",
        )
    cosmetics_hpp_path.write_text(cosmetics_hpp, encoding="utf-8")

    cosmetics_cpp = cosmetics_cpp_path.read_text(encoding="utf-8")
    marker = "TPR UWP: deterministic per-seed cosmetic colors"
    if marker not in cosmetics_cpp:
        cosmetics_cpp = replace_once(
            cosmetics_cpp,
            '#include "dusk/config.hpp"\n',
            '#include "dusk/config.hpp"\n'
            '#include "dusk/cosmetics/color_utils.hpp"\n'
            '#include "dusk/randomizer/game/randomizer_context.hpp"\n'
            '#include "dusk/settings.h"\n',
            "menu and per-seed cosmetic includes",
        )
        if "#include <array>" not in cosmetics_cpp:
            cosmetics_cpp = replace_once(
                cosmetics_cpp,
                "#include <random>\n",
                "#include <array>\n#include <cstdint>\n#include <random>\n#include <string_view>\n",
                "menu and per-seed cosmetic standard includes",
            )

        cosmetics_cpp = replace_once(
            cosmetics_cpp,
            '    {"b9ab00", "Yellow"},\n',
            '    {"b9ab00", "Yellow"},\n    {"58a866", "Green"},\n',
            "Green general and heart color preset",
        )

        helper_anchor = "void add_cosmetic_option(Pane& leftPane, Pane& rightPane, const std::string& key, ConfigVar<std::string>& option,\n"
        helpers = r'''// TPR UWP: deterministic per-seed cosmetic colors. Individual choices are
// stored as a compact set of exact CVar names, so every color can opt in without
// adding dozens of independent boolean settings.
bool per_seed_color_enabled(const ConfigVar<std::string>& option) {
    const std::string token = std::string("|") + option.getName() + "|";
    return getSettings().cosmetics.perSeedRandomizedColors.getValue().find(token) !=
        std::string::npos;
}

void set_per_seed_color_enabled(ConfigVar<std::string>& option, bool enabled) {
    auto stored = getSettings().cosmetics.perSeedRandomizedColors.getValue();
    const std::string token = std::string("|") + option.getName() + "|";
    const auto position = stored.find(token);
    if (enabled && position == std::string::npos) {
        stored += token;
    } else if (!enabled) {
        size_t match = position;
        while (match != std::string::npos) {
            stored.erase(match, token.size());
            match = stored.find(token);
        }
    }
    getSettings().cosmetics.perSeedRandomizedColors.setValue(stored);
}

std::uint64_t stable_seed_color_hash(
    std::string_view seedHash, std::string_view optionName) noexcept
{
    std::uint64_t value = 1469598103934665603ULL;
    const auto mix = [&value](unsigned char byte) {
        value ^= byte;
        value *= 1099511628211ULL;
    };
    for (unsigned char byte : seedHash) {
        mix(byte);
    }
    mix(0xff);
    for (unsigned char byte : optionName) {
        mix(byte);
    }
    // SplitMix64 finalization prevents similar setting names from producing
    // visibly correlated channels.
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebULL;
    value ^= value >> 31;
    return value;
}

std::string deterministic_hex_color(
    std::string_view seedHash, const ConfigVar<std::string>& option)
{
    const std::uint64_t value = stable_seed_color_hash(seedHash, option.getName());
    const std::string_view name = option.getName();
    int minimum = 32;
    int range = 224;
    if (name == "cosmetics.menuBackgroundColor") {
        minimum = 10;
        range = 42;
    } else if (name == "cosmetics.menuTextColor") {
        minimum = 190;
        range = 66;
    } else if (name == "cosmetics.menuAccentColor" ||
               name == "cosmetics.menuBorderColor")
    {
        minimum = 72;
        range = 184;
    }

    const auto channel = [value, minimum, range](int shift) {
        return minimum + static_cast<int>((value >> shift) & 0xff) % range;
    };
    return fmt::format(
        "{:02x}{:02x}{:02x}", channel(0), channel(8), channel(16));
}

std::string effective_menu_color(
    const ConfigVar<std::string>& option, std::string_view fallback)
{
    const auto& configured = option.getValue();
    if (cosmetics::is_valid_hex_color_str(configured)) {
        return configured;
    }
    return std::string(fallback);
}

void apply_menu_color_theme() noexcept {
    try {
        auto& values = getSettings().cosmetics;
        const std::string accent = effective_menu_color(values.menuAccentColor, "c2a42d");
        const std::string background = effective_menu_color(values.menuBackgroundColor, "151610");
        const std::string text = effective_menu_color(values.menuTextColor, "e0dbc8");
        const std::string border = effective_menu_color(values.menuBorderColor, "92875b");

        const std::string windowTheme =
            "body { color: #" + text + "; }\n"
            "window { background-color: #" + background + "e6; border-color: #" + border + "; }\n"
            "window tab-bar { border-bottom-color: #" + border + "; }\n"
            "window content pane:not(:last-of-type), pane.excluded-locations-pane { border-color: #" + border + "; }\n"
            "tab-bar tab:selected { border-bottom-color: #" + accent + "; }\n"
            "tab-bar tab:focus-visible, tab-bar tab:hover { decorator: vertical-gradient(#" + accent + "00 #" + accent + "26); }\n"
            "tab-bar tab:active { decorator: vertical-gradient(#" + accent + "10 #" + accent + "40); }\n"
            "button:not(:disabled):hover, button:not(:disabled):focus-visible, "
            "select-button:not(:disabled):hover, select-button:not(:disabled):focus-visible { "
            "background-color: #" + accent + "33; box-shadow: #" + accent + " 0 0 0 2dp; }\n"
            "button:not(:disabled):selected, button:not(:disabled):active, "
            "select-button:not(:disabled):selected, select-button:not(:disabled):active { "
            "background-color: #" + accent + "66; box-shadow: #" + accent + " 0 0 0 2dp; }\n"
            "scrollbarvertical sliderbar:hover, scrollbarvertical sliderbar:active { "
            "background-color: #" + accent + "cc; }\n"
            "tab-bar[closable] close:hover, tab-bar[closable] close:focus-visible, "
            "window > close:hover, window > close:focus-visible { background-color: #" + accent + "3d; }\n";

        const std::string menuBarTheme =
            "body, popup tab-bar tab { color: #" + text + "; }\n"
            "popup { background-color: #" + background + "cc; border-bottom-color: #" + border + "; }\n"
            "tab-bar tab:selected { border-bottom-color: #" + accent + "; }\n"
            "tab-bar tab:focus-visible, tab-bar tab:hover { decorator: vertical-gradient(#" + accent + "00 #" + accent + "26); }\n"
            "tab-bar tab:active { decorator: vertical-gradient(#" + accent + "10 #" + accent + "40); }\n";

        register_scoped_styles(DocumentScope::Window, "tpr-user-menu-theme", windowTheme);
        register_scoped_styles(DocumentScope::MenuBar, "tpr-user-menu-theme", menuBarTheme);
    } catch (...) {
        // A cosmetic preference must never be able to interrupt the game/UI loop.
    }
}

void apply_per_seed_cosmetics(const std::string& seedHash) noexcept {
    if (seedHash.empty()) {
        return;
    }

    try {
        auto& values = getSettings().cosmetics;
        struct SeedColorTarget {
            ConfigVar<std::string>* option;
            bool namedMidnaPreset;
        };
        const std::array targets = {
            SeedColorTarget{&values.herosTunicCapColor, false},
            SeedColorTarget{&values.herosTunicTorsoColor, false},
            SeedColorTarget{&values.herosTunicSkirtColor, false},
            SeedColorTarget{&values.zoraArmorCapColor, false},
            SeedColorTarget{&values.zoraArmorHelmetColor, false},
            SeedColorTarget{&values.zoraArmorTorsoColor, false},
            SeedColorTarget{&values.zoraArmorScalesColor, false},
            SeedColorTarget{&values.zoraArmorFlippersColor, false},
            SeedColorTarget{&values.lanternGlowColor, false},
            SeedColorTarget{&values.woodenSwordColor, false},
            SeedColorTarget{&values.msBladeColor, false},
            SeedColorTarget{&values.msHandleColor, false},
            SeedColorTarget{&values.lightSwordGlowColor, false},
            SeedColorTarget{&values.boomerangColor, false},
            SeedColorTarget{&values.ironBootsColor, false},
            SeedColorTarget{&values.spinnerColor, false},
            SeedColorTarget{&values.midnaHairBaseColor, true},
            SeedColorTarget{&values.midnaHairTipsColor, true},
            SeedColorTarget{&values.midnaChargeRingColor, false},
            SeedColorTarget{&values.linkHairColor, false},
            SeedColorTarget{&values.wolfLinkColor, false},
            SeedColorTarget{&values.eponaColor, false},
            SeedColorTarget{&values.heartColor, false},
            SeedColorTarget{&values.menuAccentColor, false},
            SeedColorTarget{&values.menuBackgroundColor, false},
            SeedColorTarget{&values.menuTextColor, false},
            SeedColorTarget{&values.menuBorderColor, false},
        };
        static constexpr std::array<std::string_view, 8> midnaPresets = {
            "Pink", "Red", "Yellow", "Green", "Blue", "Purple", "Brown", "White"};

        const bool randomizeAll = values.randomizeAllColorsPerSeed.getValue();
        bool changed = false;
        bool midnaChanged = false;
        for (const auto& target : targets) {
            if (!randomizeAll && !per_seed_color_enabled(*target.option)) {
                continue;
            }
            std::string next;
            if (target.namedMidnaPreset) {
                const auto hash = stable_seed_color_hash(seedHash, target.option->getName());
                next = std::string(midnaPresets[hash % midnaPresets.size()]);
                midnaChanged = true;
            } else {
                next = deterministic_hex_color(seedHash, *target.option);
            }
            if (target.option->getValue() != next) {
                target.option->setValue(std::move(next));
                changed = true;
            }
        }

        if (midnaChanged) {
            cosmetics::set_all_midna_hair_colors();
        }
        apply_menu_color_theme();
        if (changed) {
            config::save();
        }
    } catch (...) {
        // Per-seed cosmetics are optional and must never affect seed loading.
    }
}

'''
        cosmetics_cpp = replace_once(
            cosmetics_cpp, helper_anchor, helpers + helper_anchor,
            "menu theme and deterministic per-seed cosmetic helpers",
        )

        cosmetics_cpp = replace_once(
            cosmetics_cpp,
            "                    option.setValue(str);\n                    config::save();\n",
            "                    option.setValue(str);\n"
            "                    set_per_seed_color_enabled(option, false);\n"
            "                    apply_menu_color_theme();\n"
            "                    config::save();\n",
            "manual hex color disables individual seed randomization",
        )
        cosmetics_cpp = replace_once(
            cosmetics_cpp,
            "                option.setValue(\"\");\n"
            "                if (key.starts_with(\"Midna's Hair\")) {\n",
            "                option.setValue(\"\");\n"
            "                set_per_seed_color_enabled(option, false);\n"
            "                if (key.starts_with(\"Midna's Hair\")) {\n",
            "default color disables individual seed randomization",
        )
        cosmetics_cpp = replace_once(
            cosmetics_cpp,
            "                }\n                config::save();\n            });\n\n"
            "            pane.add_button(ControlledButton::Props{\n"
            "                .text = \"Random Color\",\n",
            "                }\n                apply_menu_color_theme();\n                config::save();\n            });\n\n"
            "            pane.add_button(ControlledButton::Props{\n"
            "                .text = \"Random Color\",\n",
            "default color live menu theme application",
        )
        cosmetics_cpp = replace_once(
            cosmetics_cpp,
            "                option.setValue(hexStr);\n                config::save();\n",
            "                option.setValue(hexStr);\n"
            "                set_per_seed_color_enabled(option, false);\n"
            "                apply_menu_color_theme();\n"
            "                config::save();\n",
            "one-time random color disables individual seed randomization",
        )

        per_option_anchor = "        }\n\n        for (const auto& [hexStr, color] : colorPresets) {\n"
        per_option_button = r'''        }

        pane.add_button(ControlledButton::Props{
            .text = "Randomize This Color Per Seed",
            .isSelected = [&option] {
                return getSettings().cosmetics.randomizeAllColorsPerSeed.getValue() ||
                    per_seed_color_enabled(option);
            },
            .isDisabled = [] {
                return getSettings().cosmetics.randomizeAllColorsPerSeed.getValue();
            },
        }).on_pressed([&option] {
            if (getSettings().cosmetics.randomizeAllColorsPerSeed.getValue()) {
                return;
            }
            set_per_seed_color_enabled(option, !per_seed_color_enabled(option));
            const auto& hash = randomizer_GetContext().mHash;
            if (!hash.empty()) {
                apply_per_seed_cosmetics(hash);
            }
            config::save();
        });

        for (const auto& [hexStr, color] : colorPresets) {
'''
        cosmetics_cpp = replace_once(
            cosmetics_cpp, per_option_anchor, per_option_button,
            "individual per-seed randomization button",
        )
        cosmetics_cpp = replace_once(
            cosmetics_cpp,
            "                option.setValue(hexStr);\n"
            "                if (key.starts_with(\"Midna's Hair\")) {\n",
            "                option.setValue(hexStr);\n"
            "                set_per_seed_color_enabled(option, false);\n"
            "                if (key.starts_with(\"Midna's Hair\")) {\n",
            "preset color disables individual seed randomization",
        )
        cosmetics_cpp = replace_once(
            cosmetics_cpp,
            "                }\n                config::save();\n            });\n"
            "        }\n    });\n}\n\nCosmeticsWindow::CosmeticsWindow() {\n",
            "                }\n                apply_menu_color_theme();\n                config::save();\n            });\n"
            "        }\n    });\n}\n\nCosmeticsWindow::CosmeticsWindow() {\n",
            "preset color live menu theme application",
        )

        tabs_anchor = '    add_tab("Equipment Colors", [this, &cosmetics](Rml::Element* content) {\n'
        tabs = r'''    add_tab("Randomize Colors", [this, &cosmetics](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Controlled);

        leftPane.register_control(leftPane.add_select_button({
            .key = "Randomize All Per Seed",
            .getValue = [&cosmetics] {
                return cosmetics.randomizeAllColorsPerSeed.getValue() ?
                    Rml::String("On") : Rml::String("Off");
            },
        }), rightPane, [&cosmetics](Pane& pane) {
            pane.clear();
            pane.add_rml(
                "When enabled, every equipment, HUD, misc, and menu color is "
                "chosen deterministically from the selected randomizer seed. "
                "The same seed always receives the same palette.");
            pane.add_button(ControlledButton::Props{
                .text = "Off",
                .isSelected = [&cosmetics] {
                    return !cosmetics.randomizeAllColorsPerSeed.getValue();
                },
            }).on_pressed([&cosmetics] {
                cosmetics.randomizeAllColorsPerSeed.setValue(false);
                config::save();
            });
            pane.add_button(ControlledButton::Props{
                .text = "On",
                .isSelected = [&cosmetics] {
                    return cosmetics.randomizeAllColorsPerSeed.getValue();
                },
            }).on_pressed([&cosmetics] {
                cosmetics.randomizeAllColorsPerSeed.setValue(true);
                const auto& hash = randomizer_GetContext().mHash;
                if (!hash.empty()) {
                    apply_per_seed_cosmetics(hash);
                }
                config::save();
            });
        });
    });

    add_tab("Menu Colors", [this, &cosmetics](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Controlled);

        add_cosmetic_option(leftPane, rightPane, "Menu Accent Color", cosmetics.menuAccentColor);
        add_cosmetic_option(leftPane, rightPane, "Menu Background Color", cosmetics.menuBackgroundColor);
        add_cosmetic_option(leftPane, rightPane, "Menu Text Color", cosmetics.menuTextColor);
        add_cosmetic_option(leftPane, rightPane, "Menu Border Color", cosmetics.menuBorderColor);
    });

'''
        cosmetics_cpp = replace_once(
            cosmetics_cpp, tabs_anchor, tabs + tabs_anchor,
            "randomization and menu color tabs",
        )
    cosmetics_cpp_path.write_text(cosmetics_cpp, encoding="utf-8")

    ui_cpp = ui_cpp_path.read_text(encoding="utf-8")
    if '#include "cosmetics.hpp"' not in ui_cpp:
        ui_cpp = replace_once(
            ui_cpp,
            '#include "aurora/lib/window.hpp"\n',
            '#include "aurora/lib/window.hpp"\n#include "cosmetics.hpp"\n',
            "startup menu color theme include",
        )
    if "apply_menu_color_theme();" not in ui_cpp:
        ui_cpp = replace_once(
            ui_cpp,
            "    register_mod_texture_provider();\n    sInitialized = true;\n",
            "    register_mod_texture_provider();\n"
            "    apply_menu_color_theme();\n"
            "    sInitialized = true;\n",
            "startup menu color theme application",
        )
    ui_cpp_path.write_text(ui_cpp, encoding="utf-8")


def patch_controller_cursor_mode(source: Path) -> None:
    """Add SOH-style controller cursor mode to Dusklight's RmlUi input path.

    Hold L3+R3 for 0.35s while a Dusklight document or the tracker is visible.
    Right stick moves the pointer, A is left click, and holding A while moving
    performs drag. The overlay renders a high-contrast cursor and large ON/OFF
    confirmation. Cursor input is isolated to UI mode so gameplay is untouched
    whenever the mode is off.
    """
    input_hpp_path = source / "src/dusk/ui/input.hpp"
    input_cpp_path = source / "src/dusk/ui/input.cpp"
    overlay_hpp_path = source / "src/dusk/ui/overlay.hpp"
    overlay_cpp_path = source / "src/dusk/ui/overlay.cpp"
    overlay_rcss_path = source / "res/rml/overlay.rcss"

    input_hpp = input_hpp_path.read_text(encoding="utf-8")
    if "controller_cursor_active()" not in input_hpp:
        anchor = "void release_input_block() noexcept;\n"
        addition = (
            "void release_input_block() noexcept;\n\n"
            "// SOH-style controller-driven RmlUi pointer.\n"
            "bool controller_cursor_active() noexcept;\n"
            "bool controller_cursor_status_visible() noexcept;\n"
            "bool controller_cursor_pressed() noexcept;\n"
            "float controller_cursor_x() noexcept;\n"
            "float controller_cursor_y() noexcept;\n"
        )
        input_hpp = replace_once(input_hpp, anchor, addition, "controller cursor input API")
    input_hpp_path.write_text(input_hpp, encoding="utf-8")

    input_cpp = input_cpp_path.read_text(encoding="utf-8")

    if '#include "imgui.h"' not in input_cpp:
        input_cpp = replace_once(
            input_cpp,
            "#include <dolphin/pad.h>\n",
            "#include <dolphin/pad.h>\n#include \"imgui.h\"\n",
            "controller cursor ImGui input include",
        )
    if '#include "dusk/randomizer/game/randomizer_context.hpp"' not in input_cpp:
        input_cpp = replace_once(
            input_cpp,
            '#include "dusk/action_bindings.h"\n',
            '#include "dusk/action_bindings.h"\n'
            '#include "dusk/randomizer/game/randomizer_context.hpp"\n',
            "controller cursor tracker visibility include",
        )

    if "kControllerCursorToggleHold" not in input_cpp:
        anchor = "constexpr double kGamepadMenuChordGraceDuration = 0.12;\n"
        addition = (
            anchor
            + "constexpr double kControllerCursorToggleHold = 0.35;\n"
            + "constexpr double kControllerCursorStatusDuration = 1.35;\n"
            + "constexpr float kControllerCursorDeadzone = 0.18f;\n"
            + "constexpr float kControllerCursorSpeed = 1150.0f;\n"
        )
        input_cpp = replace_once(input_cpp, anchor, addition, "controller cursor constants")

    if "sControllerCursorActive" not in input_cpp:
        anchor = "TouchTapState sTouchMenuTap;\n"
        state = (
            anchor
            + "\n// TPR UWP / SOH-style controller cursor state.\n"
            + "bool sControllerCursorActive = false;\n"
            + "bool sControllerCursorL3Held = false;\n"
            + "bool sControllerCursorR3Held = false;\n"
            + "bool sControllerCursorChordConsumed = false;\n"
            + "bool sControllerCursorClickHeld = false;\n"
            + "bool sControllerCursorPositionInitialized = false;\n"
            + "double sControllerCursorChordStartedAt = 0.0;\n"
            + "double sControllerCursorStatusUntil = 0.0;\n"
            + "double sControllerCursorLastUpdateAt = 0.0;\n"
            + "Sint16 sControllerCursorAxisX = 0;\n"
            + "Sint16 sControllerCursorAxisY = 0;\n"
            + "float sControllerCursorX = 0.0f;\n"
            + "float sControllerCursorY = 0.0f;\n"
        )
        input_cpp = replace_once(input_cpp, anchor, state, "controller cursor state")

    if "void rumble_controller_cursor_toggle()" not in input_cpp:
        anchor = "double now_seconds() noexcept {\n    return static_cast<double>(SDL_GetTicksNS()) / 1000000000.0;\n}\n"
        helpers = r'''double now_seconds() noexcept {
    return static_cast<double>(SDL_GetTicksNS()) / 1000000000.0;
}

bool is_controller_cursor_chord_button(SDL_GamepadButton button) noexcept {
    return button == SDL_GAMEPAD_BUTTON_LEFT_STICK || button == SDL_GAMEPAD_BUTTON_RIGHT_STICK;
}

bool controller_cursor_tracker_visible() noexcept {
    return g_randomizerState.mShowTracker && randomizer_IsActive();
}

bool controller_cursor_target_visible() noexcept {
    return any_document_visible() || controller_cursor_tracker_visible();
}

void emit_controller_cursor_move(Rml::Context& context) noexcept {
    const int x = static_cast<int>(sControllerCursorX);
    const int y = static_cast<int>(sControllerCursorY);
    if (any_document_visible()) {
        context.ProcessMouseMove(x, y, 0);
        return;
    }
    if (controller_cursor_tracker_visible() && ImGui::GetCurrentContext() != nullptr) {
        ImGui::GetIO().AddMousePosEvent(static_cast<float>(x), static_cast<float>(y));
    }
}

void emit_controller_cursor_button(Rml::Context& context, bool down) noexcept {
    if (any_document_visible()) {
        if (down) {
            context.ProcessMouseButtonDown(0, 0);
        } else {
            context.ProcessMouseButtonUp(0, 0);
        }
        return;
    }
    if (controller_cursor_tracker_visible() && ImGui::GetCurrentContext() != nullptr) {
        ImGui::GetIO().AddMouseButtonEvent(0, down);
    }
}

void set_controller_cursor_chord_button(SDL_GamepadButton button, bool held) noexcept {
    if (button == SDL_GAMEPAD_BUTTON_LEFT_STICK) {
        sControllerCursorL3Held = held;
    } else if (button == SDL_GAMEPAD_BUTTON_RIGHT_STICK) {
        sControllerCursorR3Held = held;
    }

    if (sControllerCursorL3Held && sControllerCursorR3Held) {
        if (sControllerCursorChordStartedAt <= 0.0) {
            sControllerCursorChordStartedAt = now_seconds();
        }
    } else {
        sControllerCursorChordStartedAt = 0.0;
        sControllerCursorChordConsumed = false;
    }
}

void rumble_controller_cursor_toggle() noexcept {
    const s32 index = PADGetIndexForPort(PAD_CHAN0);
    if (index < 0) {
        return;
    }
    if (SDL_Gamepad* gamepad = PADGetSDLGamepadForIndex(static_cast<u32>(index))) {
        SDL_RumbleGamepad(gamepad, 0x3000, 0x6000, 120);
    }
}

float controller_cursor_axis_response(Sint16 raw) noexcept {
    const float normalized = std::clamp(static_cast<float>(raw) / 32767.0f, -1.0f, 1.0f);
    const float magnitude = std::abs(normalized);
    if (magnitude <= kControllerCursorDeadzone) {
        return 0.0f;
    }
    const float scaled = (magnitude - kControllerCursorDeadzone) / (1.0f - kControllerCursorDeadzone);
    // Slightly square the response for precision near center while retaining full-speed travel.
    const float shaped = scaled * scaled;
    return normalized < 0.0f ? -shaped : shaped;
}

void release_controller_cursor_click(Rml::Context& context) noexcept {
    if (!sControllerCursorClickHeld) {
        return;
    }
    emit_controller_cursor_button(context, false);
    sControllerCursorClickHeld = false;
}

void disable_controller_cursor(Rml::Context& context, bool showStatus) noexcept {
    if (!sControllerCursorActive) {
        return;
    }
    release_controller_cursor_click(context);
    sControllerCursorActive = false;
    sControllerCursorAxisX = 0;
    sControllerCursorAxisY = 0;
    if (any_document_visible()) {
        context.ProcessMouseLeave();
    }
    if (showStatus) {
        sControllerCursorStatusUntil = now_seconds() + kControllerCursorStatusDuration;
        rumble_controller_cursor_toggle();
    } else {
        sControllerCursorStatusUntil = 0.0;
    }
}

void update_controller_cursor(Rml::Context& context) noexcept {
    const double now = now_seconds();
    const bool uiVisible = controller_cursor_target_visible();

    // Cursor mode is deliberately UI-only. Closing the final Dusklight document
    // returns control to the game immediately and clears the toggle chord so a
    // held stick-click pair cannot arm itself across a menu close/reopen.
    if (!uiVisible) {
        if (sControllerCursorActive) {
            disable_controller_cursor(context, false);
        }
        sControllerCursorL3Held = false;
        sControllerCursorR3Held = false;
        sControllerCursorChordConsumed = false;
        sControllerCursorChordStartedAt = 0.0;
    }

    if (sControllerCursorL3Held && sControllerCursorR3Held &&
        !sControllerCursorChordConsumed && sControllerCursorChordStartedAt > 0.0 &&
        now - sControllerCursorChordStartedAt >= kControllerCursorToggleHold &&
        uiVisible)
    {
        sControllerCursorChordConsumed = true;
        if (sControllerCursorActive) {
            disable_controller_cursor(context, true);
        } else {
            sControllerCursorActive = true;
            sControllerCursorStatusUntil = now + kControllerCursorStatusDuration;
            rumble_controller_cursor_toggle();

            const auto dimensions = context.GetDimensions();
            if (!sControllerCursorPositionInitialized) {
                sControllerCursorX = static_cast<float>(dimensions.x) * 0.5f;
                sControllerCursorY = static_cast<float>(dimensions.y) * 0.5f;
                sControllerCursorPositionInitialized = true;
            }
            emit_controller_cursor_move(context);
        }
    }

    if (!sControllerCursorActive) {
        sControllerCursorLastUpdateAt = now;
        return;
    }

    const auto dimensions = context.GetDimensions();
    const double elapsed = sControllerCursorLastUpdateAt > 0.0 ? now - sControllerCursorLastUpdateAt : 0.0;
    const float dt = static_cast<float>(std::clamp(elapsed, 0.0, 0.05));
    sControllerCursorLastUpdateAt = now;

    const float dx = controller_cursor_axis_response(sControllerCursorAxisX) * kControllerCursorSpeed * dt;
    const float dy = controller_cursor_axis_response(sControllerCursorAxisY) * kControllerCursorSpeed * dt;
    if (dx == 0.0f && dy == 0.0f) {
        return;
    }

    const float maxX = static_cast<float>(std::max(dimensions.x - 1, 0));
    const float maxY = static_cast<float>(std::max(dimensions.y - 1, 0));
    sControllerCursorX = std::clamp(sControllerCursorX + dx, 0.0f, maxX);
    sControllerCursorY = std::clamp(sControllerCursorY + dy, 0.0f, maxY);
    emit_controller_cursor_move(context);
}
'''
        input_cpp = replace_once(input_cpp, anchor, helpers, "controller cursor helpers")

    if "#include <cmath>" not in input_cpp:
        input_cpp = replace_once(
            input_cpp,
            "#include <array>\n",
            "#include <array>\n#include <cmath>\n",
            "controller cursor cmath include",
        )

    axis_marker = "// TPR UWP: controller cursor right-stick routing"
    if axis_marker not in input_cpp:
        old_axis = '''    if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
        process_axis_direction(*context, event.gaxis, AXIS_SIGN_POSITIVE);
        process_axis_direction(*context, event.gaxis, AXIS_SIGN_NEGATIVE);
        sync_input_block();
        return;
    }
'''
        new_axis = '''    if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
        // TPR UWP: controller cursor right-stick routing.
        const auto nativeAxis = static_cast<SDL_GamepadAxis>(event.gaxis.axis);
        if (nativeAxis == SDL_GAMEPAD_AXIS_RIGHTX) {
            sControllerCursorAxisX = event.gaxis.value;
            if (sControllerCursorActive) {
                sync_input_block();
                return;
            }
        } else if (nativeAxis == SDL_GAMEPAD_AXIS_RIGHTY) {
            sControllerCursorAxisY = event.gaxis.value;
            if (sControllerCursorActive) {
                sync_input_block();
                return;
            }
        }

        process_axis_direction(*context, event.gaxis, AXIS_SIGN_POSITIVE);
        process_axis_direction(*context, event.gaxis, AXIS_SIGN_NEGATIVE);
        sync_input_block();
        return;
    }
'''
        input_cpp = replace_once(input_cpp, old_axis, new_axis, "controller cursor right-stick event routing")

        button_anchor = '''    auto* repeat = button_repeat_state(static_cast<SDL_GamepadButton>(event.gbutton.button));
    u32 port = 0;
'''
        button_block = '''    const auto nativeCursorButton = static_cast<SDL_GamepadButton>(event.gbutton.button);
    const bool cursorButtonDown = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;

    // L3+R3 is reserved for cursor mode while a Dusklight document or tracker is visible.
    if (is_controller_cursor_chord_button(nativeCursorButton) && controller_cursor_target_visible()) {
        set_controller_cursor_chord_button(nativeCursorButton, cursorButtonDown);
        sync_input_block();
        return;
    }

    // Xbox A is a true pointer left-click while cursor mode is active. Keeping the
    // button held while the right stick moves naturally becomes an RmlUi drag.
    if (sControllerCursorActive && nativeCursorButton == SDL_GAMEPAD_BUTTON_SOUTH) {
        if (cursorButtonDown && !sControllerCursorClickHeld) {
            emit_controller_cursor_button(*context, true);
            sControllerCursorClickHeld = true;
        } else if (!cursorButtonDown && sControllerCursorClickHeld) {
            emit_controller_cursor_button(*context, false);
            sControllerCursorClickHeld = false;
        }
        sync_input_block();
        return;
    }

    auto* repeat = button_repeat_state(static_cast<SDL_GamepadButton>(event.gbutton.button));
    u32 port = 0;
'''
        input_cpp = replace_once(input_cpp, button_anchor, button_block, "controller cursor button routing")

    if "update_controller_cursor(*context);" not in input_cpp:
        update_fn = "void update_input() noexcept {\n    auto* context = aurora::rmlui::get_context();\n    if (context != nullptr) {\n"
        replacement = (
            update_fn
            + "        update_controller_cursor(*context);\n"
            + "        sync_input_block();\n"
        )
        input_cpp = replace_once(input_cpp, update_fn, replacement, "controller cursor per-frame update")

    if "sControllerCursorActive && controller_cursor_tracker_visible()" not in input_cpp:
        input_cpp = replace_once(
            input_cpp,
            "    const bool shouldBlock = any_document_visible();\n",
            "    const bool shouldBlock = any_document_visible() ||\n"
            "        (sControllerCursorActive && controller_cursor_tracker_visible());\n",
            "controller cursor tracker gameplay block",
        )

    reset_marker = "// TPR UWP: reset controller cursor transient state"
    if reset_marker not in input_cpp:
        old_reset = (
            "void reset_input_state() noexcept {\n"
            "    clear_gamepad_repeats();\n"
            "    reset_touch_menu_tap();\n"
            "}\n"
        )
        new_reset = (
            "void reset_input_state() noexcept {\n"
            "    clear_gamepad_repeats();\n"
            "    reset_touch_menu_tap();\n\n"
            "    // TPR UWP: reset controller cursor transient state.\n"
            "    if (auto* context = aurora::rmlui::get_context()) {\n"
            "        release_controller_cursor_click(*context);\n"
            "        if (sControllerCursorActive) {\n"
            "            context->ProcessMouseLeave();\n"
            "        }\n"
            "    }\n"
            "    sControllerCursorActive = false;\n"
            "    sControllerCursorL3Held = false;\n"
            "    sControllerCursorR3Held = false;\n"
            "    sControllerCursorChordConsumed = false;\n"
            "    sControllerCursorChordStartedAt = 0.0;\n"
            "    sControllerCursorStatusUntil = 0.0;\n"
            "    sControllerCursorLastUpdateAt = 0.0;\n"
            "    sControllerCursorAxisX = 0;\n"
            "    sControllerCursorAxisY = 0;\n"
            "    sControllerCursorPositionInitialized = false;\n"
            "}\n"
        )
        input_cpp = replace_once(input_cpp, old_reset, new_reset, "controller cursor reset state")

    if "bool controller_cursor_active() noexcept" not in input_cpp:
        namespace_close = "\n}  // namespace dusk::ui\n"
        accessors = r'''
bool controller_cursor_active() noexcept {
    return sControllerCursorActive;
}

bool controller_cursor_status_visible() noexcept {
    return now_seconds() < sControllerCursorStatusUntil;
}

bool controller_cursor_pressed() noexcept {
    return sControllerCursorClickHeld;
}

float controller_cursor_x() noexcept {
    return sControllerCursorX;
}

float controller_cursor_y() noexcept {
    return sControllerCursorY;
}

}  // namespace dusk::ui
'''
        if not input_cpp.endswith(namespace_close):
            raise RuntimeError("Could not locate dusk::ui input namespace close for controller cursor accessors")
        input_cpp = input_cpp[: -len(namespace_close)] + "\n" + accessors

    input_cpp_path.write_text(input_cpp, encoding="utf-8")

    overlay_hpp = overlay_hpp_path.read_text(encoding="utf-8")
    if "mControllerCursor" not in overlay_hpp:
        anchor = "    Rml::Element* mMenuNotification = nullptr;\n"
        overlay_hpp = replace_once(
            overlay_hpp,
            anchor,
            anchor
            + "    Rml::Element* mControllerCursor = nullptr;\n"
            + "    Rml::Element* mControllerCursorStatus = nullptr;\n",
            "controller cursor overlay members",
        )
    overlay_hpp_path.write_text(overlay_hpp, encoding="utf-8")

    overlay_cpp = overlay_cpp_path.read_text(encoding="utf-8")
    if '#include "input.hpp"' not in overlay_cpp:
        overlay_cpp = replace_once(
            overlay_cpp,
            '#include "fmt/format.h"\n',
            '#include "fmt/format.h"\n#include "input.hpp"\n',
            "controller cursor overlay input include",
        )

    if 'controller-cursor id="controller-cursor"' not in overlay_cpp:
        anchor = '    <speedrun-timer id="speedrun-timer">\n'
        markup = (
            '    <controller-cursor id="controller-cursor" />\n'
            '    <cursor-mode-status id="cursor-mode-status" />\n'
        )
        overlay_cpp = replace_once(overlay_cpp, anchor, markup + anchor, "controller cursor overlay markup")

    if 'GetElementById("controller-cursor")' not in overlay_cpp:
        anchor = '    mSpeedrunTimer = mDocument->GetElementById("speedrun-timer");\n'
        overlay_cpp = replace_once(
            overlay_cpp,
            anchor,
            '    mControllerCursor = mDocument->GetElementById("controller-cursor");\n'
            '    mControllerCursorStatus = mDocument->GetElementById("cursor-mode-status");\n'
            + anchor,
            "controller cursor overlay lookup",
        )

    if "controller_cursor_status_visible()" not in overlay_cpp:
        anchor = '''    if (mFpsCounter != nullptr) {
'''
        update = r'''    if (mControllerCursor != nullptr) {
        if (input::controller_cursor_active()) {
            mControllerCursor->SetAttribute("open", "");
            if (input::controller_cursor_pressed()) {
                mControllerCursor->SetAttribute("pressed", "");
            } else {
                mControllerCursor->RemoveAttribute("pressed");
            }
            mControllerCursor->SetProperty(
                Rml::PropertyId::Left,
                Rml::Property(input::controller_cursor_x() - 10.0f, Rml::Unit::PX));
            mControllerCursor->SetProperty(
                Rml::PropertyId::Top,
                Rml::Property(input::controller_cursor_y() - 10.0f, Rml::Unit::PX));
        } else {
            mControllerCursor->RemoveAttribute("open");
            mControllerCursor->RemoveAttribute("pressed");
        }
    }

    if (mControllerCursorStatus != nullptr) {
        if (input::controller_cursor_status_visible()) {
            mControllerCursorStatus->SetInnerRML(
                input::controller_cursor_active() ? "CURSOR MODE ON" : "CURSOR MODE OFF");
            mControllerCursorStatus->SetAttribute("open", "");
        } else {
            mControllerCursorStatus->RemoveAttribute("open");
        }
    }

    if (mFpsCounter != nullptr) {
'''
        overlay_cpp = replace_once(overlay_cpp, anchor, update, "controller cursor overlay update")
    overlay_cpp_path.write_text(overlay_cpp, encoding="utf-8")

    overlay_rcss = overlay_rcss_path.read_text(encoding="utf-8")
    if "controller-cursor[open]" not in overlay_rcss:
        overlay_rcss += r'''

/* TPR UWP / SOH-style controller cursor. */
controller-cursor {
    display: none;
    position: absolute;
    width: 20dp;
    height: 20dp;
    z-index: 10000;
    pointer-events: none;
    border: 3dp #111111;
    border-radius: 10dp;
    background-color: #ffffff;
    box-shadow: 0 0 6dp 2dp rgba(0, 0, 0, 70%);
}

controller-cursor[open] {
    display: block;
}

controller-cursor[pressed] {
    background-color: #C2A42D;
    transform: scale(0.82);
}

cursor-mode-status {
    display: none;
    position: absolute;
    left: 50%;
    top: 34%;
    z-index: 10001;
    pointer-events: none;
    transform: translateX(-50%);
    padding: 16dp 28dp;
    border: 2dp #C2A42D;
    border-radius: 12dp;
    background-color: rgba(12, 13, 10, 88%);
    color: #ffffff;
    font-family: "Fira Sans Condensed";
    font-size: 34dp;
    font-weight: bold;
    text-align: center;
    white-space: nowrap;
}

cursor-mode-status[open] {
    display: block;
}
'''
    overlay_rcss_path.write_text(overlay_rcss, encoding="utf-8")


def patch_controller_cursor_polish(source: Path) -> None:
    # Add persistent speed/deadzone/acceleration tuning to CP15's cursor mode.
    settings_h_path = source / "src/dusk/settings.h"
    settings_cpp_path = source / "src/dusk/settings.cpp"
    ui_settings_path = source / "src/dusk/ui/settings.cpp"
    input_cpp_path = source / "src/dusk/ui/input.cpp"
    overlay_cpp_path = source / "src/dusk/ui/overlay.cpp"
    overlay_rcss_path = source / "res/rml/overlay.rcss"

    settings_h = settings_h_path.read_text(encoding="utf-8")
    if "ConfigVar<float> controllerCursorSpeed;" not in settings_h:
        settings_h = replace_once(
            settings_h,
            "        ConfigVar<bool> enableMenuPointer;\n",
            "        ConfigVar<bool> enableMenuPointer;\n"
            "        ConfigVar<float> controllerCursorSpeed;\n"
            "        ConfigVar<float> controllerCursorDeadzone;\n"
            "        ConfigVar<bool> controllerCursorAcceleration;\n",
            "controller cursor tuning declarations",
        )
    settings_h_path.write_text(settings_h, encoding="utf-8")

    settings_cpp = settings_cpp_path.read_text(encoding="utf-8")
    if '.controllerCursorSpeed {"game.controllerCursorSpeed", 1.0f}' not in settings_cpp:
        settings_cpp = replace_once(
            settings_cpp,
            '        .enableMenuPointer {"game.enableMenuPointer", true},\n',
            '        .enableMenuPointer {"game.enableMenuPointer", true},\n'
            '        .controllerCursorSpeed {"game.controllerCursorSpeed", 1.0f},\n'
            '        .controllerCursorDeadzone {"game.controllerCursorDeadzone", 0.18f},\n'
            '        .controllerCursorAcceleration {"game.controllerCursorAcceleration", true},\n',
            "controller cursor tuning defaults",
        )
    if "Register(g_userSettings.game.controllerCursorSpeed);" not in settings_cpp:
        settings_cpp = replace_once(
            settings_cpp,
            "    Register(g_userSettings.game.enableMenuPointer);\n",
            "    Register(g_userSettings.game.enableMenuPointer);\n"
            "    Register(g_userSettings.game.controllerCursorSpeed);\n"
            "    Register(g_userSettings.game.controllerCursorDeadzone);\n"
            "    Register(g_userSettings.game.controllerCursorAcceleration);\n",
            "controller cursor tuning registrations",
        )
    settings_cpp_path.write_text(settings_cpp, encoding="utf-8")

    ui_settings = ui_settings_path.read_text(encoding="utf-8")
    if '"Controller Cursor Speed"' not in ui_settings:
        anchor = (
            '        config_percent_select(leftPane, rightPane, getSettings().game.mouseCameraSensitivity,\n'
            '            "Mouse Camera Sensitivity", "Controls mouse camera sensitivity.", 25, 400, 5,\n'
            '            [] { return !getSettings().game.enableMouseCamera; });\n'
        )
        addition = r'''#if defined(_UWP)
        leftPane.add_section("Xbox Controller Cursor");
        config_percent_select(leftPane, rightPane, getSettings().game.controllerCursorSpeed,
            "Controller Cursor Speed",
            "Controls right-stick pointer speed while controller cursor mode is active.", 25, 250, 5);
        config_percent_select(leftPane, rightPane, getSettings().game.controllerCursorDeadzone,
            "Controller Cursor Deadzone",
            "Controls how far the right stick must move before the pointer responds.", 0, 50, 1);
        addOption("Controller Cursor Acceleration", getSettings().game.controllerCursorAcceleration,
            "Uses a precision curve near stick center while preserving full-speed movement at the edge.");
#endif
'''
        ui_settings = replace_once(
            ui_settings, anchor, anchor + addition, "controller cursor Settings controls"
        )
    ui_settings_path.write_text(ui_settings, encoding="utf-8")

    input_cpp = input_cpp_path.read_text(encoding="utf-8")
    if '#include "dusk/settings.h"' not in input_cpp:
        input_cpp = replace_once(
            input_cpp,
            '#include "dusk/action_bindings.h"\n',
            '#include "dusk/action_bindings.h"\n#include "dusk/settings.h"\n',
            "controller cursor settings include",
        )

    old_response = r'''float controller_cursor_axis_response(Sint16 raw) noexcept {
    const float normalized = std::clamp(static_cast<float>(raw) / 32767.0f, -1.0f, 1.0f);
    const float magnitude = std::abs(normalized);
    if (magnitude <= kControllerCursorDeadzone) {
        return 0.0f;
    }
    const float scaled = (magnitude - kControllerCursorDeadzone) / (1.0f - kControllerCursorDeadzone);
    // Slightly square the response for precision near center while retaining full-speed travel.
    const float shaped = scaled * scaled;
    return normalized < 0.0f ? -shaped : shaped;
}
'''
    new_response = r'''float controller_cursor_axis_response(Sint16 raw) noexcept {
    const float normalized = std::clamp(static_cast<float>(raw) / 32767.0f, -1.0f, 1.0f);
    const float magnitude = std::abs(normalized);
    const float deadzone = std::clamp(
        dusk::getSettings().game.controllerCursorDeadzone.getValue(), 0.0f, 0.50f);
    if (magnitude <= deadzone) {
        return 0.0f;
    }
    const float denominator = std::max(1.0f - deadzone, 0.001f);
    const float scaled = std::clamp((magnitude - deadzone) / denominator, 0.0f, 1.0f);
    const float shaped = dusk::getSettings().game.controllerCursorAcceleration.getValue()
                             ? scaled * scaled
                             : scaled;
    return normalized < 0.0f ? -shaped : shaped;
}
'''
    if "controllerCursorDeadzone.getValue()" not in input_cpp:
        input_cpp = replace_once(
            input_cpp, old_response, new_response, "controller cursor live deadzone/acceleration"
        )

    old_speed = (
        "    const float dx = controller_cursor_axis_response(sControllerCursorAxisX) * kControllerCursorSpeed * dt;\n"
        "    const float dy = controller_cursor_axis_response(sControllerCursorAxisY) * kControllerCursorSpeed * dt;\n"
    )
    new_speed = (
        "    const float cursorSpeed = kControllerCursorSpeed * std::clamp(\n"
        "        dusk::getSettings().game.controllerCursorSpeed.getValue(), 0.25f, 2.50f);\n"
        "    const float dx = controller_cursor_axis_response(sControllerCursorAxisX) * cursorSpeed * dt;\n"
        "    const float dy = controller_cursor_axis_response(sControllerCursorAxisY) * cursorSpeed * dt;\n"
    )
    if "controllerCursorSpeed.getValue()" not in input_cpp:
        input_cpp = replace_once(input_cpp, old_speed, new_speed, "controller cursor live speed")
    input_cpp_path.write_text(input_cpp, encoding="utf-8")

    overlay_cpp = overlay_cpp_path.read_text(encoding="utf-8")
    overlay_cpp = overlay_cpp.replace(
        "Rml::Property(input::controller_cursor_x() - 10.0f, Rml::Unit::PX)",
        "Rml::Property(input::controller_cursor_x() - 12.0f, Rml::Unit::PX)",
    )
    overlay_cpp = overlay_cpp.replace(
        "Rml::Property(input::controller_cursor_y() - 10.0f, Rml::Unit::PX)",
        "Rml::Property(input::controller_cursor_y() - 12.0f, Rml::Unit::PX)",
    )
    overlay_cpp_path.write_text(overlay_cpp, encoding="utf-8")

    overlay_rcss = overlay_rcss_path.read_text(encoding="utf-8")
    overlay_rcss = overlay_rcss.replace("    width: 20dp;\n    height: 20dp;", "    width: 24dp;\n    height: 24dp;")
    overlay_rcss = overlay_rcss.replace("    border-radius: 10dp;", "    border-radius: 12dp;")
    overlay_rcss = overlay_rcss.replace(
        "    box-shadow: 0 0 6dp 2dp rgba(0, 0, 0, 70%);",
        "    box-shadow: 0 0 8dp 3dp rgba(0, 0, 0, 85%);",
    )
    overlay_rcss_path.write_text(overlay_rcss, encoding="utf-8")



def patch_randomizer_tracker_window(source: Path) -> None:
    """Expose Dusklight's built-in tracker as a controller-friendly Xbox window.

    The pinned randomizer already tracks inventory, dungeon items, collected
    checks, reachable checks and location requirements directly from the active
    seed/save. This patch keeps that native logic, makes the popup movable and
    resizable, adds persistent opacity/scale/lock controls, and makes opening it
    request an immediate refresh.
    """
    settings_h_path = source / "src/dusk/settings.h"
    settings_cpp_path = source / "src/dusk/settings.cpp"
    tracker_cpp_path = source / "src/dusk/imgui/ImGuiMenuRandomizer.cpp"
    rando_config_path = source / "src/dusk/ui/rando_config.cpp"

    settings_h = settings_h_path.read_text(encoding="utf-8")
    if "ConfigVar<float> randomizerTrackerOpacity;" not in settings_h:
        settings_h = replace_once(
            settings_h,
            "        ConfigVar<bool> recordingMode;\n",
            "        ConfigVar<bool> recordingMode;\n"
            "        ConfigVar<float> randomizerTrackerOpacity;\n"
            "        ConfigVar<float> randomizerTrackerScale;\n"
            "        ConfigVar<bool> randomizerTrackerLock;\n",
            "randomizer tracker setting declarations",
        )
    settings_h_path.write_text(settings_h, encoding="utf-8")

    settings_cpp = settings_cpp_path.read_text(encoding="utf-8")
    if '.randomizerTrackerOpacity {"game.randomizerTrackerOpacity", 0.75f}' not in settings_cpp:
        settings_cpp = replace_once(
            settings_cpp,
            '        .recordingMode {"game.recordingMode", false},\n',
            '        .recordingMode {"game.recordingMode", false},\n'
            '        .randomizerTrackerOpacity {"game.randomizerTrackerOpacity", 0.75f},\n'
            '        .randomizerTrackerScale {"game.randomizerTrackerScale", 1.0f},\n'
            '        .randomizerTrackerLock {"game.randomizerTrackerLock", false},\n',
            "randomizer tracker setting defaults",
        )
    if "Register(g_userSettings.game.randomizerTrackerOpacity);" not in settings_cpp:
        settings_cpp = replace_once(
            settings_cpp,
            "    Register(g_userSettings.game.recordingMode);\n",
            "    Register(g_userSettings.game.recordingMode);\n"
            "    Register(g_userSettings.game.randomizerTrackerOpacity);\n"
            "    Register(g_userSettings.game.randomizerTrackerScale);\n"
            "    Register(g_userSettings.game.randomizerTrackerLock);\n",
            "randomizer tracker setting registrations",
        )
    settings_cpp_path.write_text(settings_cpp, encoding="utf-8")

    tracker_cpp = tracker_cpp_path.read_text(encoding="utf-8")
    if '#include "dusk/config.hpp"' not in tracker_cpp:
        tracker_cpp = replace_once(
            tracker_cpp,
            '#include "dusk/data.hpp"\n',
            '#include "dusk/data.hpp"\n#include "dusk/config.hpp"\n#include "dusk/settings.h"\n',
            "randomizer tracker persistent settings includes",
        )
    if "#include <algorithm>" not in tracker_cpp:
        tracker_cpp = replace_once(
            tracker_cpp,
            "#include <mutex>\n",
            "#include <algorithm>\n#include <mutex>\n",
            "randomizer tracker algorithm include",
        )

    if "TPR UWP: resizable native randomizer tracker" not in tracker_cpp:
        old_window = r'''        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
        ImGui::SetNextWindowBgAlpha(0.5f);

        if (!ImGui::Begin("Rando Tracker", nullptr, windowFlags) || !randomizer_IsActive()) {
            ImGui::End();
            return;
        }

        auto trackerRando = getTrackerRando();
'''
        new_window = r'''        // TPR UWP: resizable native randomizer tracker.
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoFocusOnAppearing;
        if (getSettings().game.randomizerTrackerLock.getValue()) {
            windowFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
        }

        const ImVec2 trackerWorkSize = ImGui::GetMainViewport()->WorkSize;
        const ImVec2 trackerDefaultSize{
            std::min(760.0f, trackerWorkSize.x * 0.82f),
            std::min(700.0f, trackerWorkSize.y * 0.82f)};
        ImGui::SetNextWindowSize(trackerDefaultSize, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(420.0f, 320.0f),
            ImVec2(std::max(420.0f, trackerWorkSize.x), std::max(320.0f, trackerWorkSize.y)));
        ImGui::SetNextWindowBgAlpha(std::clamp(
            getSettings().game.randomizerTrackerOpacity.getValue(), 0.25f, 1.0f));

        if (!ImGui::Begin(
                "Twilight Princess Item & Check Tracker",
                &g_randomizerState.mShowTracker,
                windowFlags) ||
            !randomizer_IsActive())
        {
            ImGui::End();
            return;
        }

        ImGui::SetWindowFontScale(std::clamp(
            getSettings().game.randomizerTrackerScale.getValue(), 0.75f, 1.50f));

        if (ImGui::CollapsingHeader("Tracker Window Settings")) {
            bool trackerLocked = getSettings().game.randomizerTrackerLock.getValue();
            if (ImGui::Checkbox("Lock Position and Size", &trackerLocked)) {
                getSettings().game.randomizerTrackerLock.setValue(trackerLocked);
                config::save();
            }

            float trackerOpacity =
                getSettings().game.randomizerTrackerOpacity.getValue() * 100.0f;
            if (ImGui::SliderFloat("Opacity", &trackerOpacity, 25.0f, 100.0f, "%.0f%%")) {
                getSettings().game.randomizerTrackerOpacity.setValue(trackerOpacity / 100.0f);
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                config::save();
            }

            float trackerScale = getSettings().game.randomizerTrackerScale.getValue() * 100.0f;
            if (ImGui::SliderFloat("Content Scale", &trackerScale, 75.0f, 150.0f, "%.0f%%")) {
                getSettings().game.randomizerTrackerScale.setValue(trackerScale / 100.0f);
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                config::save();
            }

            if (ImGui::Button("Reset Window Layout")) {
                ImGui::SetWindowSize(trackerDefaultSize, ImGuiCond_Always);
                ImGui::SetWindowPos(
                    ImVec2(
                        std::max(0.0f, (trackerWorkSize.x - trackerDefaultSize.x) * 0.5f),
                        std::max(0.0f, (trackerWorkSize.y - trackerDefaultSize.y) * 0.5f)),
                    ImGuiCond_Always);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("L3+R3 cursor | Right Stick move | A click/drag");
        }

        auto trackerRando = getTrackerRando();
'''
        tracker_cpp = replace_once(
            tracker_cpp, old_window, new_window, "resizable native randomizer tracker window"
        )

    tracker_cpp = tracker_cpp.replace(
        'if (ImGui::BeginChild("ScrollRegionCurInventory", ImVec2(500, 200), true)) {',
        'if (ImGui::BeginChild("ScrollRegionCurInventory", '
        'ImVec2(0, std::min(220.0f, ImGui::GetContentRegionAvail().y * 0.35f)), true)) {',
        1,
    )
    tracker_cpp = tracker_cpp.replace(
        'if (ImGui::BeginChild("ScrollRegion", ImVec2(500, 500), true))',
        'if (ImGui::BeginChild("ScrollRegion", ImVec2(0, 0), true))',
        1,
    )
    tracker_cpp = tracker_cpp.replace(
        '            ImGui::Checkbox("Show Rando Tracker", &g_randomizerState.mShowTracker);',
        '            if (ImGui::Checkbox("Show Rando Tracker", &g_randomizerState.mShowTracker) &&\n'
        '                g_randomizerState.mShowTracker)\n'
        '            {\n'
        '                g_randomizerState.mUpdateTracker = true;\n'
        '            }',
        1,
    )
    tracker_cpp_path.write_text(tracker_cpp, encoding="utf-8")

    rando_config = rando_config_path.read_text(encoding="utf-8")
    old_toggle = r'''            leftPane.add_button("Toggle Tracker Window").on_pressed([] {
                g_randomizerState.mShowTracker = !g_randomizerState.mShowTracker;
            });
'''
    new_toggle = r'''            leftPane.add_button("Toggle Item & Check Tracker").on_pressed([] {
                g_randomizerState.mShowTracker = !g_randomizerState.mShowTracker;
                if (g_randomizerState.mShowTracker) {
                    g_randomizerState.mUpdateTracker = true;
                }
            });
'''
    if 'leftPane.add_button("Toggle Item & Check Tracker")' not in rando_config:
        rando_config = replace_once(
            rando_config, old_toggle, new_toggle, "controller-friendly tracker toggle"
        )
    rando_config_path.write_text(rando_config, encoding="utf-8")


def patch_primary_input_viewer(source: Path) -> None:
    """Install the primary layered, resizable GameCube/TP input viewer.

    The live controller backend remains the pinned .627 implementation. Only
    presentation and persistent viewer options are replaced. The template keeps
    the old compact renderer as an automatic invalid-bounds fallback, so this
    cosmetic overlay cannot block gameplay if the window geometry is unusable.
    """
    settings_h_path = source / "src/dusk/settings.h"
    settings_cpp_path = source / "src/dusk/settings.cpp"
    settings_ui_path = source / "src/dusk/ui/settings.cpp"
    tools_header_path = source / "src/dusk/imgui/ImGuiMenuTools.hpp"
    overlay_path = source / "src/dusk/imgui/ImGuiControllerOverlay.cpp"
    overlay_template = KIT_ROOT / "patches" / "ImGuiControllerOverlay.cpp"

    if not overlay_template.exists():
        raise RuntimeError(f"Primary input-viewer template is missing: {overlay_template}")

    settings_h = settings_h_path.read_text(encoding="utf-8")
    if "ConfigVar<bool> inputViewerBackground;" not in settings_h:
        settings_h = replace_once(
            settings_h,
            "        ConfigVar<bool> showInputViewerGyro;\n",
            "        ConfigVar<bool> showInputViewerGyro;\n"
            "        ConfigVar<bool> inputViewerBackground;\n"
            "        ConfigVar<bool> inputViewerOutline;\n"
            "        ConfigVar<bool> inputViewerSticks;\n"
            "        ConfigVar<bool> inputViewerAnalogValues;\n"
            "        ConfigVar<bool> inputViewerLock;\n"
            "        ConfigVar<bool> inputViewerResetSizeRequested;\n",
            "primary input-viewer setting declarations",
        )
    settings_h_path.write_text(settings_h, encoding="utf-8")

    settings_cpp = settings_cpp_path.read_text(encoding="utf-8")
    if '.inputViewerBackground {"game.inputViewerBackground", true}' not in settings_cpp:
        settings_cpp = replace_once(
            settings_cpp,
            '        .showInputViewerGyro {"game.showInputViewerGyro", false}\n',
            '        .showInputViewerGyro {"game.showInputViewerGyro", false},\n'
            '        .inputViewerBackground {"game.inputViewerBackground", true},\n'
            '        .inputViewerOutline {"game.inputViewerOutline", true},\n'
            '        .inputViewerSticks {"game.inputViewerSticks", true},\n'
            '        .inputViewerAnalogValues {"game.inputViewerAnalogValues", false},\n'
            '        .inputViewerLock {"game.inputViewerLock", false},\n'
            '        .inputViewerResetSizeRequested {"game.inputViewerResetSizeRequested", false}\n',
            "primary input-viewer setting defaults",
        )
    if "Register(g_userSettings.game.inputViewerBackground);" not in settings_cpp:
        settings_cpp = replace_once(
            settings_cpp,
            "    Register(g_userSettings.game.showInputViewerGyro);\n",
            "    Register(g_userSettings.game.showInputViewerGyro);\n"
            "    Register(g_userSettings.game.inputViewerBackground);\n"
            "    Register(g_userSettings.game.inputViewerOutline);\n"
            "    Register(g_userSettings.game.inputViewerSticks);\n"
            "    Register(g_userSettings.game.inputViewerAnalogValues);\n"
            "    Register(g_userSettings.game.inputViewerLock);\n"
            "    Register(g_userSettings.game.inputViewerResetSizeRequested);\n",
            "primary input-viewer setting registrations",
        )
    settings_cpp_path.write_text(settings_cpp, encoding="utf-8")

    settings_ui = settings_ui_path.read_text(encoding="utf-8")
    marker = '            .key = "Input Viewer Background",'
    if marker not in settings_ui:
        old_controls = r'''        config_bool_select(leftPane, rightPane, getSettings().game.showInputViewer,
            {
                .key = "Show Input Viewer",
                .helpText = "Display a controller input overlay while playing.",
            });
        config_bool_select(leftPane, rightPane, getSettings().game.showInputViewerGyro,
            {
                .key = "Show Gyro Input Viewer",
                .helpText = "Show gyro sensor values in the input viewer.",
                .isDisabled = [] { return !getSettings().game.showInputViewer; },
            });
'''
        new_controls = r'''        config_bool_select(leftPane, rightPane, getSettings().game.showInputViewer,
            {
                .key = "Show Input Viewer",
                .helpText = "Display the primary layered controller overlay while playing. "
                            "The window can be moved and resized; right-click it for quick layout options.",
            });
        config_bool_select(leftPane, rightPane, getSettings().game.showInputViewerGyro,
            {
                .key = "Show Gyro Input Viewer",
                .helpText = "Show gyro sensor values in the input viewer.",
                .isDisabled = [] { return !getSettings().game.showInputViewer; },
            });
        config_bool_select(leftPane, rightPane, getSettings().game.inputViewerBackground,
            {
                .key = "Input Viewer Background",
                .helpText = "Show the dark presentation panel behind the controller.",
                .isDisabled = [] { return !getSettings().game.showInputViewer; },
            });
        config_bool_select(leftPane, rightPane, getSettings().game.inputViewerOutline,
            {
                .key = "Input Viewer Gold Outline",
                .helpText = "Draw the Twilight Princess-inspired gold outline and control borders.",
                .isDisabled = [] { return !getSettings().game.showInputViewer; },
            });
        config_bool_select(leftPane, rightPane, getSettings().game.inputViewerSticks,
            {
                .key = "Show Input Viewer Sticks",
                .helpText = "Show both analog sticks and their live travel.",
                .isDisabled = [] { return !getSettings().game.showInputViewer; },
            });
        config_bool_select(leftPane, rightPane, getSettings().game.inputViewerAnalogValues,
            {
                .key = "Show Input Viewer Analog Values",
                .helpText = "Show numeric stick positions and analog trigger travel.",
                .isDisabled = [] { return !getSettings().game.showInputViewer; },
            });
        config_bool_select(leftPane, rightPane, getSettings().game.inputViewerLock,
            {
                .key = "Lock Input Viewer Position and Size",
                .helpText = "Prevent accidental movement or resizing while recording or streaming.",
                .isDisabled = [] { return !getSettings().game.showInputViewer; },
            });
        auto& resetInputViewerSize = leftPane.add_button(ControlledButton::Props{
            .text = "Reset Input Viewer Size",
            .isDisabled = [] { return !getSettings().game.showInputViewer; },
        });
        leftPane.register_control(
            resetInputViewerSize.on_pressed([] {
                mDoAud_seStartMenu(kSoundItemChange);
                getSettings().game.inputViewerResetSizeRequested.setValue(true);
                config::save();
            }),
            rightPane, [](Pane& pane) {
                pane.clear();
                pane.add_text("Restore the input viewer to its safe default size.");
            });
'''
        settings_ui = replace_once(
            settings_ui, old_controls, new_controls, "primary input-viewer settings controls"
        )
    settings_ui_path.write_text(settings_ui, encoding="utf-8")

    tools_header = tools_header_path.read_text(encoding="utf-8")
    tools_header = tools_header.replace(
        "        int m_inputOverlayCorner = 3;",
        "        int m_inputOverlayCorner = -1; // free-positioned and resizable by default",
        1,
    )
    tools_header_path.write_text(tools_header, encoding="utf-8")

    shutil.copy2(overlay_template, overlay_path)


def patch_seed_data_io_safety(source: Path) -> None:
    """Make generated seed files durable and malformed seed selection non-fatal.

    std::ofstream can create an empty file in the UWP sandbox without surfacing a
    useful failure through the existing code. Build the payload in memory, write
    it through Dusklight's checked FileStream implementation, verify it byte for
    byte, and only then move it into place. Seed loading is kept entirely inside
    a catch boundary because RmlUi dispatches the selection callback from a
    noexcept event path.
    """
    context_path = source / "src/dusk/randomizer/game/randomizer_context.cpp"
    rml_generation_path = source / "src/dusk/ui/rando_seed_generation.cpp"
    imgui_generation_path = source / "src/dusk/imgui/ImGuiMenuRandomizer.cpp"
    input_path = source / "src/dusk/ui/input.cpp"
    tracker_path = source / "src/dusk/imgui/ImGuiMenuRandomizer.cpp"

    context = context_path.read_text(encoding="utf-8")
    marker = "TPR UWP: checked transactional seed.dat write"
    if marker not in context:
        if '#include "dusk/io.hpp"' not in context:
            context = replace_once(
                context,
                '#include "dusk/logging.h"\n',
                '#include "dusk/io.hpp"\n#include "dusk/logging.h"\n',
                "checked seed data I/O include",
            )
        if '#include "dusk/ui/cosmetics.hpp"' not in context:
            context = replace_once(
                context,
                '#include "dusk/ui/rando_config.hpp"\n',
                '#include "dusk/ui/rando_config.hpp"\n#include "dusk/ui/cosmetics.hpp"\n',
                "per-seed cosmetic application include",
            )

        context = replace_once(
            context,
            "std::optional<std::string> RandomizerContext::WriteToFile() {\n\n"
            "    std::ofstream seedData(this->GetSeedDataPath());\n"
            "    if (!seedData.is_open()) {\n"
            "        return \"Could not open seed data file\";\n"
            "    }\n\n"
            "    YAML::Node out{};\n",
            "std::optional<std::string> RandomizerContext::WriteToFile() {\n\n"
            "    YAML::Node out{};\n",
            "unchecked seed data stream open",
        )
        context = context.replace(
            "#include <fstream>\n",
            "#include <stdexcept>\n#include <system_error>\n",
            1,
        )

        old_write_tail = r'''    seedData << YAML::Dump(out);
    seedData << '\n' << textData.c_str();
    seedData.close();

    return std::nullopt;
}
'''
        new_write_tail = r'''    // TPR UWP: checked transactional seed.dat write. Never advertise a
    // generated seed until the exact non-empty payload is durable and readable.
    std::string payload = YAML::Dump(out);
    payload.push_back('\n');
    payload.append(textData.c_str());
    if (payload.empty()) {
        return "Generated seed data payload is empty";
    }

    const std::filesystem::path seedPath = this->GetSeedDataPath();
    std::filesystem::path temporaryPath = seedPath;
    temporaryPath += ".tmp";
    std::error_code cleanupError;

    try {
        std::filesystem::remove(temporaryPath, cleanupError);
        cleanupError.clear();

        dusk::io::FileStream::WriteAllText(temporaryPath, payload);
        const auto temporaryBytes = dusk::io::FileStream::ReadAllBytes(temporaryPath);
        const std::string temporaryCopy(temporaryBytes.begin(), temporaryBytes.end());
        if (temporaryCopy.empty() || temporaryCopy != payload) {
            throw std::runtime_error("Seed data verification failed after temporary write");
        }

        // std::filesystem::rename cannot replace an existing destination on
        // Windows. The verified temporary file remains intact until this point.
        std::filesystem::remove(seedPath, cleanupError);
        if (cleanupError) {
            throw std::system_error(cleanupError, "Could not replace existing seed data file");
        }
        std::filesystem::rename(temporaryPath, seedPath);

        const auto committedBytes = dusk::io::FileStream::ReadAllBytes(seedPath);
        const std::string committedCopy(committedBytes.begin(), committedBytes.end());
        if (committedCopy.empty() || committedCopy != payload) {
            throw std::runtime_error("Seed data verification failed after commit");
        }

        DuskLog.info(
            "Committed verified seed data '{}' ({} bytes)",
            dusk::io::fs_path_to_string(seedPath), committedBytes.size());
        return std::nullopt;
    } catch (const std::exception& error) {
        std::filesystem::remove(temporaryPath, cleanupError);
        DuskLog.error(
            "Failed to commit seed data '{}': {}",
            dusk::io::fs_path_to_string(seedPath), error.what());
        return fmt::format("Could not safely save seed data: {}", error.what());
    } catch (...) {
        std::filesystem::remove(temporaryPath, cleanupError);
        DuskLog.error(
            "Failed to commit seed data '{}' because of an unknown error",
            dusk::io::fs_path_to_string(seedPath));
        return "Could not safely save seed data: unknown error";
    }
}
'''
        context = replace_once(
            context, old_write_tail, new_write_tail, "checked transactional seed data write"
        )

        old_load_head = r'''std::optional<std::string> RandomizerContext::LoadFromHash(const std::string& hash) {
    this->mHash = hash;

    if (!std::filesystem::exists(this->GetSeedDataPath())) {
        DuskLog.error("Failed to load Hash: {}", hash);
        mHash.clear();
        return std::nullopt;
    }

    auto in = LoadYAML(this->GetSeedDataPath());
'''
        new_load_head = r'''std::optional<std::string> RandomizerContext::LoadFromHash(const std::string& hash) {
    // Seed selection is invoked by noexcept UI event dispatch. No malformed or
    // incomplete on-disk seed is allowed to throw beyond this function.
    *this = RandomizerContext{};
    this->mHash = hash;
    const std::filesystem::path seedPath = this->GetSeedDataPath();

    auto failSeedLoad = [this, &hash](const std::string& reason) -> std::optional<std::string> {
        DuskLog.error("Failed to load randomizer seed '{}': {}", hash, reason);
        *this = RandomizerContext{};
        dusk::ui::push_toast(dusk::ui::Toast{
            .title = "Randomizer Seed Error",
            .content = reason,
            .duration = std::chrono::seconds(7),
        });
        return reason;
    };

    try {
        const auto seedBytes = dusk::io::FileStream::ReadAllBytes(seedPath);
        if (seedBytes.empty()) {
            return failSeedLoad(
                "Seed data file is empty. Delete this seed and generate it again.");
        }

        DuskLog.info(
            "Loading randomizer seed '{}' from '{}' ({} bytes)", hash,
            dusk::io::fs_path_to_string(seedPath), seedBytes.size());
        const std::string seedText(seedBytes.begin(), seedBytes.end());
        auto in = YAML::Load(seedText);
        if (!in.IsMap() || !in["mSettings"].IsMap()) {
            throw std::runtime_error("Seed data is missing its required settings map");
        }
'''
        context = replace_once(
            context, old_load_head, new_load_head, "non-fatal validated seed data load"
        )

        old_load_tail = r'''    dusk::ui::push_toast(dusk::ui::Toast{
        .title = "Randomizer",
        .content =  fmt::format("Loaded Randomizer Seed {}", this->mHash),
        .duration = std::chrono::seconds(3),
    });
    return std::nullopt;
}
'''
        new_load_tail = r'''        dusk::ui::apply_per_seed_cosmetics(this->mHash);
        dusk::ui::push_toast(dusk::ui::Toast{
            .title = "Randomizer",
            .content = fmt::format("Loaded Randomizer Seed {}", this->mHash),
            .duration = std::chrono::seconds(3),
        });
        return std::nullopt;
    } catch (const std::exception& error) {
        return failSeedLoad(fmt::format("Seed data is invalid or incomplete: {}", error.what()));
    } catch (...) {
        return failSeedLoad("Seed data is invalid or incomplete: unknown error");
    }
}
'''
        context = replace_once(
            context, old_load_tail, new_load_tail, "seed load exception boundary"
        )

    context_path.write_text(context, encoding="utf-8")

    rml_generation = rml_generation_path.read_text(encoding="utf-8")
    if "Seed generation stopped safely" not in rml_generation:
        old_rml_start = r'''static void StartSeedGeneration() {
    if (GenerateAndWriteSeed(generationStatusMsg)) {
        seedGenStatus.store(SeedGenerateStatus::Success);
    } else {
        seedGenStatus.store(SeedGenerateStatus::Error);
    }

    DuskLog.debug("{}", generationStatusMsg);
}
'''
        new_rml_start = r'''static void StartSeedGeneration() {
    try {
        if (GenerateAndWriteSeed(generationStatusMsg)) {
            seedGenStatus.store(SeedGenerateStatus::Success);
        } else {
            seedGenStatus.store(SeedGenerateStatus::Error);
        }
    } catch (const std::exception& error) {
        generationStatusMsg =
            std::string("Seed generation stopped safely. Reason:\n") + error.what();
        seedGenStatus.store(SeedGenerateStatus::Error);
    } catch (...) {
        generationStatusMsg = "Seed generation stopped safely. Reason:\nUnknown error";
        seedGenStatus.store(SeedGenerateStatus::Error);
    }

    DuskLog.debug("{}", generationStatusMsg);
}
'''
        rml_generation = replace_once(
            rml_generation, old_rml_start, new_rml_start, "RmlUi seed generation exception boundary"
        )
    rml_generation_path.write_text(rml_generation, encoding="utf-8")

    imgui_generation = imgui_generation_path.read_text(encoding="utf-8")
    if "Seed generation stopped safely" not in imgui_generation:
        old_imgui_start = r'''        generatingSeed = true;
        std::lock_guard lock(generationStatusMsgMutex);
        GenerateAndWriteSeed(generationStatusMsg);
        generatingSeed = false;
        DuskLog.debug("{}", generationStatusMsg);
'''
        new_imgui_start = r'''        generatingSeed = true;
        std::lock_guard lock(generationStatusMsgMutex);
        try {
            GenerateAndWriteSeed(generationStatusMsg);
        } catch (const std::exception& error) {
            generationStatusMsg =
                std::string("Seed generation stopped safely. Reason:\n") + error.what();
        } catch (...) {
            generationStatusMsg = "Seed generation stopped safely. Reason:\nUnknown error";
        }
        generatingSeed = false;
        DuskLog.debug("{}", generationStatusMsg);
'''
        imgui_generation = replace_once(
            imgui_generation, old_imgui_start, new_imgui_start, "ImGui seed generation exception boundary"
        )
    imgui_generation_path.write_text(imgui_generation, encoding="utf-8")

    # A selected seed is considered active while the game creates a new save.
    # Keep the optional tracker/cursor path out of that transition entirely.
    input_cpp = input_path.read_text(encoding="utf-8")
    input_cpp = input_cpp.replace(
        "return g_randomizerState.mShowTracker && randomizer_IsActive();",
        "return g_randomizerState.mShowTracker && g_randomizerState.mInitialized &&\n"
        "        !randomizer_GetContext().mCreatingSave && randomizer_IsActive();",
        1,
    )
    input_path.write_text(input_cpp, encoding="utf-8")

    tracker_cpp = tracker_path.read_text(encoding="utf-8")
    tracker_cpp = tracker_cpp.replace(
        "        if (!g_randomizerState.mShowTracker) {\n            return;\n        }",
        "        if (!g_randomizerState.mShowTracker || !g_randomizerState.mInitialized ||\n"
        "            randomizer_GetContext().mCreatingSave)\n"
        "        {\n            return;\n        }",
        1,
    )
    tracker_path.write_text(tracker_cpp, encoding="utf-8")


def patch_uwp_wrapper(source: Path) -> None:
    path = source / "platforms/uwp/CMakeLists.txt"
    if not path.exists():
        raise RuntimeError("UWP project was not imported: platforms/uwp/CMakeLists.txt is missing")
    text = path.read_text(encoding="utf-8")

    # Make the executable/project itself distinct too, not only the package identity.
    text = re.sub(r"project\([^\n]+LANGUAGES CXX\)", f"project({TARGET_NAME} LANGUAGES CXX)", text, count=1)

    helper_marker = "# Randomizer/new-core link compatibility"
    if helper_marker not in text:
        anchor = "set(BinLibs\n"
        helper = r'''# Randomizer/new-core link compatibility
# Later randomizer builds add static libraries that the 1.1.1 UWP wrapper predates.
function(tpr_find_built_lib OUT_VAR REGEX_TEXT)
    file(GLOB_RECURSE _tpr_candidates LIST_DIRECTORIES FALSE "${DUSK_DIR}/*.lib")
    list(FILTER _tpr_candidates INCLUDE REGEX "${REGEX_TEXT}")
    list(LENGTH _tpr_candidates _tpr_count)
    if(_tpr_count EQUAL 0)
        message(FATAL_ERROR "Required randomizer UWP feed library not found: ${REGEX_TEXT} under ${DUSK_DIR}")
    endif()
    list(GET _tpr_candidates 0 _tpr_first)
    set(${OUT_VAR} "${_tpr_first}" PARENT_SCOPE)
endfunction()

# Tracy's CMake target exists even when profiling is disabled, but with
# TRACY_ENABLE=OFF the client instrumentation macros compile out and the main
# static archive may not cause TracyClient.lib to be emitted in a target-only
# build. Only require/link the archive when the static feed was explicitly
# configured with Tracy profiling enabled.
set(TPR_TRACY_LIB "")
set(_tpr_tracy_enabled OFF)
if(EXISTS "${DUSK_DIR}/CMakeCache.txt")
    file(STRINGS "${DUSK_DIR}/CMakeCache.txt" _tpr_tracy_lines REGEX "^TRACY_ENABLE:BOOL=")
    foreach(_tpr_tracy_line IN LISTS _tpr_tracy_lines)
        if(_tpr_tracy_line STREQUAL "TRACY_ENABLE:BOOL=ON")
            set(_tpr_tracy_enabled ON)
        endif()
    endforeach()
endif()
if(_tpr_tracy_enabled)
    tpr_find_built_lib(TPR_TRACY_LIB "/TracyClient[^/]*[.]lib$")
else()
    message(STATUS "Randomizer feed has TRACY_ENABLE=OFF; TracyClient.lib is not required for UWP link")
endif()

tpr_find_built_lib(TPR_YAML_CPP_LIB "/yaml-cpp[.]lib$")
tpr_find_built_lib(TPR_BASE64PP_LIB "/base64pp[.]lib$")

'''
        text = replace_once(text, anchor, helper + anchor, "UWP extra-library resolver")

    if "${TPR_TRACY_LIB}" not in text.split("set(BinLibs", 1)[1]:
        target = "\t${DUSK_DIR}/dusklight.lib\t\n"
        replacement = (
            target
            + "\t${TPR_TRACY_LIB}\n"
            + "\t${TPR_YAML_CPP_LIB}\n"
            + "\t${TPR_BASE64PP_LIB}\n"
            + "\tWs2_32.lib\n"
        )
        text = replace_once(text, target, replacement, "randomizer UWP libraries")

    tile_assets = "\tAssets/Wide310x150Logo.scale-200.png\n\tAssets/Square310x310Logo.scale-200.png\n"
    if "Wide310x150Logo.scale-200.png" not in text:
        anchor = "\tAssets/SplashScreen.scale-200.png\n"
        text = replace_once(text, anchor, anchor + tile_assets, "Xbox tile assets")

    path.write_text(text, encoding="utf-8")


def patch_manifest(source: Path) -> None:
    path = source / "platforms/uwp/Package.appxmanifest"
    text = path.read_text(encoding="utf-8-sig")

    substitutions = [
        (r'Name="[^"]+"\s*\n\s*Publisher="[^"]+"\s*\n\s*Version="[^"]+"',
         f'Name="{PACKAGE_NAME}"\n    Publisher="{PUBLISHER}"\n    Version="{PACKAGE_VERSION}"'),
        (r'PhoneProductId="[^"]+"', f'PhoneProductId="{PHONE_PRODUCT_ID}"'),
        (r'<DisplayName>.*?</DisplayName>', f'<DisplayName>{DISPLAY_NAME}</DisplayName>'),
        (r'<PublisherDisplayName>.*?</PublisherDisplayName>', f'<PublisherDisplayName>{PUBLISHER_DISPLAY}</PublisherDisplayName>'),
        (r'DisplayName="[^"]+"', f'DisplayName="{DISPLAY_NAME}"'),
        (r'Description="[^"]+"', f'Description="{DESCRIPTION}"'),
    ]
    for pattern, repl in substitutions:
        text, count = re.subn(pattern, repl, text, count=1, flags=re.S)
        if count != 1:
            raise RuntimeError(f"Could not patch manifest field: {pattern}")

    # Prefer the user's full 16:9 artwork for wide Xbox presentation when supported.
    if "Wide310x150Logo" not in text:
        text = text.replace("<uap:DefaultTile/>", '<uap:DefaultTile Wide310x150Logo="Assets\\Wide310x150Logo.png" Square310x310Logo="Assets\\Square310x310Logo.png"/>', 1)

    path.write_text(text, encoding="utf-8-sig")


def install_branding(source: Path) -> None:
    dest = source / "platforms/uwp/Assets"
    dest.mkdir(parents=True, exist_ok=True)
    required = [
        "LockScreenLogo.scale-200.png",
        "Square44x44Logo.targetsize-24_altform-unplated.png",
        "Square44x44Logo.scale-200.png",
        "Square150x150Logo.scale-200.png",
        "StoreLogo.png",
        "SplashScreen.scale-200.png",
        "Wide310x150Logo.scale-200.png",
        "Square310x310Logo.scale-200.png",
    ]
    for name in required:
        src = BRANDING_ASSETS / name
        if not src.exists():
            raise RuntimeError(f"Branding asset missing from kit: {src}")
        shutil.copy2(src, dest / name)



def assert_preprocessor_balanced(path: Path) -> None:
    stack: list[tuple[int, str]] = []
    for lineno, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        stripped = line.lstrip()
        if re.match(r"#\s*(if|ifdef|ifndef)\b", stripped):
            stack.append((lineno, stripped))
        elif re.match(r"#\s*endif\b", stripped):
            if not stack:
                raise RuntimeError(f"Unexpected #endif in {path} at line {lineno}")
            stack.pop()
    if stack:
        lineno, directive = stack[-1]
        raise RuntimeError(f"Unclosed preprocessor directive in {path} at line {lineno}: {directive}")


def patch_settings_uwp(source: Path) -> None:
    """Normalize the stale UWP settings guard onto the newer randomizer settings layout.

    SternXD's UWP fork added an outer _UWP guard around the updater/Discord settings
    while its older base also had a nearby conditional that no longer exists in the
    pinned randomizer tree. A whole-branch merge can therefore leave one orphan #endif.
    Rebuild only this small region from the merged content, preserving the current
    updater and Discord bodies while making the _UWP guard structurally balanced.
    """
    path = source / "src/dusk/ui/settings.cpp"
    text = path.read_text(encoding="utf-8")
    check_marker = "        config_bool_select(leftPane, rightPane, getSettings().backend.checkForUpdates,"
    advanced_marker = "        config_bool_select(leftPane, rightPane, getSettings().backend.enableAdvancedSettings,"
    check_pos = text.find(check_marker)
    advanced_pos = text.find(advanced_marker, check_pos if check_pos >= 0 else 0)
    if check_pos < 0 or advanced_pos < 0 or advanced_pos <= check_pos:
        raise RuntimeError("Could not locate updater/advanced settings region for UWP normalization")

    # Remove an already-merged outer _UWP opener immediately before the updater block.
    prefix = text[:check_pos]
    prefix_lines = prefix.splitlines(keepends=True)
    scan = len(prefix_lines) - 1
    while scan >= 0 and prefix_lines[scan].strip() == "":
        scan -= 1
    if scan >= 0 and prefix_lines[scan].strip() in {"#ifndef _UWP", "#if !defined(_UWP)"}:
        del prefix_lines[scan]
    prefix = "".join(prefix_lines)

    # Preserve all current randomizer code in the region, but discard only #endifs
    # that are unmatched within the region. Those are stale closers from the old
    # UWP base; nested directives such as DUSK_DISCORD remain intact.
    region = text[check_pos:advanced_pos]
    depth = 0
    cleaned: list[str] = []
    removed_unmatched = 0
    for line in region.splitlines(keepends=True):
        stripped = line.lstrip()
        if re.match(r"#\s*(if|ifdef|ifndef)\b", stripped):
            depth += 1
            cleaned.append(line)
        elif re.match(r"#\s*endif\b", stripped):
            if depth > 0:
                depth -= 1
                cleaned.append(line)
            else:
                removed_unmatched += 1
        else:
            cleaned.append(line)
    if depth != 0:
        raise RuntimeError("Updater/Discord settings region contains an unclosed nested preprocessor guard")

    body = "".join(cleaned).rstrip() + "\n"
    normalized = prefix + "#ifndef _UWP\n" + body + "#endif\n" + text[advanced_pos:]
    path.write_text(normalized, encoding="utf-8")
    assert_preprocessor_balanced(path)
    print(f"Normalized UWP settings preprocessor guard; removed {removed_unmatched} stale #endif line(s)", flush=True)


def patch_direct_texture_zip_loading(source: Path) -> None:
    """Add one isolated feature: lazy, file-backed texture loading from ZIP files.

    The proven 1.4.1.627 code path for loose texture files remains unchanged. ZIP
    members are registered through Aurora's existing virtual-replacement API and
    decompressed individually on demand, so a multi-gigabyte pack is never loaded
    into memory as one blob.
    """
    path = source / "src/dusk/texture_replacements.cpp"
    if not path.exists():
        raise RuntimeError("Texture replacement source is missing")

    text = r'''#include "dusk/texture_replacements.hpp"

#include <aurora/texture.hpp>
#include <fmt/format.h>

#include "dusk/io.hpp"
#include "dusk/logging.h"
#include "dusk/main.h"
#include "dusk/settings.h"
#include "miniz.h"

#include <algorithm>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#if defined(_UWP)
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_iostream.h>
#endif

namespace dusk::texture_replacements {
namespace {
aurora::texture::ReplacementGroup s_directoryGroup;
aurora::texture::ReplacementGroup s_zipGroup;

unsigned char ascii_lower(unsigned char ch) noexcept {
    if (ch >= 'A' && ch <= 'Z') {
        return static_cast<unsigned char>(ch - 'A' + 'a');
    }
    return ch;
}

bool iequals_ascii(std::string_view lhs, std::string_view rhs) noexcept {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (ascii_lower(static_cast<unsigned char>(lhs[i])) !=
            ascii_lower(static_cast<unsigned char>(rhs[i])))
        {
            return false;
        }
    }
    return true;
}

int compare_ascii_ci(std::string_view lhs, std::string_view rhs) noexcept {
    const auto count = std::min(lhs.size(), rhs.size());
    for (std::size_t i = 0; i < count; ++i) {
        const auto lhsCh = ascii_lower(static_cast<unsigned char>(lhs[i]));
        const auto rhsCh = ascii_lower(static_cast<unsigned char>(rhs[i]));
        if (lhsCh != rhsCh) {
            return lhsCh < rhsCh ? -1 : 1;
        }
    }
    if (lhs.size() == rhs.size()) {
        return 0;
    }
    return lhs.size() < rhs.size() ? -1 : 1;
}

std::vector<std::string> virtual_path_components(std::string_view path) {
    std::vector<std::string> result;
    std::size_t offset = 0;
    while (offset <= path.size()) {
        const auto slash = path.find('/', offset);
        const auto count = slash == std::string_view::npos ? path.size() - offset : slash - offset;
        if (count != 0) {
            result.emplace_back(path.substr(offset, count));
        }
        if (slash == std::string_view::npos) {
            break;
        }
        offset = slash + 1;
    }
    return result;
}

bool is_sidecar_mip(std::string_view stem) noexcept {
    constexpr std::string_view tag = "_mip";
    std::size_t i = stem.size();
    while (i > 0 && stem[i - 1] >= '0' && stem[i - 1] <= '9') {
        --i;
    }
    return i != stem.size() && i >= tag.size() &&
           stem.substr(i - tag.size(), tag.size()) == tag;
}

std::string_view filename_of(std::string_view path) noexcept {
    const auto slash = path.rfind('/');
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

std::string_view extension_of(std::string_view path) noexcept {
    const auto filename = filename_of(path);
    const auto dot = filename.rfind('.');
    return dot == std::string_view::npos ? std::string_view{} : filename.substr(dot);
}

std::string_view stem_of(std::string_view path) noexcept {
    const auto filename = filename_of(path);
    const auto dot = filename.rfind('.');
    return dot == std::string_view::npos ? filename : filename.substr(0, dot);
}

#if defined(_UWP)
std::size_t sdl_zip_read_at(void* opaque, mz_uint64 offset, void* output, std::size_t bytes) {
    auto* stream = static_cast<SDL_IOStream*>(opaque);
    if (stream == nullptr ||
        offset > static_cast<mz_uint64>(std::numeric_limits<Sint64>::max()))
    {
        return 0;
    }
    if (SDL_SeekIO(stream, static_cast<Sint64>(offset), SDL_IO_SEEK_SET) < 0) {
        return 0;
    }
    return SDL_ReadIO(stream, output, bytes);
}
#endif

class TextureZipArchive final {
public:
    explicit TextureZipArchive(const std::filesystem::path& path) : m_path(path) {
        const auto pathString = io::fs_path_to_string(path);
#if defined(_UWP)
        m_stream = SDL_IOFromFile(pathString.c_str(), "rb");
        if (m_stream == nullptr) {
            throw std::runtime_error(fmt::format("opening file failed: {}", SDL_GetError()));
        }
        const Sint64 streamSize = SDL_GetIOSize(m_stream);
        if (streamSize < 0) {
            const std::string error = SDL_GetError();
            SDL_CloseIO(m_stream);
            m_stream = nullptr;
            throw std::runtime_error(fmt::format("reading file size failed: {}", error));
        }
        m_zip.m_pRead = sdl_zip_read_at;
        m_zip.m_pIO_opaque = m_stream;
        if (!mz_zip_reader_init(&m_zip, static_cast<mz_uint64>(streamSize), 0)) {
            const auto error = mz_zip_get_last_error(&m_zip);
            SDL_CloseIO(m_stream);
            m_stream = nullptr;
            throw std::runtime_error(
                fmt::format("opening ZIP failed: {}", mz_zip_get_error_string(error)));
        }
#else
        if (!mz_zip_reader_init_file(&m_zip, pathString.c_str(), 0)) {
            const auto error = mz_zip_get_last_error(&m_zip);
            throw std::runtime_error(
                fmt::format("opening ZIP failed: {}", mz_zip_get_error_string(error)));
        }
#endif
        m_open = true;

        try {
            const auto count = mz_zip_reader_get_num_files(&m_zip);
            m_files.reserve(count);
            m_indices.reserve(count);
            for (mz_uint i = 0; i < count; ++i) {
                mz_zip_archive_file_stat stat{};
                if (!mz_zip_reader_file_stat(&m_zip, i, &stat) ||
                    mz_zip_reader_is_file_a_directory(&m_zip, i))
                {
                    continue;
                }

                std::string name{stat.m_filename};
                std::replace(name.begin(), name.end(), '\\', '/');
                while (!name.empty() && name.front() == '/') {
                    name.erase(name.begin());
                }
                while (name.starts_with("./")) {
                    name.erase(0, 2);
                }
                if (name.empty()) {
                    continue;
                }

                const auto [unused, inserted] = m_indices.emplace(name, i);
                if (inserted) {
                    m_files.push_back(std::move(name));
                }
            }
        } catch (...) {
            close();
            throw;
        }
    }

    TextureZipArchive(const TextureZipArchive&) = delete;
    TextureZipArchive& operator=(const TextureZipArchive&) = delete;

    ~TextureZipArchive() { close(); }

    const std::filesystem::path& path() const noexcept { return m_path; }
    const std::vector<std::string>& files() const noexcept { return m_files; }

    bool read(const char* path, std::vector<uint8_t>& outBytes) noexcept {
        if (path == nullptr) {
            return false;
        }
        try {
            std::lock_guard lock{m_mutex};
            const auto entry = m_indices.find(path);
            if (entry == m_indices.end()) {
                return false;
            }

            std::size_t size = 0;
            void* data = mz_zip_reader_extract_to_heap(&m_zip, entry->second, &size, 0);
            if (data == nullptr) {
                return false;
            }
            try {
                const auto* begin = static_cast<const uint8_t*>(data);
                outBytes.assign(begin, begin + size);
            } catch (...) {
                mz_free(data);
                throw;
            }
            mz_free(data);
            return true;
        } catch (...) {
            return false;
        }
    }

private:
    void close() noexcept {
        if (m_open) {
            mz_zip_reader_end(&m_zip);
            m_open = false;
        }
#if defined(_UWP)
        if (m_stream != nullptr) {
            SDL_CloseIO(m_stream);
            m_stream = nullptr;
        }
#endif
    }

    std::filesystem::path m_path;
    mz_zip_archive m_zip{};
    bool m_open = false;
#if defined(_UWP)
    SDL_IOStream* m_stream = nullptr;
#endif
    std::mutex m_mutex;
    std::vector<std::string> m_files;
    std::unordered_map<std::string, mz_uint> m_indices;
};

std::vector<std::shared_ptr<TextureZipArchive>> s_zipArchives;

bool read_zip_member(void* userData, const char* path, std::vector<uint8_t>& outBytes) {
    if (userData == nullptr) {
        return false;
    }
    return static_cast<TextureZipArchive*>(userData)->read(path, outBytes);
}

struct ZipCandidate {
    std::shared_ptr<TextureZipArchive> archive;
    std::size_t archiveOrder = 0;
    std::string path;
    std::vector<std::string> components;
};

bool compare_zip_candidates(const ZipCandidate& lhs, const ZipCandidate& rhs) noexcept {
    if (lhs.archiveOrder != rhs.archiveOrder) {
        return lhs.archiveOrder < rhs.archiveOrder;
    }
    const auto count = std::min(lhs.components.size(), rhs.components.size());
    for (std::size_t i = 0; i < count; ++i) {
        const auto cmp = compare_ascii_ci(lhs.components[i], rhs.components[i]);
        if (cmp != 0) {
            return cmp < 0;
        }
        if (lhs.components[i] != rhs.components[i]) {
            return lhs.components[i] < rhs.components[i];
        }
    }
    if (lhs.components.size() != rhs.components.size()) {
        return lhs.components.size() < rhs.components.size();
    }
    return io::fs_path_to_string(lhs.archive->path()) < io::fs_path_to_string(rhs.archive->path());
}

void unload_zip_replacements() {
    aurora::texture::unregister_replacements(s_zipGroup);
    s_zipGroup.registrations.clear();
    s_zipArchives.clear();
}

void load_zip_replacements(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> zipPaths;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(
             root, std::filesystem::directory_options::skip_permission_denied, ec);
         !ec && it != std::filesystem::directory_iterator(); it.increment(ec))
    {
        if (!it->is_regular_file(ec) || ec) {
            ec.clear();
            continue;
        }
        if (iequals_ascii(io::fs_path_to_string(it->path().extension()), ".zip")) {
            zipPaths.push_back(it->path());
        }
    }
    std::sort(zipPaths.begin(), zipPaths.end(), [](const auto& lhs, const auto& rhs) {
        const auto lhsText = io::fs_path_to_string(lhs.filename());
        const auto rhsText = io::fs_path_to_string(rhs.filename());
        const auto cmp = compare_ascii_ci(lhsText, rhsText);
        return cmp != 0 ? cmp < 0 : lhsText < rhsText;
    });

    std::vector<ZipCandidate> candidates;
    for (std::size_t zipIndex = 0; zipIndex < zipPaths.size(); ++zipIndex) {
        const auto& zipPath = zipPaths[zipIndex];
        try {
            auto archive = std::make_shared<TextureZipArchive>(zipPath);
            for (const auto& member : archive->files()) {
                const auto extension = extension_of(member);
                if ((!iequals_ascii(extension, ".dds") && !iequals_ascii(extension, ".png")) ||
                    is_sidecar_mip(stem_of(member)))
                {
                    continue;
                }
                candidates.push_back({archive, zipIndex, member, virtual_path_components(member)});
            }
            s_zipArchives.push_back(std::move(archive));
        } catch (const std::exception& error) {
            DuskLog.warn("Texture ZIP '{}' was skipped: {}", io::fs_path_to_string(zipPath), error.what());
        }
    }

    std::sort(candidates.begin(), candidates.end(), compare_zip_candidates);
    std::vector<aurora::texture::TextureSourceKey> registeredKeys;
    registeredKeys.reserve(candidates.size());
    std::size_t currentArchive = std::numeric_limits<std::size_t>::max();
    for (const auto& candidate : candidates) {
        if (candidate.archiveOrder != currentArchive) {
            currentArchive = candidate.archiveOrder;
            registeredKeys.clear();
        }
        const auto parsed = aurora::texture::parse_replacement_filename(filename_of(candidate.path));
        if (!parsed.has_value() ||
            std::find(registeredKeys.begin(), registeredKeys.end(), *parsed) != registeredKeys.end())
        {
            continue;
        }
        registeredKeys.push_back(*parsed);
        auto registration = aurora::texture::register_virtual_replacement(
            candidate.path,
            {.read = read_zip_member, .userData = candidate.archive.get()},
            {.priority = kUserTextureReplacementPriority});
        if (registration.id != 0) {
            s_zipGroup.registrations.push_back(std::move(registration));
        }
    }

    DuskLog.info("Texture ZIP loading complete: {} archive(s), {} registration(s)",
                 s_zipArchives.size(), s_zipGroup.registrations.size());
}
}  // namespace

void reload() {
    unload_zip_replacements();
    aurora::texture::unregister_replacements(s_directoryGroup);
    s_directoryGroup.registrations.clear();

    if (!getSettings().game.enableTextureReplacements) {
        return;
    }

    const auto root = ConfigPath / "texture_replacements";
    s_directoryGroup = aurora::texture::load_replacement_directory(
        root, {.priority = kUserTextureReplacementPriority});
    DuskLog.info("Texture replacement directory loaded: {} registration(s)",
                 s_directoryGroup.registrations.size());

    // Keep the ZIP path additive and isolated: loose files still load exactly as in 1.4.1.627.
    load_zip_replacements(root);
}

void set_enabled(bool enabled) {
    getSettings().game.enableTextureReplacements.setValue(enabled);
    reload();
}

void shutdown() {
    unload_zip_replacements();
    aurora::texture::unregister_replacements(s_directoryGroup);
    s_directoryGroup.registrations.clear();
}

}  // namespace dusk::texture_replacements
'''
    path.write_text(text, encoding="utf-8")


def patch_aurora_input_uwp(aurora: Path) -> None:
    path = aurora / "lib/input.cpp"
    text = path.read_text(encoding="utf-8")
    marker = "#ifndef AURORA_WINDOWS_STORE\n  AURORA_ASSERT(SDL_Init"
    if marker not in text:
        old = (
            '  AURORA_ASSERT(SDL_Init(SDL_INIT_HAPTIC | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD | SDL_INIT_SENSOR),\n'
            '         "Failed to initialize SDL subsystems: {}", SDL_GetError());\n'
        )
        new = (
            '#ifndef AURORA_WINDOWS_STORE\n'
            '  AURORA_ASSERT(SDL_Init(SDL_INIT_HAPTIC | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD | SDL_INIT_SENSOR),\n'
            '         "Failed to initialize SDL subsystems: {}", SDL_GetError());\n'
            '#else\n'
            '  AURORA_ASSERT(SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD),\n'
            '         "Failed to initialize SDL subsystems: {}", SDL_GetError());\n'
            '#endif\n'
        )
        text = replace_once(text, old, new, "Aurora UWP SDL initialization")
    path.write_text(text, encoding="utf-8")


def patch_aurora_gpu_uwp(aurora: Path) -> None:
    path = aurora / "lib/webgpu/gpu.cpp"
    text = path.read_text(encoding="utf-8")

    if '#if !defined(AURORA_WINDOWS_STORE)\n#include "../dawn/TracyPlatform.hpp"' not in text:
        old = (
            '#ifdef WEBGPU_DAWN\n'
            '#include "../dawn/TracyPlatform.hpp"\n'
            '#include <dawn/native/DawnNative.h>\n'
            '#endif\n'
        )
        new = (
            '#ifdef WEBGPU_DAWN\n'
            '#if !defined(AURORA_WINDOWS_STORE)\n'
            '#include "../dawn/TracyPlatform.hpp"\n'
            '#include <dawn/native/DawnNative.h>\n'
            '#endif\n'
            '#endif\n'
        )
        text = replace_once(text, old, new, "Aurora Dawn UWP includes")

    if 'extern "C" __declspec(dllimport) void* uwp_GetWindowReference();' not in text:
        anchor = 'static std::atomic_bool g_vsyncEnabled = true;\n'
        addition = (
            anchor
            + '\n#if defined(AURORA_WINDOWS_STORE)\n'
            + 'extern "C" __declspec(dllimport) void* uwp_GetWindowReference();\n'
            + '#endif\n'
        )
        text = replace_once(text, anchor, addition, "Aurora UWP CoreWindow import")

    if 'wgpu::SurfaceDescriptorFromWindowsCoreWindow surfaceDesc{};' not in text:
        old = (
            '  window::SurfaceLock surfaceLock;\n'
            '  release_surface_locked();\n'
            '  g_surface = create_window_surface(g_instance, window, "Surface");\n'
        )
        new = (
            '  window::SurfaceLock surfaceLock;\n'
            '  release_surface_locked();\n'
            '#if defined(AURORA_WINDOWS_STORE)\n'
            '  wgpu::SurfaceDescriptorFromWindowsCoreWindow surfaceDesc{};\n'
            '  surfaceDesc.sType = wgpu::SType::SurfaceDescriptorFromWindowsCoreWindow;\n'
            '  surfaceDesc.coreWindow = uwp_GetWindowReference();\n'
            '  const wgpu::SurfaceDescriptor surfaceDescriptor{\n'
            '      .nextInChain = reinterpret_cast<const wgpu::ChainedStruct*>(&surfaceDesc),\n'
            '      .label = "Surface",\n'
            '  };\n'
            '  g_surface = g_instance.CreateSurface(&surfaceDescriptor);\n'
            '#else\n'
            '  g_surface = create_window_surface(g_instance, window, "Surface");\n'
            '#endif\n'
        )
        text = replace_once(text, old, new, "Aurora UWP CoreWindow surface")

    if '#ifdef WEBGPU_DAWN\n#if !defined(AURORA_WINDOWS_STORE)\n    dawn::native::DawnInstanceDescriptor' not in text:
        old = (
            '#ifdef WEBGPU_DAWN\n'
            '    dawn::native::DawnInstanceDescriptor dawnInstanceDescriptor;\n'
            '    dawnInstanceDescriptor.backendValidationLevel = dawn::native::BackendValidationLevel::Disabled;\n'
            '    dawnInstanceDescriptor.SetLoggingCallback(wgpu_log);\n'
            '#ifdef TRACY_ENABLE\n'
            '    dawnInstanceDescriptor.platform = tracy_dawn_platform();\n'
            '#endif\n'
            '    instanceDescriptor.nextInChain = &dawnInstanceDescriptor;\n'
            '#endif\n'
        )
        new = (
            '#ifdef WEBGPU_DAWN\n'
            '#if !defined(AURORA_WINDOWS_STORE)\n'
            '    dawn::native::DawnInstanceDescriptor dawnInstanceDescriptor;\n'
            '    dawnInstanceDescriptor.backendValidationLevel = dawn::native::BackendValidationLevel::Disabled;\n'
            '    dawnInstanceDescriptor.SetLoggingCallback(wgpu_log);\n'
            '#ifdef TRACY_ENABLE\n'
            '    dawnInstanceDescriptor.platform = tracy_dawn_platform();\n'
            '#endif\n'
            '    instanceDescriptor.nextInChain = &dawnInstanceDescriptor;\n'
            '#endif\n'
            '#endif\n'
        )
        text = replace_once(text, old, new, "Aurora Dawn UWP instance")

    path.write_text(text, encoding="utf-8")


def resolve_aurora_uwp_cherry_pick(aurora: Path) -> None:
    unmerged = [
        line.strip()
        for line in capture(["git", "diff", "--name-only", "--diff-filter=U"], aurora).splitlines()
        if line.strip()
    ]
    expected = {"lib/input.cpp", "lib/webgpu/gpu.cpp"}
    if set(unmerged) != expected:
        run(["git", "status", "--short"], aurora, check=False)
        run(["git", "cherry-pick", "--abort"], aurora, check=False)
        raise RuntimeError(
            "Aurora UWP cherry-pick has unexpected conflicts: "
            + (", ".join(unmerged) if unmerged else "cherry-pick failed without an unmerged path")
        )

    # The pinned UWP commit predates significant input and WebGPU refactors. Keep the
    # newer randomizer Aurora versions of those two files and port only the UWP
    # semantics from SternXD's patch onto their current structure. Every other file
    # in the UWP commit has already applied cleanly at this point.
    run(["git", "checkout", "--ours", "--", "lib/input.cpp", "lib/webgpu/gpu.cpp"], aurora)
    patch_aurora_input_uwp(aurora)
    patch_aurora_gpu_uwp(aurora)
    run(["git", "add", "lib/input.cpp", "lib/webgpu/gpu.cpp"], aurora)

    remaining = capture(["git", "diff", "--name-only", "--diff-filter=U"], aurora)
    if remaining:
        run(["git", "status", "--short"], aurora, check=False)
        run(["git", "cherry-pick", "--abort"], aurora, check=False)
        raise RuntimeError(f"Aurora UWP cherry-pick still has unresolved paths: {remaining}")

    run([
        "git", "-c", "user.name=TPR UWP Port Bot", "-c", "user.email=tpr-uwp@example.invalid",
        "-c", "core.editor=true",
        "cherry-pick", "--continue",
    ], aurora)

def verify_randomizer_storage(source: Path) -> None:
    # Package identity isolates UWP LocalState. The current UI must still route randomizer data
    # through Dusklight's configured data path rather than an immutable install path.
    candidates = [
        source / "src/dusk/ui/rando_config.cpp",
        source / "src/dusk/ui/rando_config.hpp",
        source / "src/dusk/ui/rando_seed_generation.cpp",
    ]
    joined = "\n".join(p.read_text(encoding="utf-8", errors="ignore") for p in candidates if p.exists())
    if "GetRandomizer" not in joined:
        raise RuntimeError("Randomizer UI storage helpers were not found; refusing unverified UWP port")


def verify_port(source: Path) -> None:
    required = [
        source / "platforms/uwp/CMakeLists.txt",
        source / "platforms/uwp/Package.appxmanifest",
        source / "src/dusk/randomizer/generator/randomizer.cpp",
        source / "src/dusk/ui/rando_config.cpp",
        source / "src/dusk/ui/file_browser.cpp",
    ]
    missing = [str(p.relative_to(source)) for p in required if not p.exists()]
    if missing:
        raise RuntimeError("Missing required port components: " + ", ".join(missing))

    cmake = (source / "CMakeLists.txt").read_text(encoding="utf-8")
    for marker in (
        "DUSK_UWP",
        "add_library(dusklight STATIC",
        "_UWP",
        "src/dusk/ui/file_browser.cpp",
        "src/dusk/ui/file_browser.hpp",
    ):
        if marker not in cmake:
            raise RuntimeError(f"Top-level CMake missing required UWP marker: {marker}")

    wrapper = (source / "platforms/uwp/CMakeLists.txt").read_text(encoding="utf-8")
    for marker in ("TPR_YAML_CPP_LIB", "TPR_BASE64PP_LIB", "TPR_TRACY_LIB", TARGET_NAME, "Wide310x150Logo"):
        if marker not in wrapper:
            raise RuntimeError(f"UWP wrapper missing marker: {marker}")

    manifest = (source / "platforms/uwp/Package.appxmanifest").read_text(encoding="utf-8-sig")
    for marker in (PACKAGE_NAME, DISPLAY_NAME, PUBLISHER, PACKAGE_VERSION, PHONE_PRODUCT_ID):
        if marker not in manifest:
            raise RuntimeError(f"UWP manifest missing custom identity marker: {marker}")

    aurora_input = (source / "extern/aurora/lib/input.cpp").read_text(encoding="utf-8")
    for marker in ("AURORA_WINDOWS_STORE", "SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD"):
        if marker not in aurora_input:
            raise RuntimeError(f"Aurora input missing UWP marker: {marker}")

    aurora_gpu = (source / "extern/aurora/lib/webgpu/gpu.cpp").read_text(encoding="utf-8")
    for marker in ("uwp_GetWindowReference", "SurfaceDescriptorFromWindowsCoreWindow", "AURORA_WINDOWS_STORE"):
        if marker not in aurora_gpu:
            raise RuntimeError(f"Aurora WebGPU missing UWP marker: {marker}")

    io_cpp = (source / "src/dusk/io.cpp").read_text(encoding="utf-8")
    for marker in (
        "std::optional<std::filesystem::path> uwp_local_folder_path()",
        "SDL_GetPrefPath(dusk::OrgName, dusk::AppName)",
        "#if defined(_UWP)",
    ):
        if marker not in io_cpp:
            raise RuntimeError(f"UWP LocalState implementation missing from io.cpp: {marker}")

    file_browser = (source / "src/dusk/ui/file_browser.cpp").read_text(encoding="utf-8")
    for marker in (
        "int file_pane_row_count(Pane* pane)",
        "int file_pane_child_index_containing(Pane* pane, Rml::Element* target)",
        "pane->children()",
    ):
        if marker not in file_browser:
            raise RuntimeError(f"Current-Pane FileBrowser forward-port missing: {marker}")
    for marker in ("->row_count()", "->child_index_containing(", "->focus_last()"):
        if marker in file_browser:
            raise RuntimeError(f"UWP FileBrowser still references removed Pane API: {marker}")

    heart_sources = "\n".join(
        (source / relative).read_text(encoding="utf-8")
        for relative in (
            "src/dusk/settings.h",
            "src/dusk/settings.cpp",
            "src/dusk/ui/cosmetics.cpp",
            "src/d/d_meter2_draw.cpp",
        )
    )
    for marker in (
        "ConfigVar<std::string> heartColor;",
        'cosmetics.heartColor", ""',
        "Register(g_userSettings.cosmetics.heartColor);",
        '"Heart Color", cosmetics.heartColor',
        "TPR UWP: live HUD heart-color cosmetic",
        "mpLifeTexture[i][1]",
        "tprHeartQuarterTags",
    ):
        if marker not in heart_sources:
            raise RuntimeError(f"Heart-color feature marker missing after assembly: {marker}")

    cosmetic_sources = "\n".join(
        (source / relative).read_text(encoding="utf-8")
        for relative in (
            "src/dusk/settings.h",
            "src/dusk/settings.cpp",
            "src/dusk/ui/cosmetics.hpp",
            "src/dusk/ui/cosmetics.cpp",
            "src/dusk/ui/ui.cpp",
            "src/dusk/randomizer/game/randomizer_context.cpp",
        )
    )
    for marker in (
        '"58a866", "Green"',
        "ConfigVar<std::string> menuAccentColor;",
        "ConfigVar<std::string> menuBackgroundColor;",
        "ConfigVar<std::string> menuTextColor;",
        "ConfigVar<std::string> menuBorderColor;",
        "randomizeAllColorsPerSeed",
        "perSeedRandomizedColors",
        'add_tab("Menu Colors"',
        'add_tab("Randomize Colors"',
        '"Randomize All Per Seed"',
        '"Randomize This Color Per Seed"',
        "stable_seed_color_hash",
        "apply_menu_color_theme();",
        "apply_per_seed_cosmetics(this->mHash);",
        '"tpr-user-menu-theme"',
    ):
        if marker not in cosmetic_sources:
            raise RuntimeError(f"Menu/per-seed cosmetic marker missing after assembly: {marker}")
    cosmetics_cpp = (source / "src/dusk/ui/cosmetics.cpp").read_text(encoding="utf-8")
    if cosmetics_cpp.count("SeedColorTarget{&values.") != 27:
        raise RuntimeError("Per-seed randomization does not cover all 27 color settings")

    cursor_sources = "\n".join(
        (source / relative).read_text(encoding="utf-8")
        for relative in (
            "src/dusk/settings.h",
            "src/dusk/settings.cpp",
            "src/dusk/ui/settings.cpp",
            "src/dusk/ui/input.cpp",
            "src/dusk/ui/overlay.cpp",
            "res/rml/overlay.rcss",
        )
    )
    for marker in (
        "CURSOR MODE ON",
        "controller_cursor_tracker_visible",
        "g_randomizerState.mInitialized",
        "!randomizer_GetContext().mCreatingSave",
        "AddMousePosEvent",
        "AddMouseButtonEvent",
        "Controller Cursor Speed",
        "Controller Cursor Deadzone",
        "Controller Cursor Acceleration",
        "controllerCursorSpeed",
        "controllerCursorDeadzone",
        "controllerCursorAcceleration",
    ):
        if marker not in cursor_sources:
            raise RuntimeError(f"Controller-cursor feature marker missing after assembly: {marker}")

    tracker_sources = "\n".join(
        (source / relative).read_text(encoding="utf-8")
        for relative in (
            "src/dusk/settings.h",
            "src/dusk/settings.cpp",
            "src/dusk/imgui/ImGuiMenuRandomizer.cpp",
            "src/dusk/ui/rando_config.cpp",
        )
    )
    for marker in (
        "Twilight Princess Item & Check Tracker",
        "Toggle Item & Check Tracker",
        "Tracker Window Settings",
        "Lock Position and Size",
        "randomizerTrackerOpacity",
        "randomizerTrackerScale",
        "randomizerTrackerLock",
        "mUpdateTracker = true",
    ):
        if marker not in tracker_sources:
            raise RuntimeError(f"Resizable tracker feature marker missing after assembly: {marker}")

    input_viewer_sources = "\n".join(
        (source / relative).read_text(encoding="utf-8")
        for relative in (
            "src/dusk/settings.h",
            "src/dusk/settings.cpp",
            "src/dusk/ui/settings.cpp",
            "src/dusk/imgui/ImGuiMenuTools.hpp",
            "src/dusk/imgui/ImGuiControllerOverlay.cpp",
        )
    )
    for marker in (
        "TPR UWP: primary layered input viewer",
        "inputViewerBackground",
        "inputViewerOutline",
        "inputViewerSticks",
        "inputViewerAnalogValues",
        "inputViewerLock",
        "inputViewerResetSizeRequested",
        "SetNextWindowSizeConstraints",
        "ImGuiWindowFlags_NoResize",
        "draw_input_viewer_fallback",
        "sample_input_viewer_state",
        "getAnalogL(PAD_1)",
        "getAnalogR(PAD_1)",
        "PADGetSensorData",
        "Reset Input Viewer Size",
        "free-positioned and resizable by default",
    ):
        if marker not in input_viewer_sources:
            raise RuntimeError(f"Primary input-viewer feature marker missing after assembly: {marker}")
    input_viewer_cpp = (source / "src/dusk/imgui/ImGuiControllerOverlay.cpp").read_text(
        encoding="utf-8"
    )
    if "ImGuiWindowFlags_AlwaysAutoResize" in input_viewer_cpp:
        raise RuntimeError("Primary input viewer still forces automatic sizing")
    if input_viewer_cpp.count("void ImGuiMenuTools::ShowInputViewer()") != 1:
        raise RuntimeError("Primary and legacy input viewers would compete at runtime")

    seed_io_sources = "\n".join(
        (source / relative).read_text(encoding="utf-8")
        for relative in (
            "src/dusk/randomizer/game/randomizer_context.cpp",
            "src/dusk/ui/rando_seed_generation.cpp",
            "src/dusk/imgui/ImGuiMenuRandomizer.cpp",
        )
    )
    for marker in (
        "TPR UWP: checked transactional seed.dat write",
        "dusk::io::FileStream::WriteAllText",
        "dusk::io::FileStream::ReadAllBytes",
        'temporaryPath += ".tmp"',
        "Seed data verification failed after temporary write",
        "Seed data verification failed after commit",
        "Seed data file is empty",
        "Seed data is invalid or incomplete",
        "Seed generation stopped safely",
    ):
        if marker not in seed_io_sources:
            raise RuntimeError(f"Safe seed data I/O marker missing after assembly: {marker}")

    settings_path = source / "src/dusk/ui/settings.cpp"
    settings = settings_path.read_text(encoding="utf-8")
    if "#ifndef _UWP\n        config_bool_select(leftPane, rightPane, getSettings().backend.checkForUpdates," not in settings:
        raise RuntimeError("Settings updater block is not guarded for UWP")
    assert_preprocessor_balanced(settings_path)

    verify_randomizer_storage(source)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=Path("work/TwilightPrincessRandomizer"))
    args = parser.parse_args()
    out = args.output.resolve()
    if out.exists():
        shutil.rmtree(out)
    out.parent.mkdir(parents=True, exist_ok=True)

    run(["git", "clone", "--no-checkout", RANDO_REPO, str(out)])
    run(["git", "checkout", "--detach", RANDO_SHA], out)
    run(["git", "submodule", "update", "--init", "--recursive"], out)
    if capture(["git", "rev-parse", "HEAD"], out) != RANDO_SHA:
        raise RuntimeError("Randomizer pin mismatch")

    run(["git", "remote", "add", "dusklight-uwp", UWP_REPO], out)
    run(["git", "fetch", "--no-tags", "dusklight-uwp", UWP_SHA], out)
    merge_cmd = [
        "git", "-c", "user.name=TPR UWP Port Bot", "-c", "user.email=tpr-uwp@example.invalid",
        "merge", "--no-ff", "--no-edit", "-X", "ours", UWP_SHA,
    ]
    merge_result = run(merge_cmd, out, check=False)
    if merge_result.returncode != 0:
        # Git cannot automatically resolve non-trivial submodule gitlink merges, even
        # with -X ours. At the pinned revisions the only expected unresolved path is
        # extern/aurora. Keep the randomizer's newer Aurora base here; the dedicated
        # SternXD UWP Aurora patch is applied immediately afterward inside the
        # submodule. Any other conflict remains a hard failure.
        unmerged = [
            line.strip()
            for line in capture(["git", "diff", "--name-only", "--diff-filter=U"], out).splitlines()
            if line.strip()
        ]
        if set(unmerged) != {"extern/aurora"}:
            run(["git", "status", "--short"], out, check=False)
            run(["git", "merge", "--abort"], out, check=False)
            raise RuntimeError(
                "UWP reference merge has unexpected conflicts: "
                + (", ".join(unmerged) if unmerged else "merge failed without an unmerged path")
            )

        ours_aurora = capture(["git", "rev-parse", ":2:extern/aurora"], out)
        if ours_aurora != EXPECTED_AURORA_BASE:
            run(["git", "status", "--short"], out, check=False)
            run(["git", "merge", "--abort"], out, check=False)
            raise RuntimeError(
                f"Unexpected randomizer Aurora side during UWP merge: {ours_aurora}; "
                f"expected {EXPECTED_AURORA_BASE}"
            )

        print(
            "Resolving expected extern/aurora gitlink conflict by keeping the "
            f"randomizer Aurora base {ours_aurora}",
            flush=True,
        )
        run(["git", "-C", "extern/aurora", "checkout", "--detach", ours_aurora], out)
        run(["git", "add", "extern/aurora"], out)

        remaining = capture(["git", "diff", "--name-only", "--diff-filter=U"], out)
        if remaining:
            run(["git", "status", "--short"], out, check=False)
            run(["git", "merge", "--abort"], out, check=False)
            raise RuntimeError(f"UWP reference merge still has unresolved paths: {remaining}")

        run([
            "git", "-c", "user.name=TPR UWP Port Bot", "-c", "user.email=tpr-uwp@example.invalid",
            "commit", "--no-edit",
        ], out)

    aurora = out / "extern/aurora"
    run(["git", "fetch", "origin"], aurora)
    run(["git", "checkout", "--detach", EXPECTED_AURORA_BASE], aurora)
    run(["git", "remote", "add", "stern-uwp", AURORA_UWP_REPO], aurora)
    run(["git", "fetch", "--no-tags", "stern-uwp", AURORA_UWP_SHA], aurora)
    try:
        run([
            "git", "-c", "user.name=TPR UWP Port Bot", "-c", "user.email=tpr-uwp@example.invalid",
            "cherry-pick", AURORA_UWP_SHA,
        ], aurora)
    except subprocess.CalledProcessError:
        run(["git", "status", "--short"], aurora, check=False)
        resolve_aurora_uwp_cherry_pick(aurora)
    run(["git", "add", "extern/aurora"], out)

    ensure_main_uwp_cmake(out)
    ensure_main_cpp_uwp(out)
    patch_io_uwp_local_folder(out)
    patch_settings_uwp(out)
    patch_heart_color_cosmetic(out)
    patch_menu_and_per_seed_cosmetics(out)
    patch_controller_cursor_mode(out)
    patch_controller_cursor_polish(out)
    patch_randomizer_tracker_window(out)
    patch_primary_input_viewer(out)
    patch_seed_data_io_safety(out)
    patch_file_browser_current_pane(out)
    patch_direct_texture_zip_loading(out)
    patch_uwp_wrapper(out)
    patch_manifest(out)
    install_branding(out)
    verify_port(out)

    run(["git", "add", "-A"], out)
    run([
        "git", "-c", "user.name=TPR UWP Port Bot", "-c", "user.email=tpr-uwp@example.invalid",
        "commit", "-m", "Create standalone Twilight Princess Randomizer UWP app",
    ], out)

    print("\nStandalone UWP source assembled successfully:", out)
    print("HEAD:", capture(["git", "rev-parse", "HEAD"], out))
    print("Aurora:", capture(["git", "rev-parse", "HEAD"], aurora))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
