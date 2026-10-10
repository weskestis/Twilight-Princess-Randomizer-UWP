"""Apply scoped, contained asset scans on the complete green .739 source."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(os.environ['TPR_SRC'])
control = Path(os.environ['TPR_CONTROL'])
identity_files = ['src/dusk/startup_guard.hpp', 'src/dusk/ui/prelaunch.cpp',
                  'mods/randomizer/src/session.cpp', 'platforms/uwp/Package.appxmanifest',
                  'src/dusk/xbox_memory_budget.hpp']
for name in identity_files:
    if '1.4.1.739' not in (root/name).read_text(encoding='utf-8'):
        raise RuntimeError('.740 requires the complete .739 source: ' + name)
patch_name = 'xbox-asset-scan-740-host.patch'
payload = (control/'patches'/patch_name).read_bytes().replace(b'\r\n', b'\n')
expected = json.loads((control/'APP_IDENTITY-740.json').read_text())['sourcePatches'][patch_name]
if hashlib.sha256(payload).hexdigest() != expected:
    raise RuntimeError('.740 asset scan patch hash mismatch')
originals = {}
for line in payload.decode('utf-8').splitlines():
    if not line.startswith('--- a/'):
        continue
    name = line[6:]
    if name.startswith('/') or '..' in Path(name).parts:
        raise RuntimeError('.740 unsafe patch path')
    path = root/name
    before = path.read_bytes()
    if before != before.replace(b'\r\n', b'\n'):
        originals[path] = before
        path.write_bytes(before.replace(b'\r\n', b'\n'))
with tempfile.TemporaryDirectory(prefix='tpr-740-patch-') as directory:
    patch = Path(directory)/patch_name
    patch.write_bytes(payload)
    try:
        subprocess.run(['git', '-C', str(root), 'apply', '--check', str(patch)], check=True)
    except Exception:
        for path, before in originals.items():
            path.write_bytes(before)
        raise
    subprocess.run(['git', '-C', str(root), 'apply', str(patch)], check=True)

for name in identity_files:
    text = (root/name).read_text(encoding='utf-8').replace('1.4.1.739', '1.4.1.740')
    if name == 'src/dusk/startup_guard.hpp':
        current = 'detail::markerPath = userPath / "xbox-startup-stage-739.txt";'
        previous = '{"xbox-startup-stage-738.txt", "xbox-startup-stage-737.txt", "xbox-startup-stage-736.txt", "xbox-startup-stage-735.txt", "xbox-startup-stage-734.txt", "xbox-startup-stage-733.txt", "xbox-startup-stage-732.txt"}'
        if text.count(current) != 1 or text.count(previous) != 1:
            raise RuntimeError('.740 startup journal anchors changed')
        text = text.replace(current, 'detail::markerPath = userPath / "xbox-startup-stage-740.txt";')
        text = text.replace(previous, '{"xbox-startup-stage-739.txt", ' + previous[1:])
    (root/name).write_text(text, encoding='utf-8', newline='\n')
print('Applied .740 scoped lexical asset scans, contained listing failures, staged overlay publication and dirty-flag repair; retained the complete .739 game.')
