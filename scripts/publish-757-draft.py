"""Deliver the signed Xbox installers and verify their remote digests."""
import argparse
import hashlib
import json
import os
import subprocess
from pathlib import Path
from urllib.parse import quote


def api(endpoint, *args, payload=None):
    return json.loads(subprocess.check_output(["gh", "api", endpoint, *args], input=payload))


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def publish(out):
    repo = os.environ["GITHUB_REPOSITORY"]
    revision = os.environ["GITHUB_SHA"]
    tag = f"xbox-1.4.1.757-{os.environ['GITHUB_RUN_ID']}-{os.environ['GITHUB_RUN_ATTEMPT']}"
    release_api = f"repos/{repo}/releases"
    files = [out / name for name in (
        "TwilightPrincessRandomizer_1.4.1.757_x64.msix", "Microsoft.VCLibs.x64.14.00.appx",
        "SHA256SUMS.txt", "TwilightPrincessRandomizer.cer",
    )]
    hashes = {file.name: "sha256:" + sha256(file) for file in files}
    payload = {
        "name": "Xbox UWP 1.4.1.757 - Crests of Courage and Twilit Essentials",
        "draft": True, "target_commitish": revision, "tag_name": tag,
        "body": (out / "INSTALL_XBOX-757.md").read_text(encoding="utf-8"),
    }
    # A fresh draft has a temporary public-facing tag. Upload to the numeric ID.
    release = api(release_api, "--method", "POST", "--input", "-", payload=json.dumps(payload).encode())
    if not release["draft"] or release["target_commitish"] != revision or release["tag_name"] != tag:
        raise ValueError("Unexpected draft identity")
    endpoint = f"https://uploads.github.com/repos/{repo}/releases/{release['id']}/assets"
    for file in files:
        api(f"{endpoint}?name={quote(file.name)}", "--method", "POST", "--input", str(file),
            "-H", "Content-Type: application/octet-stream")
    confirmed = api(f"{release_api}/{release['id']}")
    assets = {asset["name"]: asset for asset in confirmed["assets"]}
    for file in files:
        asset = assets[file.name]
        if asset["size"] != file.stat().st_size or asset.get("digest") != hashes[file.name]:
            raise ValueError(f"Release asset mismatch: {file.name}")
        print(asset["browser_download_url"], hashes[file.name])


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("out", type=Path)
    publish(parser.parse_args().out)
