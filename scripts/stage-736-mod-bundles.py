"""Stage hash-pinned optional resources and the controller's source for signed UWP ports."""
import argparse
import hashlib
import json
import re
import stat
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path, PurePosixPath

parser = argparse.ArgumentParser()
parser.add_argument('source', type=Path)
parser.add_argument('--cache', type=Path)
args = parser.parse_args()
control = Path(__file__).resolve().parents[1]
lock = json.loads((control / 'BUNDLED_MODS-736.json').read_text())
source = args.source
cache = args.cache or source / '.uwp-mod-downloads-736'
cache.mkdir(parents=True, exist_ok=True)
optional = source / 'platforms/uwp/optional_mods'
optional.mkdir(parents=True, exist_ok=True)
controller = source / 'mods/controller_ui'
headers = {'User-Agent': 'Dusklight-UWP-Bundler/1.4.1.736',
           'X-Dusklight-Version': '2.0.0-alpha2'}


def verified_file(url, target, expected_hash, expected_size=None):
    if not re.fullmatch(r'[a-f0-9]{64}', expected_hash):
        raise ValueError('Invalid pinned SHA256')
    if target.is_file() and (expected_size is None or target.stat().st_size == expected_size):
        with target.open('rb') as stream:
            if hashlib.file_digest(stream, 'sha256').hexdigest() == expected_hash:
                return
    target.parent.mkdir(parents=True, exist_ok=True)
    partial = target.with_suffix('.partial')
    digest = hashlib.sha256()
    length = 0
    limit = expected_size if expected_size is not None else 1024 * 1024
    with urllib.request.urlopen(urllib.request.Request(url, headers=headers), timeout=60) as response:
        with partial.open('wb') as output:
            while block := response.read(1024 * 1024):
                length += len(block)
                if length > limit:
                    raise ValueError('Download exceeds pinned size')
                digest.update(block)
                output.write(block)
    if digest.hexdigest() != expected_hash or (expected_size is not None and length != expected_size):
        raise ValueError('Pinned download does not match its SHA256 or size')
    partial.replace(target)


for mod in lock['mods']:
    if not mod['bundle']:
        continue
    identifier, version = mod['id'], mod['version']
    if not re.fullmatch(r'[a-z0-9_]+(?:\.[a-z0-9_]+)+', identifier):
        raise ValueError('Unsafe package identity')
    if not re.fullmatch(r'[A-Za-z0-9.+_-]+', version):
        raise ValueError('Unsafe package version')
    archive = cache / (identifier + '_' + version + '.dusk')
    download = mod['download']
    url = urllib.parse.urljoin('https://twilitrealm.dev', download['url'])
    parsed = urllib.parse.urlparse(url)
    if parsed.scheme != 'https' or parsed.hostname != 'twilitrealm.dev':
        raise ValueError('Unexpected package download origin')
    verified_file(url, archive, download['sha256'].lower(), download['size'])
    destination = controller if identifier == 'org.dusklight.tp_classic_buttons' else optional / identifier
    with zipfile.ZipFile(archive) as package:
        entries = package.infolist()
        files = [entry for entry in entries if not entry.is_dir()]
        names = [entry.filename for entry in files]
        if len(names) != len({name.casefold() for name in names}):
            raise ValueError('Duplicate or case-colliding package paths')
        manifest = json.loads(package.read('mod.json'))
        if manifest.get('id') != identifier or manifest.get('version') != version:
            raise ValueError('Pinned package manifest mismatch')
        native = [name for name in names if name.lower().endswith(('.dll', '.so', '.dylib'))]
        is_controller = identifier == 'org.dusklight.tp_classic_buttons'
        if native and not is_controller:
            raise ValueError('Optional resource package unexpectedly contains native code')
        runtime = manifest.get('runtime')
        if runtime is not None and runtime != 'dev.twilitrealm.luau@1.0':
            raise ValueError('Unported script runtime')
        expanded = 0
        for entry in entries:
            name = entry.filename
            path = PurePosixPath(name)
            if ('\\' in name or path.is_absolute() or '..' in path.parts or ':' in name
                    or '\x00' in name or stat.S_ISLNK(entry.external_attr >> 16)):
                raise ValueError('Unsafe resource entry')
            expanded += entry.file_size
            if entry.file_size > 512 * 1024 * 1024 or expanded > 2 * 1024 ** 3:
                raise ValueError('Resource expansion exceeds limit')
            if entry.is_dir() or name in native:
                continue
            # Only this exact known version has a signed native implementation.
            # Its DLLs are omitted; the author's unchanged textures remain intact.
            target = destination / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(package.read(entry))
    print(f'Staged optional {identifier} {version}', flush=True)

pin = lock['controllerSource']
url = f"https://raw.githubusercontent.com/{pin['repository']}/{pin['commit']}/{pin['path']}"
verified_file(url, controller / 'src/mod.cpp', pin['sha256'])
(optional / 'BUNDLED-MODS.json').write_text(json.dumps(lock, indent=2) + '\n', encoding='utf-8')
bundled = sum(mod['bundle'] for mod in lock['mods'])
if bundled != lock['bundledCatalogMods'] or bundled != 22:
    raise ValueError('Incomplete pinned optional mod set')
print('Staged 18 asset mods, 3 Luau script mods and controller UI 1.3.2; existing Randomizer data preserved.')
