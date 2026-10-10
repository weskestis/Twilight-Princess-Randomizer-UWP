"""Apply only the picker repair after the complete, green .736 source layers."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(os.environ['TPR_SRC'])
control = Path(os.environ.get('TPR_CONTROL', Path(__file__).resolve().parents[1]))
identity_files = ['src/dusk/startup_guard.hpp', 'src/dusk/ui/prelaunch.cpp',
                  'mods/randomizer/src/session.cpp', 'platforms/uwp/Package.appxmanifest']
identities = {name: (root/name).read_text(encoding='utf-8') for name in identity_files}
for name, text in identities.items():
    if '1.4.1.736' not in text:
        raise RuntimeError('.737 requires the complete .736 identity in '+name)
lock = json.loads((control/'APP_IDENTITY-737.json').read_text())['sourcePatches']
layers = [('xbox-file-picker-737-host.patch', root),
          ('xbox-file-picker-737-borealis.patch', root/'extern/borealis')]
originals = {}
with tempfile.TemporaryDirectory(prefix='tpr-737-patch-') as directory:
    prepared = []
    for name, base in layers:
        payload = (control/'patches'/name).read_bytes().replace(b'\r\n', b'\n')
        if hashlib.sha256(payload).hexdigest() != lock[name]:
            raise RuntimeError('.737 source patch hash mismatch: '+name)
        for line in payload.decode('utf-8').splitlines():
            if not line.startswith('--- ') or line == '--- /dev/null':
                continue
            filename = line[4:]
            if not filename.startswith('a/') or '..' in Path(filename).parts:
                raise RuntimeError('.737 unsafe patch path')
            path = base/filename[2:]
            before = path.read_bytes()
            normalized = before.replace(b'\r\n', b'\n')
            if before != normalized:
                originals[path] = before
                path.write_bytes(normalized)
        path = Path(directory)/name
        path.write_bytes(payload)
        prepared.append((base, path))
    try:
        for base, path in prepared:
            subprocess.run(['git','-C',str(base),'apply','--check',str(path)], check=True)
    except Exception:
        for path, before in originals.items():
            path.write_bytes(before)
        raise
    for base, path in prepared:
        subprocess.run(['git','-C',str(base),'apply',str(path)], check=True)

# Re-read the prelaunch file because the picker patch also changed that file.
for name in identity_files:
    text = (root/name).read_text(encoding='utf-8').replace('1.4.1.736','1.4.1.737')
    if name == 'src/dusk/startup_guard.hpp':
        old = 'detail::markerPath = userPath / "xbox-startup-stage-736.txt";'
        if text.count(old) != 1:
            raise RuntimeError('.737 current crash journal anchor changed')
        text = text.replace(old, 'detail::markerPath = userPath / "xbox-startup-stage-737.txt";')
        old = '{"xbox-startup-stage-735.txt", "xbox-startup-stage-734.txt", "xbox-startup-stage-733.txt", "xbox-startup-stage-732.txt"}'
        if text.count(old) != 1:
            raise RuntimeError('.737 previous crash journal anchor changed')
        text = text.replace(old, '{"xbox-startup-stage-736.txt", "xbox-startup-stage-735.txt", "xbox-startup-stage-734.txt", "xbox-startup-stage-733.txt", "xbox-startup-stage-732.txt"}')
    (root/name).write_text(text, encoding='utf-8', newline='\n')
print('Applied .737 controller picker, main-thread completion and visible folder/disc errors; preserved the complete .736 source and crash journals.')
