"""Apply the forward .735 online-mod layer to exact green .734 sources."""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(os.environ['TPR_SRC'])
control = Path(os.environ.get('TPR_CONTROL',Path(__file__).resolve().parents[1]))
identity_files = ['src/dusk/startup_guard.hpp','src/dusk/ui/prelaunch.cpp',
                  'mods/randomizer/src/session.cpp','platforms/uwp/Package.appxmanifest']
identities = {name:(root/name).read_text(encoding='utf-8') for name in identity_files}
for name,source in identities.items():
    if '1.4.1.734' not in source:
        raise RuntimeError(f'.735 expected the .734 identity in {name}')
expected = json.loads((control/'APP_IDENTITY-735.json').read_text())['sourcePatches']
patches = [(root/'extern/borealis',control/'patches/online-mod-storage-735-borealis.patch'),
           (root,control/'patches/online-mod-compatibility-735-host.patch'),
           (root,control/'patches/shop-idle-735-host.patch'),
           (root/'mods/randomizer',control/'patches/shop-idle-735-randomizer.patch')]
originals = {}
with tempfile.TemporaryDirectory(prefix='tpr-735-patches-') as temporary:
    prepared = []
    for directory,patch in patches:
        payload = patch.read_bytes().replace(b'\r\n',b'\n')
        digest = hashlib.sha256(payload).hexdigest()
        if digest != expected[patch.name]:
            raise RuntimeError(f'.735 patch hash mismatch: {patch.name}')
        for line in payload.decode('utf-8').splitlines():
            if not line.startswith('--- '): continue
            name = line[4:]
            if name == '/dev/null': continue
            name = json.loads(name) if name.startswith('"') else name
            if not name.startswith('a/') or '..' in Path(name).parts:
                raise RuntimeError(f'.735 unexpected patch path: {name}')
            path = directory/name[2:]
            before = path.read_bytes()
            normalized = before.replace(b'\r\n',b'\n')
            if before != normalized:
                originals[path] = before
                path.write_bytes(normalized)
        normalized_patch = Path(temporary)/patch.name
        normalized_patch.write_bytes(payload)
        prepared.append((directory,normalized_patch))
    try:
        for directory,patch in prepared:
            subprocess.run(['git','-C',str(directory),'apply','--check',str(patch)],check=True)
    except Exception:
        for path,before in originals.items(): path.write_bytes(before)
        raise
    for directory,patch in prepared:
        subprocess.run(['git','-C',str(directory),'apply',str(patch)],check=True)
        print(f'Applied .735 {patch.name}: SHA256 {expected[patch.name]}')

for name,source in identities.items():
    source = source.replace('1.4.1.734','1.4.1.735')
    if name == 'src/dusk/startup_guard.hpp':
        old = 'detail::markerPath = userPath / "xbox-startup-stage-734.txt";'
        if source.count(old) != 1: raise RuntimeError('.735 startup journal anchor changed')
        source = source.replace(old,'detail::markerPath = userPath / "xbox-startup-stage-735.txt";')
        old = '{"xbox-startup-stage-733.txt", "xbox-startup-stage-732.txt"}'
        if source.count(old) != 1: raise RuntimeError('.735 prior crash journals anchor changed')
        source = source.replace(old,'{"xbox-startup-stage-734.txt", "xbox-startup-stage-733.txt", "xbox-startup-stage-732.txt"}')
    (root/name).write_text(source,encoding='utf-8',newline='\n')
print('Applied .735 UWP writer, native compatibility and idle shop lifetime repairs; preserved the green .734 feature layer.')
