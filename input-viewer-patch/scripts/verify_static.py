#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import hashlib
import json
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
EXPECTED_ORIGINAL_SHA256 = "521496b108c8f26e811cae511af04b2a33c82b70e9b0cac8d33d8ec303f170ff"
EXPECTED_ASSETS = {
    "LockScreenLogo.scale-200.png": (48, 48),
    "Square44x44Logo.targetsize-24_altform-unplated.png": (24, 24),
    "Square44x44Logo.scale-200.png": (88, 88),
    "Square150x150Logo.scale-200.png": (300, 300),
    "StoreLogo.png": (50, 50),
    "SplashScreen.scale-200.png": (1240, 600),
    "Wide310x150Logo.scale-200.png": (620, 300),
    "Square310x310Logo.scale-200.png": (620, 620),
}
APPX_MAX_TILE_BYTES = {
    "Wide310x150Logo.scale-200.png": 204800,
    "Square310x310Logo.scale-200.png": 204800,
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def png_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()[:24]
    if len(data) != 24 or data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        raise SystemExit(f"Not a valid PNG: {path}")
    return struct.unpack(">II", data[16:24])


def assert_preprocessor_balanced(path: Path) -> None:
    stack: list[tuple[int, str]] = []
    for lineno, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        stripped = line.lstrip()
        if re.match(r"#\s*(if|ifdef|ifndef)\b", stripped):
            stack.append((lineno, stripped))
        elif re.match(r"#\s*endif\b", stripped):
            if not stack:
                raise SystemExit(f"Unexpected #endif in {path} at line {lineno}")
            stack.pop()
    if stack:
        lineno, directive = stack[-1]
        raise SystemExit(f"Unclosed preprocessor directive in {path} at line {lineno}: {directive}")


def verify_kit() -> None:
    identity = json.loads((ROOT / "APP_IDENTITY.json").read_text(encoding="utf-8"))
    expected = {
        "package_identity": "TwilightPrincessRandomizer",
        "display_name": "Twilight Princess Randomizer",
        "publisher": "CN=TwilightPrincessRandomizer",
        "version": "1.4.1.634",
        "randomizer_commit": "07a4d1ec6c7d80794f6ea865774ff9f45bd7da0e",
        "uwp_commit": "48d193dfebe4b20b4984af1d27d0e99f6b6722a7",
    }
    for key, value in expected.items():
        if identity.get(key) != value:
            raise SystemExit(f"APP_IDENTITY.json mismatch: {key}={identity.get(key)!r}, expected {value!r}")

    original = ROOT / "branding" / "Twilight-Princess-Randomizer-original.jpg"
    if sha256(original) != EXPECTED_ORIGINAL_SHA256:
        raise SystemExit("Original uploaded branding image hash changed")

    for name, dims in EXPECTED_ASSETS.items():
        p = ROOT / "branding" / "Assets" / name
        if not p.exists():
            raise SystemExit(f"Missing branding asset: {name}")
        if png_size(p) != dims:
            raise SystemExit(f"Wrong asset dimensions for {name}: {png_size(p)} != {dims}")
        max_bytes = APPX_MAX_TILE_BYTES.get(name)
        if max_bytes is not None and p.stat().st_size > max_bytes:
            raise SystemExit(
                f"AppX tile asset too large: {name} is {p.stat().st_size} bytes, max {max_bytes}"
            )

    assembler = (ROOT / "scripts" / "assemble_port.py").read_text(encoding="utf-8")
    for marker in (
        'PACKAGE_NAME = "TwilightPrincessRandomizer"',
        'DISPLAY_NAME = "Twilight Princess Randomizer"',
        'TARGET_NAME = "TwilightPrincessRandomizer"',
        'TPR_YAML_CPP_LIB',
        'TPR_BASE64PP_LIB',
        'TPR_TRACY_LIB',
        'TRACY_ENABLE:BOOL=ON',
        'TracyClient.lib is not required for UWP link',
        'UWP file browser source list',
        'Wide310x150Logo.scale-200.png',
        'Square310x310Logo.scale-200.png',
        'set(unmerged) != {"extern/aurora"}',
        'git", "rev-parse", ":2:extern/aurora"',
        'expected = {"lib/input.cpp", "lib/webgpu/gpu.cpp"}',
        '"-c", "core.editor=true"',
        'patch_aurora_input_uwp(aurora)',
        'patch_aurora_gpu_uwp(aurora)',
        'guarded_install = "if (NOT DUSK_UWP)\\n    " + install_call',
        'patch_io_uwp_local_folder(out)',
        'SDL_GetPrefPath(dusk::OrgName, dusk::AppName)',
        'patch_settings_uwp(out)',
        'patch_heart_color_cosmetic(out)',
        'patch_menu_and_per_seed_cosmetics(out)',
        '"Heart Color", cosmetics.heartColor',
        'TPR UWP: live HUD heart-color cosmetic',
        'assert_preprocessor_balanced(settings_path)',
        'patch_direct_texture_zip_loading(out)',
        'SDL_IOFromFile(pathString.c_str(), "rb")',
        'mz_zip_reader_extract_to_heap',
        'aurora::texture::register_virtual_replacement',
        'ConfigPath / "texture_replacements"',
        'patch_controller_cursor_mode(out)',
        'patch_controller_cursor_polish(out)',
        'patch_randomizer_tracker_window(out)',
        'patch_primary_input_viewer(out)',
        'patch_seed_data_io_safety(out)',
        'controller_cursor_tracker_visible',
        'g_randomizerState.mInitialized',
        '!randomizer_GetContext().mCreatingSave',
        'AddMousePosEvent',
        'AddMouseButtonEvent',
        '"Controller Cursor Speed"',
        '"Controller Cursor Deadzone"',
        '"Controller Cursor Acceleration"',
        '"Twilight Princess Item & Check Tracker"',
        '"Toggle Item & Check Tracker"',
        'randomizerTrackerOpacity',
        'randomizerTrackerScale',
        'randomizerTrackerLock',
        'inputViewerBackground',
        'inputViewerOutline',
        'inputViewerSticks',
        'inputViewerAnalogValues',
        'inputViewerLock',
        'inputViewerResetSizeRequested',
        'TPR UWP: checked transactional seed.dat write',
        'dusk::io::FileStream::WriteAllText',
        'dusk::io::FileStream::ReadAllBytes',
        'temporaryPath += ".tmp"',
        'Seed data verification failed after temporary write',
        'Seed data verification failed after commit',
        'Seed data file is empty',
        'Seed data is invalid or incomplete',
        'Seed generation stopped safely',
        '"58a866", "Green"',
        'ConfigVar<std::string> menuAccentColor;',
        'ConfigVar<std::string> menuBackgroundColor;',
        'ConfigVar<std::string> menuTextColor;',
        'ConfigVar<std::string> menuBorderColor;',
        'randomizeAllColorsPerSeed',
        'perSeedRandomizedColors',
        'add_tab("Menu Colors"',
        'add_tab("Randomize Colors"',
        '"Randomize All Per Seed"',
        '"Randomize This Color Per Seed"',
        'stable_seed_color_hash',
        'apply_menu_color_theme();',
        'apply_per_seed_cosmetics(this->mHash);',
        '"tpr-user-menu-theme"',
    ):
        if marker not in assembler:
            raise SystemExit(f"Assembler missing marker: {marker}")

    input_viewer_template = (
        ROOT / "patches" / "ImGuiControllerOverlay.cpp"
    ).read_text(encoding="utf-8")
    for marker in (
        "TPR UWP: primary layered input viewer",
        "SetNextWindowSizeConstraints",
        "draw_primary_input_viewer",
        "draw_input_viewer_fallback",
        "sample_input_viewer_state",
        "getAnalogL(PAD_1)",
        "getAnalogR(PAD_1)",
        "PADGetSensorData",
        "Lock position and size",
        "Reset size",
    ):
        if marker not in input_viewer_template:
            raise SystemExit(f"Primary input-viewer template missing: {marker}")
    if "ImGuiWindowFlags_AlwaysAutoResize" in input_viewer_template:
        raise SystemExit("Primary input-viewer template still forces automatic sizing")
    if input_viewer_template.count("void ImGuiMenuTools::ShowInputViewer()") != 1:
        raise SystemExit("Primary input-viewer template defines competing viewer entry points")


def verify_assembled(root: Path) -> None:
    required = [
        "platforms/uwp/CMakeLists.txt",
        "platforms/uwp/Package.appxmanifest",
        "src/dusk/randomizer/generator/randomizer.cpp",
        "src/dusk/ui/rando_config.cpp",
        "src/dusk/ui/file_browser.cpp",
    ]
    missing = [p for p in required if not (root / p).exists()]
    if missing:
        raise SystemExit("Assembled source missing: " + ", ".join(missing))

    manifest = (root / "platforms/uwp/Package.appxmanifest").read_text(encoding="utf-8-sig")
    for marker in (
        'Name="TwilightPrincessRandomizer"',
        'Publisher="CN=TwilightPrincessRandomizer"',
        'Version="1.4.1.634"',
        'Twilight Princess Randomizer',
        'Wide310x150Logo',
    ):
        if marker not in manifest:
            raise SystemExit(f"Manifest missing: {marker}")

    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    for marker in (
        "DUSK_UWP",
        "add_library(dusklight STATIC",
        "_UWP",
        "src/dusk/ui/file_browser.cpp",
        "src/dusk/ui/file_browser.hpp",
    ):
        if marker not in cmake:
            raise SystemExit(f"Top-level CMake missing: {marker}")
    runtime_install_guard = (
        "if (NOT DUSK_UWP)\n"
        "    aurora_install_runtime_dlls(dusklight ${CMAKE_INSTALL_PREFIX})\n"
        "endif ()"
    )
    if runtime_install_guard not in cmake:
        raise SystemExit("Top-level CMake leaves Aurora runtime install unguarded for UWP")

    wrapper = (root / "platforms/uwp/CMakeLists.txt").read_text(encoding="utf-8")
    for marker in ("project(TwilightPrincessRandomizer", "TPR_YAML_CPP_LIB", "TPR_BASE64PP_LIB", "TPR_TRACY_LIB", "TRACY_ENABLE:BOOL=ON"):
        if marker not in wrapper:
            raise SystemExit(f"UWP wrapper missing: {marker}")

    aurora_input = (root / "extern/aurora/lib/input.cpp").read_text(encoding="utf-8")
    for marker in ("AURORA_WINDOWS_STORE", "SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD"):
        if marker not in aurora_input:
            raise SystemExit(f"Assembled Aurora input missing UWP marker: {marker}")

    aurora_gpu = (root / "extern/aurora/lib/webgpu/gpu.cpp").read_text(encoding="utf-8")
    for marker in ("uwp_GetWindowReference", "SurfaceDescriptorFromWindowsCoreWindow", "AURORA_WINDOWS_STORE"):
        if marker not in aurora_gpu:
            raise SystemExit(f"Assembled Aurora WebGPU missing UWP marker: {marker}")

    io_cpp = (root / "src/dusk/io.cpp").read_text(encoding="utf-8")
    for marker in (
        "std::optional<std::filesystem::path> uwp_local_folder_path()",
        "SDL_GetPrefPath(dusk::OrgName, dusk::AppName)",
    ):
        if marker not in io_cpp:
            raise SystemExit(f"Assembled UWP LocalState helper missing: {marker}")

    heart_sources = "\n".join(
        (root / relative).read_text(encoding="utf-8")
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
            raise SystemExit(f"Assembled heart-color feature missing: {marker}")

    cosmetic_sources = "\n".join(
        (root / relative).read_text(encoding="utf-8")
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
            raise SystemExit(f"Assembled menu/per-seed cosmetic feature missing: {marker}")
    cosmetics_cpp = (root / "src/dusk/ui/cosmetics.cpp").read_text(encoding="utf-8")
    if cosmetics_cpp.count("SeedColorTarget{&values.") != 27:
        raise SystemExit("Assembled per-seed randomization does not cover all 27 color settings")

    settings_path = root / "src/dusk/ui/settings.cpp"
    settings = settings_path.read_text(encoding="utf-8")
    guard = "#ifndef _UWP\n        config_bool_select(leftPane, rightPane, getSettings().backend.checkForUpdates,"
    if guard not in settings:
        raise SystemExit("Assembled settings updater block is not guarded for UWP")
    assert_preprocessor_balanced(settings_path)

    texture_source = (root / "src/dusk/texture_replacements.cpp").read_text(encoding="utf-8")
    for marker in (
        "class TextureZipArchive final",
        "SDL_IOFromFile(pathString.c_str(), \"rb\")",
        "mz_zip_reader_extract_to_heap",
        "aurora::texture::register_virtual_replacement",
        "load_replacement_directory",
        'ConfigPath / "texture_replacements"',
        "s_zipArchives.clear()",
    ):
        if marker not in texture_source:
            raise SystemExit(f"Assembled direct texture-ZIP loader missing: {marker}")
    if 'ConfigPath / "texture_packs"' in texture_source:
        raise SystemExit("Assembled source unexpectedly adds a second texture-pack folder")

    cursor_sources = "\n".join(
        (root / relative).read_text(encoding="utf-8")
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
            raise SystemExit(f"Assembled controller-cursor feature missing: {marker}")

    tracker_sources = "\n".join(
        (root / relative).read_text(encoding="utf-8")
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
            raise SystemExit(f"Assembled resizable tracker feature missing: {marker}")

    input_viewer_sources = "\n".join(
        (root / relative).read_text(encoding="utf-8")
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
        "draw_input_viewer_fallback",
        "sample_input_viewer_state",
        "getAnalogL(PAD_1)",
        "getAnalogR(PAD_1)",
        "PADGetSensorData",
        "Reset Input Viewer Size",
        "free-positioned and resizable by default",
    ):
        if marker not in input_viewer_sources:
            raise SystemExit(f"Assembled primary input-viewer feature missing: {marker}")
    input_viewer_cpp = (root / "src/dusk/imgui/ImGuiControllerOverlay.cpp").read_text(
        encoding="utf-8"
    )
    if "ImGuiWindowFlags_AlwaysAutoResize" in input_viewer_cpp:
        raise SystemExit("Assembled primary input viewer still forces automatic sizing")
    if input_viewer_cpp.count("void ImGuiMenuTools::ShowInputViewer()") != 1:
        raise SystemExit("Assembled source contains competing input viewers")

    seed_io_sources = "\n".join(
        (root / relative).read_text(encoding="utf-8")
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
            raise SystemExit(f"Assembled safe seed data I/O feature missing: {marker}")

    for name, dims in EXPECTED_ASSETS.items():
        p = root / "platforms/uwp/Assets" / name
        if not p.exists() or png_size(p) != dims:
            raise SystemExit(f"Assembled branding asset invalid: {name}")
        max_bytes = APPX_MAX_TILE_BYTES.get(name)
        if max_bytes is not None and p.stat().st_size > max_bytes:
            raise SystemExit(
                f"Assembled AppX tile asset too large: {name} is {p.stat().st_size} bytes, max {max_bytes}"
            )


if __name__ == "__main__":
    verify_kit()
    if len(sys.argv) > 1:
        candidate = Path(sys.argv[1]).resolve()
        # The checkpoint kit itself is not an assembled Dusklight source tree.
        # This guard makes the verifier tolerant of workflows that pass TPR_KIT
        # while still validating a real assembled source path when supplied.
        if candidate != ROOT.resolve():
            verify_assembled(candidate)
    print("Twilight Princess Randomizer UWP static checks passed.")
