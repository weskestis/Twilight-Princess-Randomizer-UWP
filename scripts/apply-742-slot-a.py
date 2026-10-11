#!/usr/bin/env python3
"""Add a GCI directory fallback and report the game's real Slot A probe."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

PATCHES = {
    "host": ("", "b802144df97122991eea293e1e90323e77d2cd94bfb0a5236589c3ecda4503e9"),
    "aurora": ("extern/aurora", "b0364e82e6e58d6312576b970c28ff1640cadbb268d272e5736ac9ff8ba95580"),
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", nargs="?", default=os.environ.get("TPR_SRC"), type=Path)
    args = parser.parse_args()
    if args.source is None:
        parser.error("source or TPR_SRC is required")
    root = args.source.resolve()
    control = Path(__file__).resolve().parent.parent
    originals, prepared = {}, []
    with tempfile.TemporaryDirectory(prefix="tpr-742-slot-a-") as temporary:
        try:
            for kind, (sub, digest) in PATCHES.items():
                name = f"xbox-slot-a-742-{kind}.patch"
                payload = (control / "patches" / name).read_bytes().replace(b"\r\n", b"\n")
                if hashlib.sha256(payload).hexdigest() != digest:
                    raise RuntimeError("Slot A patch hash mismatch: " + name)
                destination = root / sub
                for line in payload.decode("utf-8").splitlines():
                    if not line.startswith("+++ b/"):
                        continue
                    name_in_patch = line[6:]
                    if name_in_patch.startswith("/") or ".." in Path(name_in_patch).parts:
                        raise RuntimeError("Unsafe Slot A patch path")
                    path = destination / name_in_patch
                    before = path.read_bytes()
                    originals[path] = before
                    path.write_bytes(before.replace(b"\r\n", b"\n"))
                patch = Path(temporary) / name
                patch.write_bytes(payload)
                prepared.append((destination, patch))
            for destination, patch in prepared:
                subprocess.run(["git", "-C", str(destination), "apply", "--check", str(patch)], check=True)
            for destination, patch in prepared:
                subprocess.run(["git", "-C", str(destination), "apply", str(patch)], check=True)
        except Exception:
            for path, before in originals.items():
                path.write_bytes(before)
            raise
    print("Staged GCI directory provisioning fallback and actual Slot A initialization diagnostics; existing saves and package identity retained.")


if __name__ == "__main__":
    main()
