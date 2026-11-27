"""Exercise the actual engine scent award, Essentials assignment hook and Z-slot sync."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def body(path, signature):
    source = path.read_text(encoding="utf-8")
    start = source.index(signature)
    return source[start:source.index("\n}", start) + 2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    root = parser.parse_args().source
    z = root / "mods/uwp_ports/port_twilit_essentials/src/z_button"
    fixture = Path(__file__).resolve().parents[1] / "tests/z_slot_scent_757.cpp"
    windows = os.name == "nt"
    compiler = shutil.which("cl" if windows else "c++")
    if not compiler:
        raise RuntimeError("C++ compiler unavailable")
    with tempfile.TemporaryDirectory() as directory:
        work = Path(directory)
        for name, path, signature in [
            ("sync.inc", z / "z_common.cpp", "void sync_z_item_state()"),
            ("assign.inc", z / "z_itemwheel.cpp", "HookAction on_set_select_item_index_pre("),
            ("scent.inc", root / "src/d/d_msg_object.cpp", "void dMsgObject_c::setSmellTypeLocal("),
        ]:
            (work / name).write_text(body(path, signature), encoding="utf-8")
        exe = work / ("slot.exe" if windows else "slot")
        args = ["/nologo", "/EHsc", "/std:c++17", "/I" + str(work), str(fixture), "/Fe:" + str(exe)] if windows else [
            "-std=c++17", "-I", str(work), str(fixture), "-o", str(exe)]
        subprocess.run([compiler, *args], check=True, cwd=work)
        subprocess.run([str(exe)], check=True, cwd=work)
    print("PASS: six wolf scents, saved Z item and mix, reassignment, clear, X/Y and mod-off semantics")


if __name__ == "__main__":
    main()
