"""Extend the signed host registry without modifying the existing .743 ports."""
import argparse
import json
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('source', type=Path)
a = p.parse_args()
root = a.source.resolve()
control = Path(__file__).resolve().parents[1]
mods = json.loads((control / 'NATIVE_MODS-744.json').read_text())['mods']

def edit(name, callback):
    path = root / name
    text = path.read_text().replace('\r\n', '\n')
    path.write_text(callback(text), encoding='utf-8', newline='\n')

def replace(text, old, new):
    assert text.count(old) == 1, 'Native registry anchor changed: ' + old[:80]
    return text.replace(old, new)

def metadata(text):
    sections, bounds = [], []
    for mod in mods:
        section = mod['metadataSection']
        sections.append('#elif defined(' + mod['metadataDefine'] + ')\n' + ''.join('#pragma section("' + section + '$' + letter + '", read, write)\n' for letter in ['a', 'd', 'z']) + '#define DUSK_MOD_META_RECORD_SECTION "' + section + '$d"\n')
        bounds.append('#elif defined(' + mod['metadataDefine'] + ')\n#define DUSK_MOD_META_BOUNDS_A "' + section + '$a"\n#define DUSK_MOD_META_BOUNDS_Z "' + section + '$z"\n')
    anchor = '#elif defined(DUSK_STATIC_CONTROLLER_UI)'
    assert text.count(anchor) == 2
    text = text.replace(anchor, ''.join(sections) + anchor, 1)
    pos = text.index('#define DUSK_MOD_META_BOUNDS_A')
    prefix, tail = text[:pos], text[pos:]
    return prefix + replace(tail, anchor, ''.join(bounds) + anchor)

edit('sdk/include/mods/meta.hpp', metadata)

def versions(text):
    return replace(text, 'inline constexpr AdditionalPortVersion AdditionalPorts[] = {',
        'inline constexpr AdditionalPortVersion AdditionalPorts[] = {\n' + ''.join('    {"' + m['id'] + '", "' + m['version'] + '"},\n' for m in mods))

edit('src/dusk/mods/uwp_ports.hpp', versions)

def loader(text):
    anchor = 'ModResult luau_runtime_mod_shutdown(ModError*);'
    declarations = '\n'.join('extern ModContext* ' + m['target'] + '_mod_ctx;\nextern const ModMeta ' + m['target'] + '_mod_meta;\nModResult ' + m['target'] + '_mod_initialize(ModError*);\nModResult ' + m['target'] + '_mod_update(ModError*);\nModResult ' + m['target'] + '_mod_shutdown(ModError*);' for m in mods)
    text = replace(text, anchor, anchor + '\n' + declarations)
    anchor = '    } else {\n        return false;\n    }\n    if (!parse_meta(*native, mod))'
    dispatch = ''.join('    } else if (mod.metadata.id == "' + m['id'] + '") {\n        native->meta = &' + m['target'] + '_mod_meta;\n        native->contextSymbol = &' + m['target'] + '_mod_ctx;\n        native->fn_initialize = &' + m['target'] + '_mod_initialize;\n        native->fn_update = &' + m['target'] + '_mod_update;\n        native->fn_shutdown = &' + m['target'] + '_mod_shutdown;\n' for m in mods)
    return replace(text, anchor, dispatch + anchor)

edit('src/dusk/mods/loader/natives.cpp', loader)
print('PASS: exact version gates, individual contexts and metadata for all three new native ports')
