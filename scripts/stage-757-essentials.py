"""Apply Xbox fixes to the audited 2.5.0 Twilit Essentials native port."""
import argparse
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

CONTROL = Path(__file__).resolve().parents[1]
RELEASE_DIR = CONTROL / "ports/updates-757/essentials"
PORT = Path("mods/uwp_ports/port_twilit_essentials")
RECEIPT = Path("platforms/uwp/optional_mods/ESSENTIALS-XBOX-757.json")


def normalized(path):
    return path.read_bytes().replace(b"\r\n", b"\n")


def sha(data):
    return hashlib.sha256(data).hexdigest()


def stage(root, verify):
    release = json.loads((RELEASE_DIR / "release.json").read_text(encoding="utf-8"))
    entries = release["normalizedSourceSha256"]
    directory = root / PORT
    identity = json.loads((directory / "mod.json").read_text(encoding="utf-8-sig"))
    port_inventory = json.loads((CONTROL / "NATIVE_MODS-743.json").read_text(encoding="utf-8"))
    upstream = next(mod for mod in port_inventory["mods"] if mod["id"] == release["id"])
    if (identity["id"] != release["id"] or identity["version"] != release["upstreamVersion"]
            or upstream["sourceCommit"] != release["upstreamCommit"]
            or upstream["sourceArchive"]["sha256"] != release["upstreamSourceSha256"]):
        raise ValueError("Twilit Essentials upstream identity changed")

    source = {name: normalized(directory / name) for name in entries}
    finished = all(sha(data) == entries[name]["after"] for name, data in source.items())
    if verify and not finished:
        raise ValueError("Twilit Essentials Xbox sources differ from verified outputs")

    if not finished:
        with tempfile.TemporaryDirectory(prefix="essentials-757-") as temp:
            working = Path(temp)
            for name, data in source.items():
                if sha(data) != entries[name]["before"]:
                    raise ValueError(f"Unexpected Twilit Essentials base source: {name}")
                dest = working / name
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_bytes(data)
            patch = working / "xbox.patch"
            patch.write_bytes(normalized(RELEASE_DIR / patch.name))
            subprocess.run(["git", "apply", "--whitespace=error", str(patch)], cwd=working, check=True)
            for name, hashes in entries.items():
                if sha(normalized(working / name)) != hashes["after"]:
                    raise ValueError(f"Unexpected Xbox patch output: {name}")
            for name in entries:
                (directory / name).write_bytes(normalized(working / name))

    receipt = root / RECEIPT
    if verify:
        if json.loads(receipt.read_text(encoding="utf-8")) != release:
            raise ValueError("Twilit Essentials Xbox receipt differs")
    else:
        receipt.parent.mkdir(parents=True, exist_ok=True)
        receipt.write_text(json.dumps(release, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("PASS: native Twilit Essentials 2.5.0 Xbox camera and inventory changes verified")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    stage(args.source.resolve(), args.verify)
