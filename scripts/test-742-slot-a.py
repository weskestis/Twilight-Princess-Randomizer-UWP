#!/usr/bin/env python3
"""Compile the actual GCI format/probe and CARD initialization/detection bodies."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def function(source, signature):
    start = source.index(signature)
    token = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/|[{}]')
    depth = 0
    for match in token.finditer(source, start):
        if match.group() == "{":
            depth += 1
        elif match.group() == "}":
            depth -= 1
            if depth == 0:
                return source[start:match.end()]
    raise RuntimeError("Unclosed production function: " + signature)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("--negative-control", action="store_true")
    args = parser.parse_args()
    root = args.source.resolve()
    control = Path(__file__).resolve().parent.parent
    names = ["extern/aurora/lib/card/CardGciFolder.cpp", "extern/aurora/lib/dolphin/card.cpp"]
    bodies = {name: (root / name).read_text(encoding="utf-8") for name in names}
    with tempfile.TemporaryDirectory(prefix="test-742-slot-a-") as temporary:
        work = Path(temporary)
        if args.negative_control:
            path = work / "original/lib/card/CardGciFolder.cpp"
            path.parent.mkdir(parents=True)
            path.write_text(bodies[names[0]], encoding="utf-8", newline="\n")
            patch = work / "reverse.patch"
            patch.write_bytes((control / "patches/xbox-slot-a-742-aurora.patch").read_bytes().replace(b"\r\n", b"\n"))
            subprocess.run(["git", "-C", str(work / "original"), "apply", "--reverse", str(patch)], check=True)
            bodies[names[0]] = path.read_text(encoding="utf-8")
        gci = bodies[names[0]]
        card = bodies[names[1]]
        generated = "namespace aurora::card {\n"
        generated += function(gci, "ECardResult CardGciFolder::format(") + "\n"
        generated += function(gci, "ProbeResults CardGciFolder::probeCardFile(") + "\n}\n"
        for signature in ["void CARDSetBasePath(", "void CARDInit(", "s32 CARDCheck(", "BOOL CARDProbe(", "s32 CARDProbeEx("]:
            generated += function(card, signature) + "\n"
        (work / "production-slot-a.inc").write_text(generated, encoding="utf-8", newline="\n")
        compiler = shutil.which("cl" if os.name == "nt" else "c++")
        if not compiler:
            raise RuntimeError("C++ compiler is unavailable")
        binary = work / ("slot-a.exe" if os.name == "nt" else "slot-a")
        fixture = control / "tests/slot_a_initialization.cpp"
        if os.name == "nt":
            command = [compiler, "/nologo", "/EHsc", "/std:c++20", "/utf-8", "/MD", "/DAURORA_WINDOWS_STORE=1",
                       "/I" + str(work), str(fixture), "/Fe:" + str(binary)]
        else:
            command = [compiler, "-std=c++20", "-O0", "-g", "-DAURORA_WINDOWS_STORE=1",
                       "-I" + str(work), str(fixture), "-o", str(binary)]
        subprocess.run(command, cwd=work, check=True)
        result = subprocess.run([str(binary), str(work / "data")] + (["negative"] if args.negative_control else []), cwd=work)
        if args.negative_control:
            if result.returncode != 42:
                raise RuntimeError("Shipped .741 card setup did not reproduce missing Slot A under a directory failure")
            print("PASS: shipped .741 directory provisioning failure produces real CARDProbe/CARDProbeEx NOCARD")
        elif result.returncode:
            raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
