"""Keep a signed Xbox port selectable when LocalState has a desktop-only update."""
import argparse
from pathlib import Path


def patch(path, anchor, replacement):
    text = path.read_text(encoding="utf-8")
    if text.count(anchor) != 1:
        raise ValueError(f"Unexpected package discovery source in {path}: {anchor[:70]}")
    path.write_text(text.replace(anchor, replacement), encoding="utf-8", newline="\n")


def stage(root):
    loader = root / "src/dusk/mods/loader/loader.cpp"
    packages = root / "src/dusk/mods/loader/packages.cpp"
    patch(packages, '#include "packages.hpp"\n', '''#include "packages.hpp"

#if defined(_UWP)
#include "dusk/mods/uwp_ports.hpp"
#endif
''')
    anchor = "        const int order = selected ?"
    patch(packages, anchor, '''#if defined(_UWP)
        // A desktop update may share the mod ID without matching the code linked into this app.
        if (selected) {
            const bool candidateAvailable = uwp_ports::available(modId, package.metadata.version);
            const bool selectedAvailable = uwp_ports::available(modId, selected->metadata.version);
            if (candidateAvailable != selectedAvailable) {
                if (candidateAvailable) {
                    selected = &package;
                }
                continue;
            }
        }
#endif
''' + anchor)
    # Existing catalog updates are gated here too, but manually imported bundles can bypass the catalog.
    if '#include "dusk/mods/uwp_ports.hpp"' not in loader.read_text(encoding="utf-8"):
        raise ValueError("Xbox native mod port support must be staged before the discovery fix")
    anchor = "    if (!borealis::update::parse_version(metadata.version)) {"
    patch(loader, anchor, '''#if defined(_UWP)
    if ((uwp_ports::native_id_known(metadata.id) || metadata.id == uwp_ports::ControllerId ||
            metadata.id == uwp_ports::LuauId) &&
        !uwp_ports::available(metadata.id, metadata.version)) {
        return {.success = false, .message = "This mod version requires a matching Xbox app port."};
    }
#endif
''' + anchor)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    stage(parser.parse_args().source)
