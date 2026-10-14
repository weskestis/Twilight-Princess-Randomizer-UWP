"""Verify all inherited controls and advance the release after cosmetics validation."""
import hashlib, json, os
from pathlib import Path

r = Path(os.environ['TPR_SRC'])
c = Path(os.environ['TPR_CONTROL'])
previous = json.loads((c / 'APP_IDENTITY-747.json').read_text())
identity = json.loads((c / 'APP_IDENTITY-748.json').read_text())
assert previous['version'] == '1.4.1.747' and identity['version'] == '1.4.1.748'
for key, folder in [('sourceControls', c), ('sourcePatches', c / 'patches')]:
    for name, expected in identity[key].items():
        actual = hashlib.sha256((folder / name).read_bytes().replace(b'\r\n', b'\n')).hexdigest()
        if actual != expected:
            raise RuntimeError('.748 source control hash mismatch: ' + name)
required = {
    'mods/cosmetics/src/mod.cpp': ['Off - Keep My Colors', 'Vanilla Colors', 'get_saved_str_option',
        'set_manual_color_control', 'set_manual_midna_control', 'control.user_data = &cvar;', 'tpr_appearance_set_mode(0u);'],
    'mods/cosmetics/src/appearance_runtime.hpp': ['Mode::Vanilla', 'mode_ = Mode::Vanilla'],
    'src/dusk/ui/menu_colors.cpp': ['settings.vanillaAppearance.getValue()', 'if (s.vanillaAppearance.getValue()) return 4u;'],
    'src/dusk/settings.cpp': ['"cosmetics.vanillaAppearance", false', 'Register(g_userSettings.cosmetics.vanillaAppearance);'],
    'src/dusk/companion/server.cpp': ['/v1/pair', '/v1/snapshot', 'now + 500ms'],
    'src/dusk/ui/companion_window.cpp': ['Pair Android', 'Approve matching code'],
    'src/dusk/ui/mods_window.cpp': ['std::chrono::milliseconds(180)', 'commit_detail(true);', 'if (mCulled) return;'],
    'src/dusk/imgui/PureItemTracker.cpp': ['###GameTimerOverlayV1', '###DeathCounterOverlayV1'],
}
for name, markers in required.items():
    text = (r / name).read_text()
    for marker in markers:
        if marker not in text:
            raise RuntimeError('.748 runtime missing ' + marker + ' in ' + name)
assert identity['previousStartupJournals'] == [previous['startupJournal']] + previous['previousStartupJournals']
originals = {}
prepared = {}
for name in ['src/dusk/startup_guard.hpp', 'src/dusk/ui/prelaunch.cpp', 'mods/randomizer/src/session.cpp',
    'platforms/uwp/Package.appxmanifest', 'src/dusk/xbox_memory_budget.hpp']:
    path = r / name
    originals[path] = path.read_bytes()
    text = originals[path].decode().replace('\r\n', '\n')
    if previous['version'] not in text or identity['version'] in text:
        raise RuntimeError('Unexpected .748 source identity: ' + name)
    text = text.replace(previous['version'], identity['version'])
    if name == 'src/dusk/startup_guard.hpp':
        marker = 'detail::markerPath = userPath / "' + previous['startupJournal'] + '";'
        journals = '{' + ', '.join(json.dumps(value) for value in previous['previousStartupJournals']) + '}'
        assert text.count(marker) == 1 and text.count(journals) == 1
        text = text.replace(marker, 'detail::markerPath = userPath / "' + identity['startupJournal'] + '";')
        text = text.replace(journals, '{' + ', '.join(json.dumps(value) for value in identity['previousStartupJournals']) + '}')
    prepared[path] = text.encode()
try:
    for path, data in prepared.items():
        path.write_bytes(data)
except Exception:
    for path, data in originals.items():
        path.write_bytes(data)
    raise
print('Verified .748 cosmetics controls, preserved .744-.747 layers and startup journals, advanced package identity')
