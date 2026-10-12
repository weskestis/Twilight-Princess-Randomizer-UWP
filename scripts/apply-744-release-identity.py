"""Advance the complete .743 source only after all .744 fixes are applied."""
import hashlib
import json
import os
from pathlib import Path

root = Path(os.environ["TPR_SRC"])
control = Path(os.environ["TPR_CONTROL"])
previous = json.loads((control / "APP_IDENTITY-743.json").read_text(encoding="utf-8"))
identity = json.loads((control / "APP_IDENTITY-744.json").read_text(encoding="utf-8"))
if previous["version"] != "1.4.1.743" or identity["version"] != "1.4.1.744":
    raise RuntimeError("Unexpected .744 release identity")
for name, expected in identity['sourceControls'].items():
    payload = (control / name).read_bytes().replace(b"\r\n", b"\n")
    if hashlib.sha256(payload).hexdigest() != expected:
        raise RuntimeError('.744 source control hash mismatch: ' + name)
for name in identity['sourcePatches']:
    payload = (control / 'patches' / name).read_bytes().replace(b"\r\n", b"\n")
    if hashlib.sha256(payload).hexdigest() != identity['sourcePatches'][name]:
        raise RuntimeError('.744 source patch hash mismatch: ' + name)
required_sources = {
    'src/dusk/ui/mod_texture_provider.cpp': ['kMaxModIconDimension = 128', 'kMaxCachedImageBytes = 16', 'prepared_image_path', 'image_use()'],
    'src/dusk/ui/mods_window.cpp': ['mPanelDelay.take(', 'mModPanelBuilt', 'menu.mods.panel:'],
    'src/dusk/ui/mod_panel_delay.hpp': ['milliseconds(100)', 'mResource'],
    'mods/randomizer/src/breakables.cpp': ['void inherit_spawned_item(', 's_spawnedItemKeys[childId] = *key;', 'std::none_of('],
    'mods/randomizer/src/hooks.cpp': ['inherit_spawned_item(actor, actor->m_item_id)', 'hookPostCheckEndPickupMessage', 'Recovered missing breakable pickup text (744)', 'resetPickupMessageGuard();'],
    'mods/randomizer/src/pickup_message_guard.hpp': ['seconds(5)', 'steady_clock'],
    'src/d/actor/d_a_obj_item.cpp': ['tpr_breakable_item_bound(this) ? 0'],
    'src/d/actor/d_a_obj_carry.cpp': ['breakable_tint::apply(', '*tprPotMarkerColor = tprPotSavedMarkerColor;'],
    'src/d/actor/d_a_obj_pumpkin.cpp': ['breakable_tint::apply(', '*tprPumpkinMarkerColor = tprPumpkinSavedMarkerColor;'],
    'src/d/actor/d_a_alldie.cpp': ['tpr_room_has_pending_soul_enemies'],
    'src/d/actor/d_a_tbox.cpp': ['tpr_room_has_pending_soul_enemies'],
    'mods/randomizer/src/enemy_souls.cpp': ['bool room_has_pending_enemies(', 'fopAcM_GetGroup(actor) == fopAc_ENEMY_e'],
    'src/d/actor/d_a_itembase.cpp': ['if (modelData == NULL)', 'if (pbtk == NULL)', 'if (pbpk == NULL)', 'if (pbck == NULL)', 'if (pbrk == NULL)', 'if (pbtp == NULL)'],
    'src/d/actor/d_a_shop_item.cpp': ['Xbox shop resource failure (744)', 'if (arcName == NULL) return 1;', 'invalid-heap-index'],
    'mods/uwp_ports/port_dof_hq/src/mod.cpp': ['xbox_effect_budget::fit(', '"xboxEffectHeight", 720'],
    'mods/uwp_ports/port_vbao/mods/vbao/src/mod.cpp': ['xbox_effect_budget::bound_ao_samples(', '"xboxBalanced", true'],
    'src/m_Do/m_Do_MemCard.cpp': ['card.direct-gci-and-randomizer-743', 'xboxDirectSaveName'],
    'src/dusk/xbox_vanilla_save.hpp': ['gczelda2', '.bak'],
    'src/d/actor/d_a_title.cpp': ['Xbox title scene request accepted (743)'],
    'src/dusk/mods/loader/natives.cpp': ['port_twilit_essentials_mod_initialize', 'uwp_ports::native_id_known', 'port_crests_of_courage_mod_initialize', 'port_a_link_between_twilight_mod_initialize', 'port_mfb_multiplayer_mod_initialize'],
    'mods/uwp_ports/port_crests_of_courage/src/voice_io.cpp': ['class XboxSpeakers', 'std::make_unique<XboxSpeakers>'],
    'mods/uwp_ports/port_a_link_between_twilight/src/albw_l1_input.cpp': ['s_getIndexForPort = &PADGetIndexForPort;', 'SDL_GetGamepadButton(static_cast<SDL_Gamepad*>'],
    'mods/uwp_ports/port_mfb_multiplayer/src/game/voice_chat.cpp': ['dusk_xbox_voice::Playback xboxOutput;', 'Xbox microphone input is unavailable; you can hear other players.'],
    'sdk/include/mods/svc/gfx.h': ['GFX_SERVICE_MINOR 4u', 'WGPUTexture colorTexture'],
}
for name, markers in required_sources.items():
    text = (root / name).read_text(encoding='utf-8')
    for marker in markers:
        if marker not in text:
            raise RuntimeError('.744 source layer is missing ' + marker + ' in ' + name)
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
        raise RuntimeError(".744 requires the complete .743 source: " + name)
    text = text.replace(previous["version"], identity["version"])
    if name == "src/dusk/startup_guard.hpp":
        current = 'detail::markerPath = userPath / "' + previous["startupJournal"] + '";'
        old_journals = "{" + ", ".join(json.dumps(value) for value in previous["previousStartupJournals"]) + "}"
        new_journals = [previous["startupJournal"]] + previous["previousStartupJournals"]
        if identity["previousStartupJournals"] != new_journals:
            raise RuntimeError(".744 must retain every previous startup journal")
        if text.count(current) != 1 or text.count(old_journals) != 1:
            raise RuntimeError(".744 startup journal anchors changed")
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
print("Advanced .744 package, launcher, session, memory-budget and startup identities; retained every previous journal and all 67 strict scenarios.")
