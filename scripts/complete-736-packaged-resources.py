"""Restore exact catalog paths before MakeAppx regenerates and signs the package.

The VS content pipeline can omit or rename literal resource filenames, including
Linkle's two palette textures containing '$'. Only pinned optional data is copied;
the compiled game, core mods, runtimes, manifest and PRI remain byte-identical.
"""
import argparse
import hashlib
import json
import stat
import zipfile
from pathlib import Path, PurePosixPath

parser = argparse.ArgumentParser()
parser.add_argument('package', type=Path)
parser.add_argument('cache', type=Path)
args = parser.parse_args()
control = Path(__file__).resolve().parents[1]
lock = json.loads((control/'BUNDLED_MODS-736.json').read_text())
root = args.package.resolve()
bundled = [mod for mod in lock['mods'] if mod['bundle']]
assert len(bundled) == 22
folders = {'controller_ui' if mod['id']=='org.dusklight.tp_classic_buttons' else mod['id']
           for mod in bundled}

def protected_files():
    result = {}
    for path in root.rglob('*'):
        if not path.is_file():
            continue
        relative = path.relative_to(root)
        if len(relative.parts)>1 and relative.parts[0]=='mods' and relative.parts[1] in folders:
            continue
        with path.open('rb') as stream:
            result[relative.as_posix()] = hashlib.file_digest(stream, 'sha256').hexdigest()
    return result

before = protected_files()
restored = 0
for mod in bundled:
    bundle = args.cache/(mod['id']+'_'+mod['version']+'.dusk')
    with bundle.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    if digest != mod['download']['sha256'].lower() or bundle.stat().st_size != mod['download']['size']:
        raise RuntimeError('Catalog payload changed: '+mod['id'])
    folder = 'controller_ui' if mod['id']=='org.dusklight.tp_classic_buttons' else mod['id']
    destination = root/'mods'/folder
    with zipfile.ZipFile(bundle) as archive:
        for entry in archive.infolist():
            name = entry.filename
            relative = PurePosixPath(name)
            if ('\\' in name or relative.is_absolute() or '..' in relative.parts or ':' in name
                    or '\0' in name or stat.S_ISLNK(entry.external_attr >> 16)):
                raise RuntimeError('Unsafe catalog resource path')
            if entry.is_dir() or name.lower().endswith(('.dll','.so','.dylib')):
                continue
            if name.lower().endswith('.exe'):
                raise RuntimeError('Executable appeared in a resource package')
            path = destination/relative
            payload = archive.read(entry)
            if not path.is_file() or path.read_bytes() != payload:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(payload)
                restored += 1
assert protected_files() == before, 'Compiled/core package payload was changed'
print(f'Completed 22 pinned resource sets; restored {restored} exact files. All core/package files unchanged.')
