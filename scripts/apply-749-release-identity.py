"""Verify all inherited controls and advance the release after native Soul pickup validation."""
import hashlib, json, os
from pathlib import Path

r = Path(os.environ['TPR_SRC'])
c = Path(os.environ['TPR_CONTROL'])
previous = json.loads((c / 'APP_IDENTITY-748.json').read_text())
identity = json.loads((c / 'APP_IDENTITY-749.json').read_text())
assert previous['version'] == '1.4.1.748' and identity['version'] == '1.4.1.749'
for key, folder in [('sourceControls', c), ('sourcePatches', c / 'patches')]:
    for name, expected in identity[key].items():
        actual = hashlib.sha256((folder / name).read_bytes().replace(b'\r\n', b'\n')).hexdigest()
        if actual != expected:
            raise RuntimeError('.749 source control hash mismatch: ' + name)
required = {
    'mods/randomizer/src/messages.cpp': ['bool soul_pickup_messages_active() noexcept',
        'return !s_soulPickupMessages.empty();', 's_soulPickupMessages.clear();'],
    'mods/randomizer/src/messages.hpp': ['bool soul_pickup_messages_active() noexcept;'],
    'mods/randomizer/src/hooks.cpp': ['tpr_resolve_soul_pickup_for_actor',
        '!messages::soul_pickup_messages_active()', 'messages::pickup_message_id(getSoulItemMessageID(physicalItemId, itemActor))'],
    'src/d/actor/d_a_alink_demo.inc': ['field_0x32cc = tpr_resolve_soul_pickup_for_actor(',
        'static_cast<u8>(mProcVar2.field_0x300c), item_partner_p);',
        'pickupMessageFinished = tpr_complete_breakable_pickup_message('],
}
for name, markers in required.items():
    text = (r / name).read_text()
    for marker in markers:
        if marker not in text:
            raise RuntimeError('.749 runtime missing ' + marker + ' in ' + name)
assert identity['previousStartupJournals'] == [previous['startupJournal']] + previous['previousStartupJournals']
originals = {}
prepared = {}
for name in ['src/dusk/startup_guard.hpp', 'src/dusk/ui/prelaunch.cpp', 'mods/randomizer/src/session.cpp',
    'platforms/uwp/Package.appxmanifest', 'src/dusk/xbox_memory_budget.hpp']:
    path = r / name
    originals[path] = path.read_bytes()
    text = originals[path].decode().replace('\r\n', '\n')
    if previous['version'] not in text or identity['version'] in text:
        raise RuntimeError('Unexpected .749 source identity: ' + name)
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
print('Verified .749 native Soul routing, preserved every .748 source layer and startup journal, advanced package identity')
