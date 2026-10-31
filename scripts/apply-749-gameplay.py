"""Resolve Soul text in the native pickup after its actor and item byte are ready."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(os.environ['TPR_SRC'])
control = Path(os.environ['TPR_CONTROL'])
identity = json.loads((control / 'APP_IDENTITY-749.json').read_text())
assert 'Version="1.4.1.748"' in (root / 'platforms/uwp/Package.appxmanifest').read_text()
name = 'xbox-soul-pickup-routing-749.patch'
data = (control / 'patches' / name).read_bytes().replace(b'\r\n', b'\n')
assert hashlib.sha256(data).hexdigest() == identity['sourcePatches'][name]
originals = {}
try:
    for line in data.decode().splitlines():
        if not line.startswith('+++ b/'): continue
        relative = Path(line[6:])
        assert not relative.is_absolute() and '..' not in relative.parts
        path = root / relative
        originals[path] = path.read_bytes()
        path.write_bytes(originals[path].replace(b'\r\n', b'\n'))
    with tempfile.TemporaryDirectory(prefix='gameplay-749-') as temp:
        patch = Path(temp) / name
        patch.write_bytes(data)
        subprocess.run(['git', '-C', str(root), 'apply', '--check', str(patch)], check=True)
        subprocess.run(['git', '-C', str(root), 'apply', str(patch)], check=True)
except Exception:
    for path, before in originals.items(): path.write_bytes(before)
    raise
print('Applied native Soul identity selection after item discovery; preserved existing native message and award cleanup')
