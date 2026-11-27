"""Compile the staged camera and bottle routines and exercise their state changes."""
import argparse
import shutil
import subprocess
import tempfile
from pathlib import Path


def function(source, name):
    start = source.index("static void " + name + "() {")
    end = source.index("\n}", start)
    return source[start:end + 2]


def run(root, baseline):
    port = root / "mods/uwp_ports/port_twilit_essentials/src"
    camera = (port / "general/horse_cam.cpp").read_text(encoding="utf-8-sig")
    bottles = (port / "quick_access/quick_access_bottles.cpp").read_text(encoding="utf-8-sig")
    camera = camera[camera.index("bool g_configHorseCamNoRecenter"):]
    if baseline:
        camera += "\nvoid shutdown_horse_cam() {}\n"
    else:
        general = (port / "general/general.cpp").read_text(encoding="utf-8-sig")
        if "shutdown_horse_cam();" not in general[general.index("void shutdown_general()"):]:
            raise RuntimeError("Lost native camera shutdown binding")

    compiler = shutil.which("cl") or shutil.which("g++") or shutil.which("clang++")
    if not compiler:
        raise RuntimeError("C++ compiler missing")
    tests = Path(__file__).resolve().parents[1] / "tests/essentials_camera_bottles_757.cpp"
    with tempfile.TemporaryDirectory() as temp:
        work = Path(temp)
        (work / "staged_camera.inc").write_text(camera, encoding="utf-8")
        funcs = [function(bottles, name) for name in ("bottles_reset_assignment", "bottles_finish_pending")]
        (work / "staged_bottles.inc").write_text("\n".join(funcs), encoding="utf-8")
        exe = work / "essentials.exe"
        if Path(compiler).stem.lower() == "cl":
            command = [compiler, "/nologo", "/EHsc", "/std:c++17", "/utf-8", "/I" + str(work),
                       str(tests), "/Fe:" + str(exe)]
        else:
            command = [compiler, "-std=c++17", "-I", str(work), str(tests), "-o", str(exe)]
        subprocess.run(command, cwd=work, check=True)
        for scenario in ("camera", "bottle"):
            result = subprocess.run([str(exe), scenario], cwd=work)
            if (result.returncode != 0) != baseline:
                raise RuntimeError("Unexpected native regression result: " + scenario)
    print("PASS: " + ("original camera and bottle defects reproduced" if baseline else
                      "boar, horse, stick movement, style transitions, duplicate bottles and selection restoration"))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("--negative-control", action="store_true")
    options = parser.parse_args()
    run(options.source, options.negative_control)
