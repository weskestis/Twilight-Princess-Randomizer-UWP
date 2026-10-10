"""Apply the signed optional mod layer after the complete .735 repair layer."""
import hashlib
import json
import os
import subprocess
import tempfile
from pathlib import Path

root = Path(os.environ['TPR_SRC'])
control = Path(os.environ.get('TPR_CONTROL', Path(__file__).resolve().parents[1]))
identity_files = ['src/dusk/startup_guard.hpp', 'src/dusk/ui/prelaunch.cpp',
                  'mods/randomizer/src/session.cpp', 'platforms/uwp/Package.appxmanifest']
identities = {name: (root/name).read_text(encoding='utf-8') for name in identity_files}
for name, text in identities.items():
    if '1.4.1.735' not in text:
        raise RuntimeError('.736 expected the complete .735 identity in '+name)
patch = control/'patches/optional-mod-ports-736-host.patch'
payload = patch.read_bytes().replace(b'\r\n', b'\n')
expected = json.loads((control/'APP_IDENTITY-736.json').read_text())['sourcePatches'][patch.name]
if hashlib.sha256(payload).hexdigest() != expected:
    raise RuntimeError('.736 signed port patch hash mismatch')
originals = {}
for line in payload.decode('utf-8').splitlines():
    if not line.startswith('--- '):
        continue
    name = line[4:]
    if name == '/dev/null':
        continue
    if not name.startswith('a/') or '..' in Path(name).parts:
        raise RuntimeError('.736 unsafe patch path')
    path = root/name[2:]
    before = path.read_bytes()
    normalized = before.replace(b'\r\n', b'\n')
    if before != normalized:
        originals[path] = before
        path.write_bytes(normalized)
with tempfile.TemporaryDirectory(prefix='tpr-736-patch-') as directory:
    prepared = Path(directory)/patch.name
    prepared.write_bytes(payload)
    try:
        subprocess.run(['git','-C',str(root),'apply','--check',str(prepared)],check=True)
    except Exception:
        for path, before in originals.items():
            path.write_bytes(before)
        raise
    subprocess.run(['git','-C',str(root),'apply',str(prepared)],check=True)
for name, text in identities.items():
    text = text.replace('1.4.1.735','1.4.1.736')
    if name == 'src/dusk/startup_guard.hpp':
        old = 'detail::markerPath = userPath / "xbox-startup-stage-735.txt";'
        if text.count(old) != 1:
            raise RuntimeError('.736 current crash journal anchor changed')
        text = text.replace(old,'detail::markerPath = userPath / "xbox-startup-stage-736.txt";')
        old = '{"xbox-startup-stage-734.txt", "xbox-startup-stage-733.txt", "xbox-startup-stage-732.txt"}'
        if text.count(old) != 1:
            raise RuntimeError('.736 previous crash journal anchor changed')
        text = text.replace(old,'{"xbox-startup-stage-735.txt", "xbox-startup-stage-734.txt", "xbox-startup-stage-733.txt", "xbox-startup-stage-732.txt"}')
    (root/name).write_text(text,encoding='utf-8',newline='\n')
print('Applied .736 signed controller/Luau ports and optional bundled resources; retained all .735 shop, writer and earlier fixes.')
