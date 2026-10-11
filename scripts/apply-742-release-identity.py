"""Advance the complete .741 source only after all .742 fixes are applied."""
import hashlib
import json
import os
from pathlib import Path

root = Path(os.environ["TPR_SRC"])
control = Path(os.environ["TPR_CONTROL"])
previous = json.loads((control / "APP_IDENTITY-741.json").read_text(encoding="utf-8"))
identity = json.loads((control / "APP_IDENTITY-742.json").read_text(encoding="utf-8"))
if previous["version"] != "1.4.1.741" or identity["version"] != "1.4.1.742":
    raise RuntimeError("Unexpected .742 release identity")
for name in ("xbox-mod-menu-742-host.patch", "xbox-slot-a-742-host.patch", "xbox-slot-a-742-aurora.patch"):
    payload = (control / "patches" / name).read_bytes().replace(b"\r\n", b"\n")
    if hashlib.sha256(payload).hexdigest() != identity["sourcePatches"][name]:
        raise RuntimeError(".742 release patch hash mismatch: " + name)
required_sources = {
    "src/dusk/imgui/PureItemTracker.cpp": ["pureItemTrackerPositionX", "pureItemTrackerPositionY"],
    "src/dusk/ui/mod_texture_provider.cpp": ["kMaxModIconDimension = 512", "kMaxModPreviewDimension = 1024"],
    "src/dusk/ui/menu_bar.cpp": ["menu.mods.open.game"],
    "src/dusk/ui/prelaunch.cpp": ["menu.mods.open.launcher"],
    "src/m_Do/m_Do_MemCard.cpp": ["card.slot-a.probe:"],
    "extern/aurora/lib/card/CardGciFolder.cpp": ["Provisioned GCI card"],
}
for name, markers in required_sources.items():
    text = (root / name).read_text(encoding="utf-8")
    for marker in markers:
        if marker not in text:
            raise RuntimeError(".742 source layer is missing " + marker + " in " + name)
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
        raise RuntimeError(".742 requires the complete .741 source: " + name)
    text = text.replace(previous["version"], identity["version"])
    if name == "src/dusk/startup_guard.hpp":
        current = 'detail::markerPath = userPath / "' + previous["startupJournal"] + '";'
        old_journals = "{" + ", ".join(json.dumps(value) for value in previous["previousStartupJournals"]) + "}"
        new_journals = [previous["startupJournal"]] + previous["previousStartupJournals"]
        if identity["previousStartupJournals"] != new_journals:
            raise RuntimeError(".742 must retain every previous startup journal")
        if text.count(current) != 1 or text.count(old_journals) != 1:
            raise RuntimeError(".742 startup journal anchors changed")
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
print("Advanced .742 package, launcher, session, memory-budget and startup identities; retained .741 through .732 journals and all 67 strict scenarios.")
