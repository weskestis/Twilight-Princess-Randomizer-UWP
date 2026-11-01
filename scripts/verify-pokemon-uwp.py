"""Check the real linked PE and resource manifest in the Xbox preview package."""
import argparse
import importlib.util
import json
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('package', type=Path)
a = p.parse_args()
control = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('pe_metadata', control / 'scripts/verify-739-native-metadata.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
descriptors = [('', 'modmeta'), ('cosmetics_', 'cosmeta'), ('controller_ui_', 'cuimeta'), ('luau_runtime_', 'luumeta')]
for name in ['NATIVE_MODS-743.json', 'NATIVE_MODS-744.json']:
    descriptors += [(m['target'] + '_', m['metadataSection']) for m in json.loads((control / name).read_text())['mods']]
descriptors += [('port_pokemon_model_lab_', 'pkmmeta')]
assert len(descriptors) == 36
module.verify(a.package / 'TwilightPrincessRandomizer.exe', descriptors)
expected = json.loads((control / 'experimental/pokemon_battle/model_lab/mod.json').read_text())
folder = a.package / 'mods/port_pokemon_model_lab'
assert json.loads((folder / 'mod.json').read_text()) == expected, 'Preview bundle manifest mismatch'
assert (folder / 'res/model-pack.txt').is_file(), 'Preview resource directory missing'
assert not (folder / 'lib').exists(), 'Preview must use the signed built-in implementation'
proof = json.loads((a.package / 'mods/POKEMON-PREVIEW.json').read_text())
assert proof['id'] == expected['id'] and proof['version'] == expected['version']
assert proof['baseCommit'] == '8b1a9bc011032b32b5e123af8d76ba644b28a971'
print('PASS: all 36 independent signed implementations, .749 provenance and Pokemon resource bundle')
