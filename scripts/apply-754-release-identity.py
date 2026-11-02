"""Verify the reviewed stability controls and advance the existing app identity."""
import hashlib, json, os
from pathlib import Path
r=Path(os.environ['TPR_SRC']);c=Path(os.environ['TPR_CONTROL'])
identity=json.loads((c/'APP_IDENTITY-754.json').read_text())
for name,expected in identity['sourceControls'].items():
    assert hashlib.sha256((c/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==expected,name
for name,expected in identity['sourcePatches'].items():
    assert hashlib.sha256((c/'patches'/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==expected,name
for name in ['src/dusk/startup_guard.hpp','src/dusk/ui/prelaunch.cpp','mods/randomizer/src/session.cpp',
             'platforms/uwp/Package.appxmanifest','src/dusk/xbox_memory_budget.hpp']:
    p=r/name;s=p.read_text()
    if name!='src/dusk/startup_guard.hpp':assert '1.4.1.749' in s,name
    else:assert 'detail::markerPath = userPath / "xbox-startup-stage-749.txt";' in s,name
    s=s.replace('1.4.1.749','1.4.1.754')
    if name=='src/dusk/startup_guard.hpp':
        s=s.replace('detail::markerPath = userPath / "xbox-startup-stage-749.txt";',
                    'detail::markerPath = userPath / "xbox-startup-stage-754.txt";')
        s=s.replace('{"xbox-startup-stage-748.txt",','{"xbox-startup-stage-749.txt", "xbox-startup-stage-748.txt",')
    p.write_text(s,encoding='utf-8',newline='\n')
print('PASS: stability controls verified; standard Xbox identity advanced to 1.4.1.754')
