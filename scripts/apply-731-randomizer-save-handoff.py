from __future__ import annotations

import os
import re
from pathlib import Path


ROOT = Path(os.environ["TPR_SRC"])


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8").replace("\r\n", "\n")


def write_text(path: Path, text: str) -> None:
    path.write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one anchor, found {count}")
    return text.replace(old, new, 1)


def function_span(text: str, signature: str) -> tuple[int, int]:
    start = text.find(signature)
    if start < 0:
        raise RuntimeError(f"function signature not found: {signature}")
    open_brace = text.find("{", start)
    if open_brace < 0:
        raise RuntimeError(f"function open brace not found: {signature}")
    depth = 0
    i = open_brace
    in_string = False
    in_char = False
    escape = False
    while i < len(text):
        ch = text[i]
        if escape:
            escape = False
        elif ch == "\\" and (in_string or in_char):
            escape = True
        elif ch == '"' and not in_char:
            in_string = not in_string
        elif ch == "'" and not in_string:
            in_char = not in_char
        elif not in_string and not in_char:
            if ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    return start, i + 1
        i += 1
    raise RuntimeError(f"function close brace not found: {signature}")


save_path = ROOT / "src/dusk/mods/svc/save.cpp"
save = read_text(save_path)

save = replace_once(
    save,
    'constexpr const char* kDefaultSaveName = "gczelda2";\n'
    'constexpr s32 kCardChannel = 0;',
    'constexpr const char* kDefaultSaveName = "gczelda2";\n'
    'constexpr std::string_view kXboxRandomizerSaveName = "randomizer-xbox";\n'
    'constexpr const char* kXboxRandomizerSidecarDir = "randomizer-xbox-mods";\n'
    'constexpr s32 kCardChannel = 0;',
    ".731 direct-save constants",
)

save = replace_once(
    save,
    'std::filesystem::path legacy_sidecar_path() {\n'
    '    return ConfigPath / kLegacySidecarName;\n'
    '}\n',
    'std::filesystem::path legacy_sidecar_path() {\n'
    '    return ConfigPath / kLegacySidecarName;\n'
    '}\n'
    '\n'
    'std::filesystem::path xbox_randomizer_sidecar_directory() {\n'
    '    return ConfigPath / kXboxRandomizerSidecarDir;\n'
    '}\n',
    ".731 direct-sidecar helper",
)

save = replace_once(
    save,
    'std::optional<std::filesystem::path> mod_sidecar_path(\n'
    '    std::string_view saveName, std::string_view modId) {\n'
    '    const auto storage = card_storage();\n'
    '    if (!storage) {\n'
    '        return std::nullopt;\n'
    '    }\n'
    '    return save_sidecar_directory(*storage, saveName) / (std::string{modId} + ".json");\n'
    '}',
    'std::optional<std::filesystem::path> mod_sidecar_path(\n'
    '    std::string_view saveName, std::string_view modId) {\n'
    '#if defined(_UWP)\n'
    '    if (saveName == kXboxRandomizerSaveName) {\n'
    '        return xbox_randomizer_sidecar_directory() / (std::string{modId} + ".json");\n'
    '    }\n'
    '#endif\n'
    '    const auto storage = card_storage();\n'
    '    if (!storage) {\n'
    '        return std::nullopt;\n'
    '    }\n'
    '    return save_sidecar_directory(*storage, saveName) / (std::string{modId} + ".json");\n'
    '}',
    ".731 direct-sidecar path",
)

old_load = (
    '    const auto storage = card_storage();\n'
    '    if (!storage) {\n'
    '        return store;\n'
    '    }\n'
    '\n'
    '    const auto& diskId = version::getDiskID();\n'
    '    if (const auto migrated = migrate_legacy_sidecar(*storage,\n'
    '            {diskId.company, sizeof(diskId.company)}, {diskId.gameName, sizeof(diskId.gameName)});\n'
    '        !migrated)\n'
    '    {\n'
    '        Log.error("{}", migrated.message);\n'
    '        return store;\n'
    '    }\n'
    '    store.loaded = true;\n'
    '\n'
    '    const auto directory = save_sidecar_directory(*storage, saveName);'
)
new_load = (
    '    std::filesystem::path directory;\n'
    '#if defined(_UWP)\n'
    '    if (saveName == kXboxRandomizerSaveName) {\n'
    '        // Xbox Randomizer gameplay uses a direct LocalState save instead of the emulated\n'
    '        // GameCube CARD. Keep its mod blob sidecars in LocalState too so seed association\n'
    '        // is available during the immediate save-loaded callback and on later launches.\n'
    '        store.loaded = true;\n'
    '        directory = xbox_randomizer_sidecar_directory();\n'
    '    } else\n'
    '#endif\n'
    '    {\n'
    '        const auto storage = card_storage();\n'
    '        if (!storage) {\n'
    '            return store;\n'
    '        }\n'
    '\n'
    '        const auto& diskId = version::getDiskID();\n'
    '        if (const auto migrated = migrate_legacy_sidecar(*storage,\n'
    '                {diskId.company, sizeof(diskId.company)},\n'
    '                {diskId.gameName, sizeof(diskId.gameName)});\n'
    '            !migrated)\n'
    '        {\n'
    '            Log.error("{}", migrated.message);\n'
    '            return store;\n'
    '        }\n'
    '        store.loaded = true;\n'
    '        directory = save_sidecar_directory(*storage, saveName);\n'
    '    }'
)
save = replace_once(save, old_load, new_load, ".731 direct-sidecar load")

for marker in (
    'kXboxRandomizerSaveName = "randomizer-xbox"',
    'kXboxRandomizerSidecarDir = "randomizer-xbox-mods"',
    'xbox_randomizer_sidecar_directory()',
    'if (saveName == kXboxRandomizerSaveName)',
    'directory = xbox_randomizer_sidecar_directory();',
):
    if marker not in save:
        raise RuntimeError(f".731 save-service marker missing after patch: {marker}")

write_text(save_path, save)


session_path = ROOT / "mods/randomizer/src/session.cpp"
session = read_text(session_path)

new_save_start, new_save_end = function_span(
    session, "ModResult onNewSave(void*, ModError* error)")
new_save = session[new_save_start:new_save_end]
new_save = replace_once(
    new_save,
    '    svc_mng.save->set_blob(svc_mng.mod_ctx, kSeedHashBlobName, hash.data(), hash.size());',
    '    const ModResult blobResult = svc_mng.save->set_blob(\n'
    '        svc_mng.mod_ctx, kSeedHashBlobName, hash.data(), hash.size());\n'
    '    if (blobResult != MOD_OK) {\n'
    '        // Do not disable the Randomizer here. The immediate save-loaded callback can recover\n'
    '        // from the still-pending selected seed, while .731 also fixes Xbox direct sidecars.\n'
    '        mods::log::warn("seed_hash staging unavailable during new save (result {})",\n'
    '            static_cast<int>(blobResult));\n'
    '    }',
    ".731 new-save seed staging",
)
clear_block = (
    '    // One selected seed belongs to one new save. Force the next slot back through\n'
    '    // seed selection/generation instead of silently reusing this hash.\n'
    '    g_pending_seed_hash.clear();\n'
)
new_save = replace_once(new_save, clear_block, "", ".731 pending-seed handoff")
session = session[:new_save_start] + new_save + session[new_save_end:]

load_start, load_end = function_span(
    session, "ModResult onSaveLoaded(void*, ModError*)")
replacement = '''ModResult onSaveLoaded(void*, ModError* error) {
    std::string hash;
    size_t size = 0;
    const ModResult sizeResult =
        svc_mng.save->get_blob(mod_ctx, kSeedHashBlobName, nullptr, &size);

    if (sizeResult == MOD_OK && size != 0) {
        hash.assign(size, char{});
        if (svc_mng.save->get_blob(mod_ctx, kSeedHashBlobName, hash.data(), &size) != MOD_OK) {
            deactivateSeed();
            return mods::set_error(
                error, MOD_ERROR, "Randomizer seed association could not be read");
        }
    } else if (!g_pending_seed_hash.empty() && g_pending_seed_hash != "None") {
        // A brand-new Xbox save reaches save-loaded immediately after onNewSave. If blob storage
        // was not ready for that first staging call, recover from the selected seed still held by
        // the gate instead of disabling the entire mod.
        hash = g_pending_seed_hash;
        const ModResult recoverResult = svc_mng.save->set_blob(
            svc_mng.mod_ctx, kSeedHashBlobName, hash.data(), hash.size());
        if (recoverResult != MOD_OK) {
            mods::log::warn("seed_hash recovery staging still unavailable (result {})",
                static_cast<int>(recoverResult));
        } else {
            mods::log::info("recovered seed_hash during Xbox save-loaded callback");
        }
    } else {
        mods::log::error("seed_hash not found and no pending seed is available");
        deactivateSeed();
        return mods::set_error(
            error, MOD_ERROR, "Randomizer save is missing its seed association");
    }

    if (randomizer_GetContext().mHash != hash) {
        deactivateSeed();
        if (!activateSeed(hash.c_str())) {
            const std::string detail = lastSeedActivationError();
            return mods::set_error(error, MOD_ERROR,
                detail.empty() ? "The Randomizer seed could not be reactivated" : detail.c_str());
        }
    }

    loadAncientDocumentNum();
    // The seed has now crossed the save-loaded boundary safely; only now consume the gate state.
    g_pending_seed_hash.clear();
    return MOD_OK;
}'''
session = session[:load_start] + replacement + session[load_end:]

for marker in (
    'seed_hash staging unavailable during new save',
    'recovered seed_hash during Xbox save-loaded callback',
    'Randomizer save is missing its seed association',
    'only now consume the gate state',
    'onObservedNewSave',
    'onObservedSaveLoaded',
):
    if marker not in session:
        raise RuntimeError(f".731 Randomizer callback marker missing after patch: {marker}")

write_text(session_path, session)
print("Applied .731 Xbox Randomizer direct-sidecar and save-loaded recovery hardening.")
