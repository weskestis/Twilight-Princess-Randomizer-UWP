"""Advance the complete .742 source only after all .743 fixes are applied."""
import hashlib
import json
import os
from pathlib import Path

root = Path(os.environ["TPR_SRC"])
control = Path(os.environ["TPR_CONTROL"])
previous = json.loads((control / "APP_IDENTITY-742.json").read_text(encoding="utf-8"))
identity = json.loads((control / "APP_IDENTITY-743.json").read_text(encoding="utf-8"))
if previous["version"] != "1.4.1.742" or identity["version"] != "1.4.1.743":
    raise RuntimeError("Unexpected .743 release identity")
for name, expected in identity['sourceControls'].items():
    payload = (control / name).read_bytes().replace(b"\r\n", b"\n")
    if hashlib.sha256(payload).hexdigest() != expected:
        raise RuntimeError('.743 source control hash mismatch: ' + name)
for name in identity['sourcePatches']:
    payload = (control / 'patches' / name).read_bytes().replace(b"\r\n", b"\n")
    if hashlib.sha256(payload).hexdigest() != identity['sourcePatches'][name]:
        raise RuntimeError('.743 source patch hash mismatch: ' + name)
required_sources = {
    'mods/controller_ui/src/mod.cpp': ['s_controllerStyleLocked ? s_useSwitch', 'Restart'],
    'src/dusk/mods/install_paths.hpp': ['symlink_status', 'lexically_normal'],
    'src/dusk/mods/loader/loader.cpp': ['exclusiveGroup', 'install_paths'],
    'src/dusk/ui/mod_texture_provider.cpp': ['kMaxModIconDimension = 256', 'kMaxCachedImageBytes = 16'],
    'src/m_Do/m_Do_MemCard.cpp': ['card.direct-gci-and-randomizer-743', 'xboxDirectSaveName'],
    'src/dusk/xbox_vanilla_save.hpp': ['gczelda2', '.bak'],
    'src/d/actor/d_a_title.cpp': ['Xbox title scene request accepted (743)'],
    'src/dusk/mods/loader/natives.cpp': ['port_twilit_essentials_mod_initialize', 'uwp_ports::native_id_known'],
}
for name, markers in required_sources.items():
    text = (root / name).read_text(encoding='utf-8')
    for marker in markers:
        if marker not in text:
            raise RuntimeError('.743 source layer is missing ' + marker + ' in ' + name)
identity_files = [
    "src/dusk/startup_guard.hpp", "src/dusk/ui/prelaunch.cpp",
    "mods/randomizer/src/session.cpp", "platforms/uwp/Package.appxmanifest",
    "src/dusk/xbox_memory_budget.hpp",
]
originals, prepared = {}, {}
for name in identity_files:
    path = root / name
    originals[path] = path.read_bytes()
    text = originals[path].decode("utf-8").replace("\r\n", "\n")
    if previous["version"] not in text or identity["version"] in text:
        raise RuntimeError(".743 requires the complete .742 source: " + name)
    text = text.replace(previous["version"], identity["version"])
    if name == "src/dusk/startup_guard.hpp":
        current = 'detail::markerPath = userPath / "' + previous["startupJournal"] + '";'
        old_journals = "{" + ", ".join(json.dumps(value) for value in previous["previousStartupJournals"]) + "}"
        new_journals = [previous["startupJournal"]] + previous["previousStartupJournals"]
        if identity["previousStartupJournals"] != new_journals:
            raise RuntimeError(".743 must retain every previous startup journal")
        if text.count(current) != 1 or text.count(old_journals) != 1:
            raise RuntimeError(".743 startup journal anchors changed")
        text = text.replace(current, 'detail::markerPath = userPath / "' + identity["startupJournal"] + '";')
        text = text.replace(old_journals, "{" + ", ".join(json.dumps(value) for value in new_journals) + "}")
    prepared[path] = text.encode("utf-8")
try:
    for path, payload in prepared.items():
        path.write_bytes(payload)
except Exception:
    for path, payload in originals.items():
        path.write_bytes(payload)
    raise
print("Advanced .743 package, launcher, session, memory-budget and startup identities; retained every previous journal and all 67 strict scenarios.")
