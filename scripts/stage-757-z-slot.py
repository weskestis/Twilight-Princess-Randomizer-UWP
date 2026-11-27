"""Preserve the Essentials Z item when the game records a new wolf scent."""
import argparse
import hashlib
from pathlib import Path


def stage(root):
    file = root / "mods/uwp_ports/port_twilit_essentials/src/z_button/z_itemwheel.cpp"
    source = file.read_text(encoding="utf-8")
    if hashlib.sha256(source.encode()).hexdigest() != "e9cbe6173cc56887d1e02e8d11f79feeeaa3d94710220d2a5123e65191950b53":
        raise ValueError("Unexpected Essentials 2.5.0 Z-item source")
    original = """    if (i_no == 2) {
        status->mSelectItem[2] = i_slotNo;"""
    patched = """    if (i_no == 2) {
        // The original game mirrors a collected scent into unused button 2.
        // The scent is already saved by setSmellTypeLocal. It is not a wheel slot.
        if (i_slotNo >= dItemNo_SMELL_YELIA_POUCH_e && i_slotNo <= dItemNo_SMELL_MEDICINE_e) {
            return HOOK_SKIP_ORIGINAL;
        }
        status->mSelectItem[2] = i_slotNo;"""
    if source.count(original) != 1:
        raise ValueError("Unexpected Z-slot assignment hook")
    file.write_text(source.replace(original, patched), encoding="utf-8", newline="\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    stage(parser.parse_args().source)
