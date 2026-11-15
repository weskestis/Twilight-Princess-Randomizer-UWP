"""Verify pinned update inputs and advance the existing standard app identity."""
import hashlib
import json
import os
from pathlib import Path

root = Path(os.environ['TPR_SRC'])
control = Path(os.environ['TPR_CONTROL'])
identity = json.loads((control / 'APP_IDENTITY-755.json').read_text())
for name, expected in identity['sourceControls'].items():
    assert hashlib.sha256((control / name).read_bytes().replace(b'\r\n', b'\n')).hexdigest() == expected, name
for name in ['src/dusk/startup_guard.hpp', 'src/dusk/ui/prelaunch.cpp', 'mods/randomizer/src/session.cpp',
             'platforms/uwp/Package.appxmanifest', 'src/dusk/xbox_memory_budget.hpp']:
    file = root / name
    content = file.read_text()
    if name == 'src/dusk/startup_guard.hpp':
        anchor = 'detail::markerPath = userPath / "xbox-startup-stage-754.txt";'
        assert content.count(anchor) == 1
        content = content.replace(anchor, anchor.replace('754', '755'))
        content = content.replace('{"xbox-startup-stage-749.txt",', '{"xbox-startup-stage-754.txt", "xbox-startup-stage-749.txt",')
    else:
        assert '1.4.1.754' in content, name
    file.write_text(content.replace('1.4.1.754', '1.4.1.755'), encoding='utf-8', newline='\n')
print('PASS: .755 inputs verified; standard app identity, save locations and custom Randomizer retained')
