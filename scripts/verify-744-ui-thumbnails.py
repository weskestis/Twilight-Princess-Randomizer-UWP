"""Verify all prepared previews in the real Windows-SDK-unpacked package."""
import argparse,hashlib,json
from pathlib import Path
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('package',type=Path);p.add_argument('source',type=Path);a=p.parse_args()
proof=json.loads((a.source/'platforms/uwp/optional_mods/UI-THUMBNAILS-744.json').read_text())
for image in proof['images']:
    file=a.package/'mods'/image['packageDirectory']/image['path']
    assert file.stat().st_size==image['size'] and hashlib.file_digest(file.open('rb'),'sha256').hexdigest()==image['sha256'],str(file)
    with Image.open(file) as actual:
        assert actual.size==(image['width'],image['height']) and max(actual.size)<=image['limit']
    original=a.package/'mods'/image['packageDirectory']/image['originalPath']
    assert hashlib.file_digest(original.open('rb'),'sha256').hexdigest()==image['originalSha256'],str(original)
print('PASS: '+str(len(proof['images']))+' packaged UI thumbnails and unchanged original images')
