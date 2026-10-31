"""Restore the exact archived Essentials input when its upstream source is unavailable."""
import hashlib
import json
import os
from pathlib import Path
import subprocess

root = Path(os.environ["TPR_SRC"])
control = Path(os.environ["TPR_CONTROL"])
lock = json.loads((control / "NATIVE_MODS-743.json").read_text(encoding="utf-8"))
entry = next(mod["sourceArchive"] for mod in lock["mods"]
             if mod["target"] == "port_twilit_essentials")
assert entry["sha256"] == "9d70b091a2ec88c0959c5e0f241acb02279326fa97e4d7db49035e18c3d7ad23"
assert entry["size"] == 49722697
target = root / ".uwp-native-downloads-743" / entry["name"]

def verified(path):
    return (path.is_file() and path.stat().st_size == entry["size"] and
            hashlib.sha256(path.read_bytes()).hexdigest() == entry["sha256"])

if verified(target):
    print("PASS: pinned Essentials source already available")
else:
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary = target.with_suffix(target.suffix + ".restore-part")
    try:
        with temporary.open("wb") as output:
            subprocess.run(["gh", "api",
                "repos/weskestis/Twilight-Princess-Randomizer-UWP/releases/assets/639402629",
                "-H", "Accept: application/octet-stream"], stdout=output, check=True)
        if not verified(temporary):
            raise RuntimeError("Archived Essentials source failed the original size/hash pins")
        temporary.replace(target)
    finally:
        temporary.unlink(missing_ok=True)
    print("PASS: restored identical pinned Essentials source from the .748 release")
