"""Backport the Quick Access Spinner to the pinned Xbox Essentials 2.5.0 source."""
import argparse
import hashlib
import json
from pathlib import Path


SOURCE_SHA256 = "1919330ff80eb5aa0237d784bbddde2a747b3ef21786406945720722d6f0ff95"
PATCHED_SHA256 = "b8367410f912165decd151809964a0cabae68d0dbdfdd622102a8c11928fa667"


def stage(root, verify):
    port = root / "mods/uwp_ports/port_twilit_essentials"
    manifest = json.loads((port / "mod.json").read_text(encoding="utf-8-sig"))
    if (manifest["id"], manifest["version"]) != ("com.dusklight.twilit_essentials", "2.5.0"):
        raise RuntimeError("Unexpected Twilit Essentials native port")

    path = port / "src/quick_access/quick_access_edit.cpp"
    data = path.read_text(encoding="utf-8")
    digest = hashlib.sha256(data.encode()).hexdigest()
    if digest == SOURCE_SHA256 and not verify:
        # Let the editor list the Spinner, including when the item wheel is filtered.
        data = data.replace("    dItemNo_SPINNER_e,\n", "")
        data = data.replace("        dItemNo_HORSE_FLUTE_e,\n",
                            "        dItemNo_HORSE_FLUTE_e,\n        dItemNo_SPINNER_e,\n")
        if hashlib.sha256(data.encode()).hexdigest() != PATCHED_SHA256:
            raise RuntimeError("Unexpected Quick Access edit result")
        path.write_text(data, encoding="utf-8", newline="\n")
    elif digest != PATCHED_SHA256:
        raise RuntimeError("Quick Access editor differs from the pinned source")
    print("PASS: Quick Access Spinner staged for the Essentials 2.5.0 Xbox port")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    stage(args.source, args.verify)
