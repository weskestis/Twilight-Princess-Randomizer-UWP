#!/usr/bin/env python3
"""Stage bounded Mods previews and compact UI without advancing package identity."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

PATCH_SHA256 = "bb5db42f39ca7aa7c408b254ff7dc4125e8aba15aff4b699323943e895b7fb44"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", nargs="?", default=os.environ.get("TPR_SRC"), type=Path)
    args = parser.parse_args()
    if args.source is None:
        parser.error("source or TPR_SRC is required")
    root = args.source.resolve()
    control = Path(__file__).resolve().parent.parent
    payload = (control / "patches/xbox-mod-menu-742-host.patch").read_bytes().replace(b"\r\n", b"\n")
    if hashlib.sha256(payload).hexdigest() != PATCH_SHA256:
        raise RuntimeError("Pending Mods menu patch hash mismatch")
    originals = {}
    with tempfile.TemporaryDirectory(prefix="tpr-742-mod-menu-") as temporary:
        patch = Path(temporary) / "mods.patch"
        patch.write_bytes(payload)
        try:
            for line in payload.decode("utf-8").splitlines():
                if not line.startswith("+++ b/"):
                    continue
                name = line[6:]
                if name.startswith("/") or ".." in Path(name).parts:
                    raise RuntimeError("Unsafe Mods menu patch path")
                path = root / name
                before = path.read_bytes()
                originals[path] = before
                prepared = before.replace(b"\r\n", b"\n")
                if name == "src/dusk/ui/prelaunch.cpp" and b'#include "dusk/startup_guard.hpp"' not in prepared:
                    anchor = b'#include "dusk/settings.h"'
                    if prepared.count(anchor) != 1:
                        raise RuntimeError("Launcher include anchor changed")
                    prepared = prepared.replace(anchor, anchor + b'\n#include "dusk/startup_guard.hpp"')
                path.write_bytes(prepared)
            subprocess.run(["git", "-C", str(root), "apply", "--check", str(patch)], check=True)
            subprocess.run(["git", "-C", str(root), "apply", str(patch)], check=True)
        except Exception:
            for path, before in originals.items():
                path.write_bytes(before)
            raise
    print("Staged Mods image fault containment, bounded previews, both entry-point journals and compact installed list; package identity unchanged.")


if __name__ == "__main__":
    main()
