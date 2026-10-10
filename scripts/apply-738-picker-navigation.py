"""Layer the controller navigation repair onto the complete shipped .737 source."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(os.environ['TPR_SRC'])
control = Path(os.environ['TPR_CONTROL'])
identity_files = ['src/dusk/startup_guard.hpp', 'src/dusk/ui/prelaunch.cpp',
                  'mods/randomizer/src/session.cpp', 'platforms/uwp/Package.appxmanifest']
for name in identity_files:
    if '1.4.1.737' not in (root/name).read_text(encoding='utf-8'):
        raise RuntimeError('.738 requires the complete green .737 source: '+name)
patch_name = 'xbox-picker-navigation-738-host.patch'
patch = control/'patches'/patch_name
payload = patch.read_bytes().replace(b'\r\n', b'\n')
lock = json.loads((control/'APP_IDENTITY-738.json').read_text())['sourcePatches']
if hashlib.sha256(payload).hexdigest() != lock[patch_name]:
    raise RuntimeError('.738 picker patch hash mismatch')
originals = {}
for line in payload.decode('utf-8').splitlines():
    if not line.startswith('--- a/'):
        continue
    name = line[6:]
    if name.startswith('/') or '..' in Path(name).parts:
        raise RuntimeError('.738 unsafe source patch path')
    path = root/name
    source_payload = path.read_bytes()
    normalized = source_payload.replace(b'\r\n', b'\n')
    if source_payload != normalized:
        originals[path] = source_payload
        path.write_bytes(normalized)
with tempfile.TemporaryDirectory(prefix='tpr-738-patch-') as directory:
    normalized_patch = Path(directory)/patch_name
    normalized_patch.write_bytes(payload)
    try:
        subprocess.run(['git', '-C', str(root), 'apply', '--check', str(normalized_patch)], check=True)
    except Exception:
        for path, original in originals.items():
            path.write_bytes(original)
        raise
    subprocess.run(['git', '-C', str(root), 'apply', str(normalized_patch)], check=True)
for name in identity_files:
    text = (root/name).read_text(encoding='utf-8').replace('1.4.1.737', '1.4.1.738')
    if name == 'src/dusk/startup_guard.hpp':
        old = 'detail::markerPath = userPath / "xbox-startup-stage-737.txt";'
        if text.count(old) != 1:
            raise RuntimeError('.738 current crash journal anchor changed')
        text = text.replace(old, 'detail::markerPath = userPath / "xbox-startup-stage-738.txt";')
        old = '{"xbox-startup-stage-736.txt", "xbox-startup-stage-735.txt", "xbox-startup-stage-734.txt", "xbox-startup-stage-733.txt", "xbox-startup-stage-732.txt"}'
        if text.count(old) != 1:
            raise RuntimeError('.738 previous crash journal anchor changed')
        text = text.replace(old, '{"xbox-startup-stage-737.txt", "xbox-startup-stage-736.txt", "xbox-startup-stage-735.txt", "xbox-startup-stage-734.txt", "xbox-startup-stage-733.txt", "xbox-startup-stage-732.txt"}')
    (root/name).write_text(text, encoding='utf-8', newline='\n')
print('Applied .738 full-size Xbox browser, named Development Files/app-folder entry points, controller Back/paging and focus restoration; retained the complete .737 game and callback layers.')
