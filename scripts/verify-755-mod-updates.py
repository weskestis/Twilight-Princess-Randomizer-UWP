"""Audit updated source, exact version gates, and every installed mod resource."""
import argparse
import hashlib
import json
import zipfile
from pathlib import Path

CONTROL = Path(__file__).resolve().parents[1]


def digest(file):
    with file.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def verify_files(folder, entries, exact=True):
    expected = {entry['path']: entry for entry in entries}
    if exact:
        actual = {file.relative_to(folder).as_posix() for file in folder.rglob('*') if file.is_file()}
        assert actual == expected.keys(), 'Resource file set differs: ' + str(folder)
    for name, entry in expected.items():
        file = folder / name
        assert file.stat().st_size == entry['size'] and digest(file) == entry['sha256'], 'Resource bytes differ: ' + str(file)


def verify_source(source):
    lock = json.loads((CONTROL / 'MOD_UPDATES-755.json').read_text())
    optional = source / 'platforms/uwp/optional_mods'
    proof = json.loads((optional / 'MOD-UPDATES-755.json').read_text())
    assert proof['blocked'] == lock['blocked']
    assert len(proof['mods']) == len(lock['mods']) == 11
    gates = (source / 'src/dusk/mods/uwp_ports.hpp').read_text()
    for pinned, actual in zip(lock['mods'], proof['mods']):
        assert all(actual[key] == value for key, value in pinned.items()), 'Update provenance changed'
        directory = source / pinned['directory']
        manifest = json.loads((directory / 'mod.json').read_text())
        assert (manifest['id'], manifest['version']) == (pinned['id'], pinned['version'])
        verify_files(directory, actual['resourceFiles'], exact=False)
        expected = {entry['path']: entry for entry in actual['resourceFiles']}
        assert all(expected[entry['path']] == entry for entry in actual['originalResourceFiles']), 'Original release resource modified'
        if not pinned['native']:
            continue
        adaptations = {entry['path']: entry for entry in pinned['adaptations']}
        for entry in pinned['sourceFiles']:
            name = entry['path']
            if name == 'mod.json':
                # Release manifests contain SDK-generated ABI/import records.
                # Their complete bytes are checked against the pinned .dusk above.
                continue
            if name == 'CMakeLists.txt' and not pinned.get('canonicalCmake'):
                continue
            if name in adaptations:
                continue
            assert digest(directory / name) == entry['sha256'], 'Author source modified: ' + name
        for name, entry in adaptations.items():
            assert digest(directory / name) == entry['sha256'], 'Xbox adaptation changed: ' + name
        if pinned['target'] == 'controller_ui':
            assert 'ControllerVersion = "' + pinned['version'] + '"' in gates
            assert 'ControllerVersion = "' + pinned['previousVersion'] + '"' not in gates
        else:
            assert '{"' + pinned['id'] + '", "' + pinned['version'] + '"}' in gates
            assert '{"' + pinned['id'] + '", "' + pinned['previousVersion'] + '"}' not in gates
        print('PASS: verified author source, Xbox port and version gate for ' + pinned['name'], flush=True)
    essentials = source / 'mods/uwp_ports/port_twilit_essentials/mod.json'
    assert json.loads(essentials.read_text())['version'] == '2.5.0', 'Unverified Essentials implementation substituted'
    print('PASS: latest update resources match their compiled source; Essentials provenance remains 2.5.0')
    return proof


def verify_package(package, source, proof):
    root = package / 'mods'
    optional = source / 'platforms/uwp/optional_mods'
    for name in ['MOD-UPDATES-755.json', 'BUNDLED-MODS.json', 'NATIVE-MODS-743.json', 'NATIVE-MODS-744.json', 'CUSTOM-MODS-743.json']:
        assert (root / name).read_bytes() == (optional / name).read_bytes(), 'Installed inventory changed: ' + name
    updates = {entry['id']: entry for entry in proof['mods']}
    count = 0
    for name, field, expected_count in [('NATIVE-MODS-743.json', 'resourceFiles', 28), ('NATIVE-MODS-744.json', 'resourceFiles', 3), ('CUSTOM-MODS-743.json', 'files', 35)]:
        inventory = json.loads((optional / name).read_text())['mods']
        assert len(inventory) == expected_count, 'Missing retained mods'
        for entry in inventory:
            directory = root / entry.get('target', entry['id'])
            verify_files(directory, entry[field])
            manifest = json.loads((directory / 'mod.json').read_text())
            assert manifest['id'] == entry['id']
            if 'target' in entry:
                assert manifest['version'] == entry['version']
            else:
                assert manifest.get('exclusive_group') == entry['exclusive_group']
            count += 1
    catalog = json.loads((optional / 'BUNDLED-MODS.json').read_text())
    bundled = [entry for entry in catalog['mods'] if entry['bundle']]
    assert len(bundled) == 22
    for entry in bundled:
        directory = root / ('controller_ui' if entry['id'] == 'org.dusklight.tp_classic_buttons' else entry['id'])
        if entry['id'] in updates:
            verify_files(directory, updates[entry['id']]['resourceFiles'])
        else:
            archive = source / '.uwp-mod-downloads-736' / (entry['id'] + '_' + entry['version'] + '.dusk')
            assert digest(archive) == entry['download']['sha256']
            with zipfile.ZipFile(archive) as bundle:
                for member in bundle.infolist():
                    if member.is_dir() or member.filename.startswith('lib/'):
                        continue
                    assert (directory / member.filename).read_bytes() == bundle.read(member), 'Retained catalog bytes changed'
        manifest = json.loads((directory / 'mod.json').read_text())
        assert (manifest['id'], manifest['version']) == (entry['id'], entry['version'])
        count += 1
    assert 'XandasLegend' in (root / 'controller_ui/res/LICENSE.txt').read_text()
    for name in ['LUAU-MIT.txt', 'LUABRIDGE3-MIT.txt']:
        assert (root / 'luau_runtime/res/licenses' / name).is_file()
    assert not [file for file in root.rglob('*') if file.suffix.lower() in ['.dll', '.exe', '.so', '.dylib', '.pdb', '.lib']], 'Downloaded native binary packaged'
    print('PASS: 88 retained/updated mod resource sets, original licenses, exact inventories; no downloaded native modules')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('--package', type=Path)
    args = parser.parse_args()
    proof = verify_source(args.source)
    if args.package:
        verify_package(args.package, args.source, proof)
