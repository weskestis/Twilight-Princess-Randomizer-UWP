"""Verify that the signed package retained every pinned mod resource byte."""
import argparse
import json
import zipfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('package', type=Path)
parser.add_argument('cache', type=Path)
args = parser.parse_args()
control = Path(__file__).resolve().parents[1]
lock = json.loads((control/'BUNDLED_MODS-736.json').read_text())
mods = args.package/'mods'
actual_lock = json.loads((mods/'BUNDLED-MODS.json').read_text())
assert actual_lock == lock, 'Packaged compatibility inventory changed'
count = 0
for mod in lock['mods']:
    if not mod['bundle']:
        continue
    folder = 'controller_ui' if mod['id']=='org.dusklight.tp_classic_buttons' else mod['id']
    root = mods/folder
    manifest = json.loads((root/'mod.json').read_text())
    assert manifest['id']==mod['id'] and manifest['version']==mod['version']
    with zipfile.ZipFile(args.cache/(mod['id']+'_'+mod['version']+'.dusk')) as bundle:
        for entry in bundle.infolist():
            if entry.is_dir() or entry.filename.lower().endswith(('.dll','.so','.dylib')):
                continue
            path = root/entry.filename
            assert path.is_file() and path.read_bytes()==bundle.read(entry), str(path)
    count += 1
assert count == 22, 'Incomplete bundled catalog set'
for root in (mods/'controller_ui', mods/'luau_runtime'):
    assert (root/'mod.json').is_file()
    assert not any(path.suffix.lower() in ('.dll','.so','.dylib') for path in root.rglob('*'))
assert 'XandasLegend' in (mods/'controller_ui/res/LICENSE.txt').read_text()
assert (mods/'luau_runtime/res/licenses/LUAU-MIT.txt').is_file()
assert (mods/'luau_runtime/res/licenses/LUABRIDGE3-MIT.txt').is_file()
native = [str(path) for path in mods.rglob('*') if path.suffix.lower() in ('.dll','.so','.dylib')]
assert not native, 'Downloaded native binary appeared in the signed app: '+repr(native)
print('PASS .736 signed package: 22 byte-exact catalog resource sets, controller/Luau manifests and licenses; no downloaded native binaries')
