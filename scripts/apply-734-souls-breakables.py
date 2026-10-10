"""Apply the forward .734 layer after the .733 source and lifecycle guards."""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(os.environ['TPR_SRC'])
control = Path(os.environ.get('TPR_CONTROL', Path(__file__).resolve().parents[1]))
patches = (
    (root, control / 'patches/souls-all-checks-734-host.patch'),
    (root / 'mods/randomizer', control / 'patches/souls-all-checks-734-randomizer.patch'),
)
# Git for Windows checks out some untouched submodule files with CRLF while
# earlier source layers write LF. Normalize only this layer's target files and
# patch input so the exact .733 content guards work on both platforms.
originals = {}
with tempfile.TemporaryDirectory(prefix='tpr-734-patches-') as temporary:
    prepared = []
    for directory, patch in patches:
        payload = patch.read_bytes().replace(b'\r\n', b'\n')
        for line in payload.decode('utf-8').splitlines():
            if not line.startswith('--- '):
                continue
            name = line[4:]
            if name == '/dev/null':
                continue
            name = json.loads(name) if name.startswith('"') else name
            if not name.startswith('a/') or '..' in Path(name).parts:
                raise RuntimeError(f'.734 unexpected patch target: {name}')
            target = directory / name[2:]
            before = target.read_bytes()
            normalized = before.replace(b'\r\n', b'\n')
            if before != normalized:
                originals[target] = before
                target.write_bytes(normalized)
        normalized_patch = Path(temporary) / patch.name
        normalized_patch.write_bytes(payload)
        prepared.append((directory, patch, normalized_patch))
    # Check both repositories before applying either feature patch.
    try:
        for directory, _, patch in prepared:
            subprocess.run(['git', '-C', str(directory), 'apply', '--check', str(patch)], check=True)
    except Exception:
        for target, before in originals.items():
            target.write_bytes(before)
        raise
    for directory, original_patch, patch in prepared:
        subprocess.run(['git', '-C', str(directory), 'apply', str(patch)], check=True)
        print(f'Applied {original_patch.name}: SHA256 {hashlib.sha256(patch.read_bytes()).hexdigest()}')

for relative in ['src/dusk/startup_guard.hpp', 'src/dusk/ui/prelaunch.cpp',
                 'mods/randomizer/src/session.cpp', 'platforms/uwp/Package.appxmanifest']:
    path = root / relative
    source = path.read_text(encoding='utf-8')
    if '1.4.1.733' not in source:
        raise RuntimeError(f'.734 expected forward .733 identity in {relative}')
    source = source.replace('1.4.1.733','1.4.1.734')
    if relative == 'src/dusk/startup_guard.hpp':
        old = 'detail::markerPath = userPath / "xbox-startup-stage-733.txt";'
        if source.count(old) != 1: raise RuntimeError('.734 startup journal anchor changed')
        source = source.replace(old, 'detail::markerPath = userPath / "xbox-startup-stage-734.txt";')
        old = '''            // Carry forward the .732 checkpoint when updating after a hardware crash.
            std::ifstream prior(userPath / "xbox-startup-stage-732.txt", std::ios::binary);
            if (prior) {
                std::getline(prior, detail::previousStage);
            }'''
        new = '''            // Carry forward the latest pre-upgrade crash checkpoint.
            for (const char* journal : {"xbox-startup-stage-733.txt", "xbox-startup-stage-732.txt"}) {
                std::ifstream prior(userPath / journal, std::ios::binary);
                if (prior) {
                    std::getline(prior, detail::previousStage);
                    break;
                }
            }'''
        if source.count(old) != 1: raise RuntimeError('.734 previous crash journal anchor changed')
        source = source.replace(old,new)
    path.write_text(source,encoding='utf-8',newline='\n')

print('Applied .734 logical Soul rewards for all enabled item checks and stable pot/pumpkin identity across save callbacks; preserved .733 reset/audio/transition code.')
