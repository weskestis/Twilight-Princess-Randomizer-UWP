"""Run the Crests gear and shared pickup regressions against the staged Xbox source."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="staged Dusklight source tree")
    root = parser.parse_args().source.resolve()
    module = root / "mods/uwp_ports/port_crests_of_courage"
    version = json.loads((module / "mod.json").read_text(encoding="utf-8-sig"))["version"]
    implementation = (module / "src/features.cpp").read_text(encoding="utf-8")
    start = implementation.index("bool is_field_item(")
    end = implementation.index("\n}", implementation.index("void apply_item_taken(", start)) + 2
    hello = implementation.index("void on_hello(")
    hello_end = implementation.index("\n}", hello) + 2
    fixture = Path(__file__).resolve().parents[1] / "tests/crests_peer_757.cpp"
    msvc = os.name == "nt"
    compiler = shutil.which("cl" if msvc else "c++")
    if compiler is None:
        raise SystemExit("C++ compiler unavailable")
    with tempfile.TemporaryDirectory(prefix="crests-757-") as directory:
        work = Path(directory)
        (work / "crests_pickup.inc").write_text(implementation[start:end], encoding="utf-8")
        (work / "crests_hello.inc").write_text(implementation[hello:hello_end], encoding="utf-8")
        (work / "version.hpp").write_text("#define COOP_MOD_VERSION " + json.dumps(version) + "\n")
        includes = [work, module / "src"]
        executable = work / ("peers.exe" if msvc else "peers")
        if msvc:
            command = [compiler, "/nologo", "/EHsc", "/std:c++20", "/utf-8",
                       *["/I" + str(p) for p in includes], str(fixture), "/Fe:" + str(executable)]
        else:
            command = [compiler, "-std=c++20", *["-I" + str(p) for p in includes],
                       str(fixture), "-o", str(executable)]
        subprocess.run(command, cwd=work, check=True)
        subprocess.run([str(executable)], cwd=work, check=True)


if __name__ == "__main__":
    main()
