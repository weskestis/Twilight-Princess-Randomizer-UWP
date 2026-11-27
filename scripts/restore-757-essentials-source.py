"""Restore the previously archived 2.5.0 source on a clean build runner."""
import argparse
import hashlib
import json
import os
import subprocess
import tempfile
from pathlib import Path


def restore(source):
    control = Path(__file__).resolve().parents[1]
    mods = json.loads((control / "NATIVE_MODS-743.json").read_text(encoding="utf-8"))["mods"]
    archive = next(m["sourceArchive"] for m in mods if m["id"] == "com.dusklight.twilit_essentials")
    target = source / ".uwp-native-downloads-743" / archive["name"]
    target.parent.mkdir(parents=True, exist_ok=True)

    def matches(path):
        if not path.is_file() or path.stat().st_size != archive["size"]:
            return False
        with path.open("rb") as stream:
            return hashlib.file_digest(stream, "sha256").hexdigest() == archive["sha256"]

    if matches(target):
        print("Verified cached Twilit Essentials 2.5.0 source")
        return
    endpoint = "repos/" + os.environ["GITHUB_REPOSITORY"] + "/releases/assets/752322311"
    with tempfile.TemporaryDirectory(dir=target.parent) as directory:
        recovered = Path(directory) / "archive.zip"
        with recovered.open("wb") as stream:
            subprocess.run(["gh", "api", endpoint, "-H", "Accept: application/octet-stream"],
                           check=True, stdout=stream)
        if not matches(recovered):
            raise ValueError("Archived .755 source did not match the original 2.5.0 lock")
        recovered.replace(target)
    print("Recovered and verified Twilit Essentials 2.5.0 from the .755 release")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    restore(parser.parse_args().source)
