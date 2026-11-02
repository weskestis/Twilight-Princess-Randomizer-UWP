"""Add the preview to the fully assembled, verified .749 source tree.

Retail model bytes are deliberately supplied only during private final packaging.
The public build contains the complete native implementation and resource loader.
"""
import argparse
import hashlib
import json
from pathlib import Path


def replace(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError('Pokemon .749 source anchor changed: ' + old[:100])
    return text.replace(old, new)


def stage(root, control):
    experiment = control / 'experimental/pokemon_battle'
    target = 'port_pokemon_model_lab'
    metadata_define = 'DUSK_STATIC_POKEMON_MODEL_LAB'
    metadata_section = 'pkmmeta'
    manifest = json.loads((experiment / 'model_lab/mod.json').read_text())
    staged = {}

    def edit(name, callback):
        staged[root / name] = callback((root / name).read_text().replace('\r\n', '\n'))

    identity = root / 'platforms/uwp/Package.appxmanifest'
    if 'Version="1.4.1.749"' not in identity.read_text():
        raise RuntimeError('Pokemon preview must be staged on verified .749')
    if target in (root / 'mods/uwp_ports/CMakeLists.txt').read_text():
        raise RuntimeError('Pokemon preview is already staged')
    edit('src/dusk/game_mode.cpp', lambda t: replace(t,
        'GameMode vanilla{kVanillaGameModeId, "Vanilla"}',
        'GameMode vanilla{kVanillaGameModeId, "Pokemon Vanilla"}'))
    edit('mods/randomizer/src/session.cpp', lambda t: replace(t,
        '.full_name = "Randomizer"', '.full_name = "Pokemon Randomizer"'))
    edit('mods/uwp_ports/CMakeLists.txt', lambda t: t + '\nadd_subdirectory(' + target + ')\n')
    edit('platforms/uwp/NativePorts743.cmake', lambda t: t + 'list(APPEND DUSK_UWP_ADDITIONAL_PORTS ' + target + ')\n')

    def metadata(text):
        anchor = '#elif defined(DUSK_STATIC_CONTROLLER_UI)'
        if text.count(anchor) != 2:
            raise RuntimeError('Expected independent .749 metadata records and bounds')
        sections = '#elif defined(' + metadata_define + ')\n' + ''.join(
            '#pragma section("' + metadata_section + '$' + letter + '", read, write)\n'
            for letter in ['a', 'd', 'z']) + '#define DUSK_MOD_META_RECORD_SECTION "pkmmeta$d"\n'
        bounds = '#elif defined(' + metadata_define + ')\n#define DUSK_MOD_META_BOUNDS_A "pkmmeta$a"\n#define DUSK_MOD_META_BOUNDS_Z "pkmmeta$z"\n'
        text = text.replace(anchor, sections + anchor, 1)
        pos = text.index('#define DUSK_MOD_META_BOUNDS_A')
        return text[:pos] + replace(text[pos:], anchor, bounds + anchor)

    edit('sdk/include/mods/meta.hpp', metadata)
    edit('src/dusk/mods/uwp_ports.hpp', lambda t: replace(t,
        'inline constexpr AdditionalPortVersion AdditionalPorts[] = {',
        'inline constexpr AdditionalPortVersion AdditionalPorts[] = {\n    {"' + manifest['id'] + '", "' + manifest['version'] + '"},'))

    def loader(text):
        anchor = 'ModResult luau_runtime_mod_shutdown(ModError*);'
        declarations = '\nextern ModContext* ' + target + '_mod_ctx;\nextern const ModMeta ' + target + '_mod_meta;\n' + ''.join(
            'ModResult ' + target + '_mod_' + fn + '(ModError*);\n' for fn in ['initialize', 'update', 'shutdown'])
        text = replace(text, anchor, anchor + declarations)
        anchor = '    } else {\n        return false;\n    }\n    if (!parse_meta(*native, mod))'
        dispatch = '    } else if (mod.metadata.id == "' + manifest['id'] + '") {\n' + \
            '        native->meta = &' + target + '_mod_meta;\n' + \
            '        native->contextSymbol = &' + target + '_mod_ctx;\n' + ''.join(
            '        native->fn_' + fn + ' = &' + target + '_mod_' + fn + ';\n' for fn in ['initialize', 'update', 'shutdown'])
        return replace(text, anchor, dispatch + anchor)

    edit('src/dusk/mods/loader/natives.cpp', loader)
    port = root / 'mods/uwp_ports' / target
    for name in ['model_lab/mod.cpp', 'model_lab/gx_renderer.cpp', 'model_lab/gx_renderer.hpp',
                 'model_lab/spawn_context.hpp',
                 'model_lab/field.cpp', 'model_lab/field.hpp',
                 'model_lab/content.cpp','model_lab/content.hpp',
                 'model_lab/party_save.hpp',
                 'model_lab/field_effects.cpp', 'model_lab/field_effects.hpp',
                 'field/party.cpp', 'field/party.hpp', 'field/combat.cpp', 'field/combat.hpp',
                 'field/animation.cpp', 'field/animation.hpp',
                 'field/roster.cpp','field/roster.hpp','field/roster_data.inc',
                 'field/move_enum.inc','field/move_data.inc',
                 'field/type_chart.inc','field/ball.cpp','field/ball.hpp',
                 'runtime/effects.cpp','runtime/effects.hpp',
                 'runtime/model.cpp', 'runtime/model.hpp']:
        staged[port / name] = (experiment / name).read_text()
    staged[port / 'CMakeLists.txt'] = (experiment / 'model_lab/CMakeLists.uwp.txt').read_text()
    staged[port / 'mod.json'] = json.dumps(manifest, indent=2) + '\n'
    staged[port / 'res/model-pack.txt'] = '20 species packs and effects are added to res/pokemon/ during private final packaging.\n'
    proof = {'baseVersion': '1.4.1.749', 'baseCommit': '8b1a9bc011032b32b5e123af8d76ba644b28a971',
             'id': manifest['id'], 'version': manifest['version'], 'target': target,
             'metadataSection': metadata_section, 'sources': {}}
    for path, text in staged.items():
        data = text.encode('utf-8')
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        proof['sources'][path.relative_to(root).as_posix()] = hashlib.sha256(data).hexdigest()
    (root / 'platforms/uwp/optional_mods/POKEMON-PREVIEW.json').write_text(json.dumps(proof, indent=2) + '\n')
    print('PASS: .749 preview source, isolated SDK, signed registry, version gate and resource bundle staged')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('source', type=Path)
    args = p.parse_args()
    stage(args.source.resolve(), Path(__file__).resolve().parents[1])
