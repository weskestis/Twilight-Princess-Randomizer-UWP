"""Apply the forward .734 layer after the .733 source and lifecycle guards."""
from __future__ import annotations
import hashlib
import os
from pathlib import Path
import subprocess

root = Path(os.environ['TPR_SRC'])
control = Path(os.environ.get('TPR_CONTROL', Path(__file__).resolve().parents[1]))
patches = (
    (root, control / 'patches/souls-all-checks-734-host.patch'),
    (root / 'mods/randomizer', control / 'patches/souls-all-checks-734-randomizer.patch'),
)
# Check both repositories before mutating either one. Keep .733 as the base.
for directory, patch in patches:
    subprocess.run(['git', '-C', str(directory), 'apply', '--check', str(patch)],check=True)
for directory, patch in patches:
    subprocess.run(['git', '-C', str(directory), 'apply', str(patch)],check=True)
    print(f'Applied {patch.name}: SHA256 {hashlib.sha256(patch.read_bytes()).hexdigest()}')

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
