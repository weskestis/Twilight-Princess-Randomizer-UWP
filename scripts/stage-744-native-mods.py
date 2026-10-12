"""Stage exact author source and matching resources for the three new Xbox ports."""
import argparse
import hashlib
import json
import shutil
import stat
import subprocess
import tempfile
import urllib.request
import zipfile
from pathlib import Path, PurePosixPath

p = argparse.ArgumentParser()
p.add_argument('source', type=Path)
p.add_argument('--cache', type=Path)
p.add_argument('--source-cache', type=Path)
a = p.parse_args()
root = a.source.resolve()
control = Path(__file__).resolve().parents[1]
lock = json.loads((control / 'NATIVE_MODS-744.json').read_text())
cache = (a.cache or root / '.uwp-native-downloads-744').resolve()
source_cache = (a.source_cache or root / '.uwp-native-source-744').resolve()
cache.mkdir(parents=True, exist_ok=True)
source_cache.mkdir(parents=True, exist_ok=True)

def digest(data):
    return hashlib.sha256(data).hexdigest()

def safe(name):
    path = PurePosixPath(name)
    return not path.is_absolute() and '..' not in path.parts and '\\' not in name and ':' not in name

proof = []
parent = root / 'mods/uwp_ports'
for mod in lock['mods']:
    repo = source_cache / mod['target']
    if not (repo / '.git').exists():
        repo.mkdir(parents=True, exist_ok=True)
        subprocess.run(['git', 'init', str(repo)], check=True, stdout=subprocess.DEVNULL)
        subprocess.run(['git', '-C', str(repo), 'remote', 'add', 'origin', mod['source_url'] + '.git'], check=True)
    head = subprocess.run(['git', '-C', str(repo), 'rev-parse', 'HEAD'], capture_output=True, text=True)
    if head.returncode or head.stdout.strip() != mod['sourceCommit']:
        subprocess.run(['git', '-C', str(repo), 'fetch', '--depth', '1', 'origin', mod['sourceCommit']], check=True)
        subprocess.run(['git', '-C', str(repo), '-c', 'core.autocrlf=false', 'checkout', '--detach', 'FETCH_HEAD'], check=True)
    assert subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip() == mod['sourceCommit']
    directory = parent / mod['target']
    directory.mkdir(parents=True, exist_ok=True)
    for entry in mod['sourceFiles']:
        name = entry['path']
        assert safe(name), 'Unsafe source path'
        source = repo / name
        assert not source.is_symlink(), 'Source symlinks are not packaged'
        data = source.read_bytes()
        assert len(data) == entry['size'] and digest(data) == entry['sha256'], 'Author source mismatch: ' + name
        file = directory / name
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_bytes(data)
    bundle = cache / (mod['id'] + '.dusk')
    spec = mod['download']
    if not bundle.exists() or bundle.stat().st_size != spec['size'] or digest(bundle.read_bytes()) != spec['sha256']:
        request = urllib.request.Request(spec['url'], headers={'User-Agent': 'Dusklight-UWP-Bundler/1.4.1.744', 'X-Dusklight-Version': '2.0.0-alpha2'})
        with urllib.request.urlopen(request, timeout=90) as response:
            data = response.read()
        assert len(data) == spec['size'] and digest(data) == spec['sha256'], 'Mod download mismatch'
        bundle.write_bytes(data)
    original_resources = []
    with zipfile.ZipFile(bundle) as package:
        manifest = json.loads(package.read('mod.json'))
        assert manifest['id'] == mod['id'] and manifest['version'] == mod['version']
        names = set()
        for info in package.infolist():
            name = info.filename
            assert safe(name) and not stat.S_ISLNK(info.external_attr >> 16), 'Unsafe bundle entry'
            assert name.casefold() not in names, 'Duplicate bundle entry'
            names.add(name.casefold())
            if info.is_dir() or name.startswith('lib/'):
                continue
            assert not name.lower().endswith(('.dll', '.exe', '.so', '.dylib', '.pdb')), 'Native code outside lib/'
            assert name == 'mod.json' or name.startswith(('res/', 'overlay/', 'textures/')), 'Unexpected resource'
            assert info.file_size <= 128 * 1024**2, 'Oversized resource'
            data = package.read(info)
            file = directory / name
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_bytes(data)
            original_resources.append({'path': name, 'size': len(data), 'sha256': digest(data)})
    # Every platform adaptation is hash-pinned separately from the author inputs.
    for entry in mod['adaptations']:
        path = directory / entry['path']
        if path.exists():
            before = path.read_bytes().replace(b'\r\n', b'\n')
            assert digest(before) == entry['beforeSha256'], 'Native adaptation input changed: ' + entry['path']
        else:
            assert entry['beforeSha256'] is None
        payload = (control / entry['controlPath']).read_bytes().replace(b'\r\n', b'\n')
        assert digest(payload) == entry['sha256'], 'Native adaptation control changed'
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(payload)
    for license in directory.iterdir():
        if license.is_file() and license.name.lower().startswith(('license', 'copying', 'credits')):
            destination = directory / 'res/licenses' / license.name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(license, destination)
    resource_files = []
    for file in sorted(directory.rglob('*')):
        if not file.is_file():
            continue
        relative = file.relative_to(directory)
        if relative.as_posix() != 'mod.json' and relative.parts[0] not in ('res', 'overlay', 'textures'):
            continue
        if any(part.startswith('.') for part in relative.parts):
            continue
        data = file.read_bytes()
        resource_files.append({'path': relative.as_posix(), 'size': len(data), 'sha256': digest(data)})
    proof.append({**mod, 'originalResourceFiles': original_resources, 'resourceFiles': resource_files})
    print('Staged exact ' + mod['name'] + ' ' + mod['version'] + ' source and resources', flush=True)

common = (control / 'native_ports/AdditionalPorts744.cmake').read_bytes().replace(b'\r\n', b'\n')
assert digest(common) == lock['commonCmakeSha256'], 'Shared native dependencies changed'
(parent / 'AdditionalPorts744.cmake').write_bytes(common)
cmake = (parent / 'CMakeLists.txt').read_text()
assert 'port_crests_of_courage' not in cmake, 'Additional ports already staged'
cmake += '\ninclude(AdditionalPorts744.cmake)\n' + ''.join('add_subdirectory(' + mod['target'] + ')\n' for mod in lock['mods'])
(parent / 'CMakeLists.txt').write_text(cmake, encoding='utf-8', newline='\n')
ports = root / 'platforms/uwp/NativePorts743.cmake'
text = ports.read_text()
text += 'list(APPEND DUSK_UWP_ADDITIONAL_PORTS ' + ' '.join(mod['target'] for mod in lock['mods']) + ')\n'
ports.write_text(text, encoding='utf-8', newline='\n')
(root / 'platforms/uwp/optional_mods/NATIVE-MODS-744.json').write_text(json.dumps({'mods': proof, 'blocked': lock['blocked']}, indent=2) + '\n')
print('PASS: three new pinned source ports staged; downloaded desktop libraries excluded')
