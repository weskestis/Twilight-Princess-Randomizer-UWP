"""Build Crests 1.10.1 from the pinned author sources and install its Xbox resources."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile
import zipfile


CONTROL = Path(__file__).resolve().parents[1]
PORT = CONTROL / "ports/updates-757/crests"
LOCK = json.loads((PORT / "release.json").read_text(encoding="utf-8"))
BASE = next(mod for mod in json.loads((CONTROL / "NATIVE_MODS-744.json").read_text())["mods"]
            if mod["id"] == LOCK["id"])
shared = importlib.util.spec_from_file_location("mod_resources", CONTROL / "scripts/stage-755-mod-updates.py")
resources = importlib.util.module_from_spec(shared)
shared.loader.exec_module(resources)
TARGET = BASE["target"]


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8", newline="\n")


def get_mod(inventory):
    matches = [entry for entry in inventory["mods"] if entry["id"] == LOCK["id"]]
    if len(matches) != 1:
        raise ValueError("Expected one Crests of Courage inventory entry")
    return matches[0]


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def file_digest(path):
    return resources.digest(path.read_bytes())


def author_files():
    entries = {row["path"]: row for row in BASE["sourceFiles"]}
    entries.update({row["path"]: row for row in LOCK["sourceChanges"]})
    return [entries[name] for name in sorted(entries)]


def adaptation_files():
    entries = {row["path"]: dict(row) for row in BASE["adaptations"]}
    source = {row["path"]: row["sha256"] for row in author_files()}
    for name, sha in LOCK["patchedSha256"].items():
        entries[name].update({
            "beforeSha256": source.get(name), "sha256": sha,
            "patchPath": "ports/updates-757/crests/xbox.patch",
        })
        if name in LOCK["patchAuthorFiles"]:
            del entries[name]["controlPath"]
    return list(entries.values())


def inventory_digest(entries):
    canonical = sorted(entries, key=lambda row: row["path"])
    return resources.digest(json.dumps(canonical, sort_keys=True, separators=(",", ":")).encode())


def gate(version):
    return '{"' + LOCK["id"] + '", "' + version + '"}'


def prepare_sources(archive, directory):
    with zipfile.ZipFile(archive) as package:
        prefix = package.namelist()[0].split("/", 1)[0] + "/"
        files = author_files()
        expected_src = {row["path"] for row in files if row["path"].startswith("src/")}
        archived_src = {name[len(prefix):] for name in package.namelist()
                        if name.startswith(prefix + "src/") and not name.endswith("/")}
        if expected_src != archived_src:
            raise ValueError("Source lock does not cover complete Crests src directory")
        for record in files:
            name = record["path"]
            info = package.getinfo(prefix + name)
            data = package.read(info)
            if not resources.safe(name) or stat.S_ISLNK(info.external_attr >> 16):
                raise ValueError("Unsafe source path: " + name)
            if len(data) != record["size"] or resources.digest(data) != record["sha256"]:
                raise ValueError("Author source does not match Crests lock: " + name)
            target = directory / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)

    for record in BASE["adaptations"]:
        name = record["path"]
        if name in LOCK["patchAuthorFiles"]:
            continue
        path = directory / name
        original = resources.digest(path.read_bytes().replace(b"\r\n", b"\n")) if path.is_file() else None
        payload = (CONTROL / record["controlPath"]).read_bytes().replace(b"\r\n", b"\n")
        if original != record["beforeSha256"] or resources.digest(payload) != record["sha256"]:
            raise ValueError("Xbox base adaptation changed: " + name)
        path.write_bytes(payload)

    patch = (PORT / "xbox.patch").read_bytes().replace(b"\r\n", b"\n")
    if resources.digest(patch) != LOCK["patchSha256"]:
        raise ValueError("Crests 1.10.1 Xbox adaptation changed")
    env = dict(os.environ, GIT_CEILING_DIRECTORIES=str(directory.parent))
    subprocess.run(["git", "apply", "--no-index", "--whitespace=nowarn", "-"],
                   input=patch, check=True, cwd=directory, env=env)
    for name, sha in LOCK["patchedSha256"].items():
        if file_digest(directory / name) != sha:
            raise ValueError("Rebased Crests Xbox adaptation mismatch: " + name)


def stage(root, cache):
    directory = root / "mods/uwp_ports" / TARGET
    optional = root / "platforms/uwp/optional_mods"
    native_path = optional / "NATIVE-MODS-744.json"
    catalog_path = optional / "BUNDLED-MODS.json"
    native, catalog = read(native_path), read(catalog_path)
    old = LOCK["previousVersion"]
    manifest = read(directory / "mod.json")
    if manifest["id"] != LOCK["id"] or any(item["version"] != old for item in
                                            (manifest, get_mod(native), get_mod(catalog))):
        raise ValueError("Crests update requires the known 1.10.0 mod")
    policy = root / "src/dusk/mods/uwp_ports.hpp"
    prior = policy.read_text(encoding="utf-8")
    if prior.count(gate(old)) != 1:
        raise ValueError("Crests 1.10.0 native admission was missing")

    cache.mkdir(parents=True, exist_ok=True)
    archive = resources.verified(LOCK["sourceArchive"], cache, "crests-1.10.1.zip")
    bundle = resources.verified(LOCK["download"], cache, "crests-1.10.1.dusk")
    with tempfile.TemporaryDirectory(dir=directory.parent, prefix="crests-update-") as tmp:
        fresh = Path(tmp) / "port"
        fresh.mkdir()
        prepare_sources(archive, fresh)
        originals = resources.resources(bundle, fresh, LOCK)
        if len(originals) != LOCK["originalResourceCount"] or inventory_digest(originals) != LOCK["originalResourceInventorySha256"]:
            raise ValueError("Crests author resources differ from the 1.10.1 release")
        for document in (fresh / "LICENSE", fresh / "CREDITS.md"):
            licenses = fresh / "res/licenses"
            licenses.mkdir(exist_ok=True)
            shutil.copyfile(document, licenses / document.name)
        # Replace the entire old directory so removed models or thumbnails cannot survive.
        prior_dir = Path(tmp) / "previous"
        directory.rename(prior_dir)
        try:
            fresh.rename(directory)
        except OSError:
            prior_dir.rename(directory)
            raise

    get_mod(native).update({
        "version": LOCK["version"], "sourceCommit": LOCK["sourceCommit"],
        "sourceArchive": LOCK["sourceArchive"], "sourceFiles": author_files(),
        "adaptations": adaptation_files(), "download": LOCK["download"],
        "xboxPatchSha256": LOCK["patchSha256"], "originalResourceFiles": originals,
        "resourceFiles": resources.inventory(directory),
    })
    get_mod(catalog).update({"version": LOCK["version"], "download": LOCK["download"]})
    policy.write_text(prior.replace(gate(old), gate(LOCK["version"])), encoding="utf-8", newline="\n")
    write_json(native_path, native)
    write_json(catalog_path, catalog)
    subprocess.run([sys.executable, str(CONTROL / "scripts/stage-744-ui-thumbnails.py"), str(root)], check=True)


def verify(root, package):
    optional = root / "platforms/uwp/optional_mods"
    directory = root / "mods/uwp_ports" / TARGET
    native = get_mod(read(optional / "NATIVE-MODS-744.json"))
    catalog = get_mod(read(optional / "BUNDLED-MODS.json"))
    if any(entry["version"] != LOCK["version"] or entry["download"] != LOCK["download"]
           for entry in (native, catalog)):
        raise ValueError("Crests catalog or native inventory mismatch")
    if native["sourceCommit"] != LOCK["sourceCommit"] or native["sourceArchive"] != LOCK["sourceArchive"]:
        raise ValueError("Wrong Crests source provenance")
    if native["sourceFiles"] != author_files() or native["adaptations"] != adaptation_files():
        raise ValueError("Incorrect Crests source/adaptation inventory")
    if native["xboxPatchSha256"] != LOCK["patchSha256"]:
        raise ValueError("Incorrect Crests patch identity")
    actual_hashes = {entry["path"]: entry["sha256"] for entry in author_files()}
    actual_hashes.update({entry["path"]: entry["sha256"] for entry in adaptation_files()})
    del actual_hashes["mod.json"]  # The SDK adds ABI and service metadata to the release manifest.
    for name, sha in actual_hashes.items():
        if file_digest(directory / name) != sha:
            raise ValueError("Crests staged source mismatch: " + name)
    files = resources.inventory(directory)
    by_name = {row["path"]: row for row in files}
    if by_name != {row["path"]: row for row in native["resourceFiles"]}:
        raise ValueError("Crests staged resource inventory differs")
    originals = native["originalResourceFiles"]
    if (len(originals) != LOCK["originalResourceCount"] or inventory_digest(originals) != LOCK["originalResourceInventorySha256"]
            or any(by_name.get(row["path"]) != row for row in originals)):
        raise ValueError("Crests original release resources differ")
    manifest = read(directory / "mod.json")
    policy = (root / "src/dusk/mods/uwp_ports.hpp").read_text(encoding="utf-8")
    if (manifest["id"], manifest["version"]) != (LOCK["id"], LOCK["version"]):
        raise ValueError("Crests mod manifest mismatch")
    if policy.count(gate(LOCK["version"])) != 1 or gate(LOCK["previousVersion"]) in policy:
        raise ValueError("Crests native version gate mismatch")
    if package is not None:
        installed = package / "mods" / TARGET
        installed_set = {p.relative_to(installed).as_posix() for p in installed.rglob("*") if p.is_file()}
        if installed_set != set(by_name) or any(file_digest(installed / name) != row["sha256"]
                                                 for name, row in by_name.items()):
            raise ValueError("Installed Crests resources differ from Xbox build inputs")
        for name in ("NATIVE-MODS-744.json", "BUNDLED-MODS.json"):
            if (package / "mods" / name).read_bytes() != (optional / name).read_bytes():
                raise ValueError("Installed Crests inventory differs: " + name)
    print(f"PASS: Crests {LOCK['version']} author source, Xbox port and {len(originals)} release resources verified")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("--cache", type=Path)
    parser.add_argument("--verify", action="store_true")
    parser.add_argument("--package", type=Path, help="unpacked MSIX root to verify")
    args = parser.parse_args()
    source = args.source.resolve()
    if not args.verify and args.package is None:
        stage(source, (args.cache or source / ".uwp-mod-updates-757").resolve())
    verify(source, args.package)
