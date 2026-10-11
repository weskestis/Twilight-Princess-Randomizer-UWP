"""Compare the production inventory with the host's actual CMake asset-copy policy."""
import argparse
import ast
import copy
import hashlib
import json
import shutil
import subprocess
import tempfile
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('source',type=Path)
p.add_argument('--negative-control',action='store_true')
a=p.parse_args()
control=Path(__file__).resolve().parents[1]
stager=control/'scripts/stage-743-native-mods.py'
tree=ast.parse(stager.read_text())
function=next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name=='packaged_resources')
function=copy.deepcopy(function)
if a.negative_control:
    guards=[n for n in ast.walk(function) if isinstance(n,ast.If) and isinstance(n.test,ast.Call)
        and isinstance(n.test.func,ast.Name) and n.test.func.id=='any']
    assert len(guards)==1,'Production dotfile filter changed'
    guards[0].test=ast.Constant(False)
module=ast.fix_missing_locations(ast.Module(body=[function],type_ignores=[]))
scope={'hashlib':hashlib}
exec(compile(module,str(stager),'exec'),scope)
inventory=scope['packaged_resources']
lock=json.loads((control/'NATIVE_MODS-743.json').read_text())
essentials=next(m for m in lock['mods'] if m['target']=='port_twilit_essentials')
copy_script=a.source.resolve()/'cmake/CopyModAssets.cmake'
assert copy_script.is_file(),'Actual host asset-copy script missing'
cmake=shutil.which('cmake')
assert cmake,'CMake unavailable'
with tempfile.TemporaryDirectory(prefix='native-resources-743-') as temp:
    work=Path(temp);source=work/'source';package=work/'package'
    # Use the actual pinned Essentials resource paths, including its three .gitkeep files.
    names=['mod.json',*essentials['sourceResourceFiles'],
        'res/nested/.gitkeep','res/.private/config.json','res/ordinary.name.bin',
        'textures/.cache/image.bin','textures/ordinary.dds','overlay/ordinary.arc',
        'src/source.cpp','CMakeLists.txt']
    for name in names:
        file=source/name;file.parent.mkdir(parents=True,exist_ok=True)
        file.write_bytes(('fixture for '+name+'\n').encode())
    before={f.relative_to(source).as_posix():f.read_bytes() for f in source.rglob('*') if f.is_file()}
    for directory in ('res','overlay','textures'):
        subprocess.run([cmake,'-DSRC='+str(source/directory),'-DDST='+str(package/directory),
            '-P',str(copy_script)],check=True)
    shutil.copyfile(source/'mod.json',package/'mod.json')
    records=inventory(source)
    expected={r['path'] for r in records}
    actual={f.relative_to(package).as_posix() for f in package.rglob('*') if f.is_file()}
    assert before=={f.relative_to(source).as_posix():f.read_bytes() for f in source.rglob('*') if f.is_file()},'Source audit inputs changed'
    if a.negative_control:
        omitted=expected-actual
        assert {'overlay/.gitkeep','res/.gitkeep','res/textures/.gitkeep'}<=omitted,'Original Essentials mismatch was not reproduced'
        assert not actual-expected,'Negative control lost ordinary assets'
        print('PASS: original inventory includes '+str(len(omitted))+' dotfiles that the actual CMake packager omits')
    else:
        assert expected==actual,'Production inventory differs from actual copied assets: '+repr(sorted(expected^actual))
        for record in records:
            file=package/record['path']
            assert file.stat().st_size==record['size']
            with file.open('rb') as stream:
                assert hashlib.file_digest(stream,'sha256').hexdigest()==record['sha256'],'Copied asset bytes changed'
        assert 'res/ordinary.name.bin' in actual and 'textures/ordinary.dds' in actual and 'overlay/ordinary.arc' in actual
        assert not any(any(part.startswith('.') for part in Path(name).parts) for name in actual)
        print('PASS: production native inventory matches '+str(len(records))+' actual CMake-packaged files and hashes; pinned source placeholders retained')
