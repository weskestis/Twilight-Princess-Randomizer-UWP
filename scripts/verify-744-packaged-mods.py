"""Verify every added source-built port resource and exact author provenance."""
import argparse
import hashlib
import json
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('package', type=Path)
p.add_argument('source', type=Path)
a = p.parse_args()
control = Path(__file__).resolve().parents[1]
inventory = json.loads((a.source / 'platforms/uwp/optional_mods/NATIVE-MODS-744.json').read_text())
assert inventory == json.loads((a.package / 'mods/NATIVE-MODS-744.json').read_text())
lock = json.loads((control / 'NATIVE_MODS-744.json').read_text())
assert len(inventory['mods']) == 3
for mod, pinned in zip(inventory['mods'], lock['mods']):
    for key in ['id', 'version', 'target', 'sourceCommit', 'sourceFiles', 'adaptations', 'download']:
        assert mod[key] == pinned[key], 'New source port provenance changed: ' + key
    folder = a.package / 'mods' / mod['target']
    expected = {f['path']: f for f in mod['resourceFiles']}
    actual = {f.relative_to(folder).as_posix() for f in folder.rglob('*') if f.is_file()}
    assert actual == expected.keys(), 'New native resource set changed: ' + mod['id']
    for name, entry in expected.items():
        file = folder / name
        assert file.stat().st_size == entry['size'] and hashlib.file_digest(file.open('rb'), 'sha256').hexdigest() == entry['sha256'], 'New native resource bytes changed'
    for original in mod['originalResourceFiles']:
        assert expected[original['path']] == original, 'Original download resource changed'
    assert not any(f.suffix.lower() in ['.dll', '.exe', '.so', '.dylib', '.lib'] for f in folder.rglob('*'))
    print('PASS: exact ' + mod['name'] + ' resources; signed source implementation ' + mod['sourceCommit'])
