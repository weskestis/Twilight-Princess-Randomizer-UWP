"""Harden startup as a checked layer on the complete green .738 build."""
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
    if '1.4.1.738' not in (root/name).read_text(encoding='utf-8'):
        raise RuntimeError('.739 requires the complete green .738 source: '+name)
patch_name = 'xbox-package-startup-739-host.patch'
payload = (control/'patches'/patch_name).read_bytes().replace(b'\r\n', b'\n')
expected = json.loads((control/'APP_IDENTITY-739.json').read_text())['sourcePatches'][patch_name]
if hashlib.sha256(payload).hexdigest() != expected:
    raise RuntimeError('.739 package startup patch hash mismatch')
originals = {}
for line in payload.decode('utf-8').splitlines():
    if not line.startswith('--- a/'):
        continue
    name = line[6:]
    if name.startswith('/') or '..' in Path(name).parts:
        raise RuntimeError('.739 unsafe patch path')
    path = root/name
    before = path.read_bytes()
    if before != before.replace(b'\r\n', b'\n'):
        originals[path] = before
        path.write_bytes(before.replace(b'\r\n', b'\n'))
with tempfile.TemporaryDirectory(prefix='tpr-739-patch-') as directory:
    patch = Path(directory)/patch_name
    patch.write_bytes(payload)
    try:
        subprocess.run(['git', '-C', str(root), 'apply', '--check', str(patch)], check=True)
    except Exception:
        for path, before in originals.items():
            path.write_bytes(before)
        raise
    subprocess.run(['git', '-C', str(root), 'apply', str(patch)], check=True)

def once(text, old, new, label):
    if text.count(old) != 1:
        raise RuntimeError('.739 '+label+' anchor changed')
    return text.replace(old, new, 1)

# Check before launcher audio/card/hook/session changes. Unknown APIs and the
# measured Xbox Game budget use the existing launch flow without restrictions.
path = root/'src/dusk/ui/prelaunch.cpp'
text = once(path.read_text(encoding='utf-8'), '#include "dusk/startup_guard.hpp"',
    '#include "dusk/startup_guard.hpp"\n#include "dusk/xbox_memory_budget.hpp"', 'launcher include')
anchor = '''            if (prelaunch_state().activeDiscPath.empty()) {
                open_iso_picker();
                return;
            }
'''
guard = '''
#if defined(_UWP)
            xbox_memory::record(ConfigPath, "prelaunch.memory-budget", "launcher");
            if (xbox_memory::needs_game_allowance(xbox_memory::query())) {
                auto dismiss = [](Modal& modal) { modal.pop(); };
                push(std::make_unique<Modal>(Modal::Props{
                    .title = "Xbox memory allowance",
                    .bodyText = "Xbox is allowing Dusklight only the App memory budget. "
                        "In Xbox Dev Home, set Dusklight's type to Game. Fully close "
                        "Dusklight and reopen it, then try Play again.",
                    .actions = {{.label = "OK", .onPressed = dismiss}},
                    .onDismiss = dismiss,
                    .icon = "warning",
                }));
                return;
            }
#endif
'''
text = once(text, anchor, anchor+guard, 'prelaunch budget gate')
path.write_text(text, encoding='utf-8', newline='\n')
for name in identity_files:
    text = (root/name).read_text(encoding='utf-8').replace('1.4.1.738', '1.4.1.739')
    if name == 'src/dusk/startup_guard.hpp':
        text = once(text, 'detail::markerPath = userPath / "xbox-startup-stage-738.txt";',
            'detail::markerPath = userPath / "xbox-startup-stage-739.txt";', 'current journal')
        text = once(text,
            '{"xbox-startup-stage-737.txt", "xbox-startup-stage-736.txt", "xbox-startup-stage-735.txt", "xbox-startup-stage-734.txt", "xbox-startup-stage-733.txt", "xbox-startup-stage-732.txt"}',
            '{"xbox-startup-stage-738.txt", "xbox-startup-stage-737.txt", "xbox-startup-stage-736.txt", "xbox-startup-stage-735.txt", "xbox-startup-stage-734.txt", "xbox-startup-stage-733.txt", "xbox-startup-stage-732.txt"}', 'previous journals')
    (root/name).write_text(text, encoding='utf-8', newline='\n')
print('Applied .739 atomic package preparation, late native context binding, package/operation/memory diagnostics and pre-Play Xbox App-budget guard; retained the complete .738 game.')
