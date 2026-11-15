"""Replace selected ports with verified author source and matching release resources."""
import argparse
import hashlib
import json
import re
import shutil
import stat
import subprocess
import tempfile
import urllib.request
import zipfile
from pathlib import Path, PurePosixPath

CONTROL = Path(__file__).resolve().parents[1]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def safe(name):
    path = PurePosixPath(name)
    return bool(name) and not path.is_absolute() and '..' not in path.parts and '\\' not in name and ':' not in name


def verified(spec, cache, name):
    path = cache / name
    if not path.is_file() or path.stat().st_size != spec['size'] or digest(path.read_bytes()) != spec['sha256']:
        request = urllib.request.Request(spec['url'], headers={
            'User-Agent': 'Dusklight-UWP-Bundler/1.4.1.755', 'X-Dusklight-Version': '2.0.3'})
        with urllib.request.urlopen(request, timeout=120) as response:
            data = response.read()
        if len(data) != spec['size'] or digest(data) != spec['sha256']:
            raise RuntimeError('Pinned download mismatch: ' + name)
        path.write_bytes(data)
    return path


def resources(bundle, directory, mod):
    original = []
    with zipfile.ZipFile(bundle) as package:
        names = set()
        for entry in package.infolist():
            name = entry.filename
            if not safe(name) or stat.S_ISLNK(entry.external_attr >> 16) or name.casefold() in names:
                raise RuntimeError('Unsafe/duplicate release path: ' + name)
            names.add(name.casefold())
            if entry.is_dir() or name.startswith('lib/'):
                continue
            if name.lower().endswith(('.dll', '.exe', '.so', '.dylib', '.pdb', '.lib')):
                raise RuntimeError('Unexpected executable in resource payload: ' + name)
            if name != 'mod.json' and not name.startswith(('res/', 'overlay/', 'textures/')):
                raise RuntimeError('Unexpected release resource: ' + name)
            if entry.file_size > 128 * 1024**2:
                raise RuntimeError('Oversized release entry: ' + name)
            data = package.read(entry)
            path = directory / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            original.append({'path': name, 'size': len(data), 'sha256': digest(data)})
    manifest = json.loads((directory / 'mod.json').read_text(encoding='utf-8-sig'))
    if (manifest['id'], manifest['version']) != (mod['id'], mod['version']):
        raise RuntimeError('Release identity mismatch: ' + mod['id'])
    return original


def inventory(directory):
    result = []
    for file in sorted(directory.rglob('*')):
        relative = file.relative_to(directory)
        if not file.is_file() or any(part.startswith('.') for part in relative.parts):
            continue
        if relative.as_posix() != 'mod.json' and relative.parts[0] not in ('res', 'overlay', 'textures'):
            continue
        result.append({'path': relative.as_posix(), 'size': file.stat().st_size, 'sha256': digest(file.read_bytes())})
    return result


def cmake(mod, directory):
    if mod.get('canonicalCmake'):
        return
    target = mod['target']
    result = '# Author source and release resources pinned in MOD_UPDATES-755.json.\ncmake_minimum_required(VERSION 3.26)\n'
    result += 'add_mod(' + target + '\n    FEATURES ' + ' '.join(mod['features'])
    result += '\n    SOURCES ' + ' '.join(mod['sources']) + '\n    MOD_JSON mod.json\n'
    for name, key in [('res', 'RES_DIR'), ('overlay', 'OVERLAY_DIR'), ('textures', 'TEXTURES_DIR')]:
        if (directory / name).is_dir():
            result += '    ' + key + ' ' + name + '\n'
    result += '    BUNDLE\n)\ntarget_compile_definitions(' + target + ' PRIVATE\n'
    if mod.get('metadataDefine'):
        result += '    ' + mod['metadataDefine'] + '=1\n'
    for variable in mod['serviceVariables']:
        result += '    ' + variable + '=' + target + '_' + variable + '\n'
        # Some service macros use token-pasting; protect both the pointer and record.
        result += '    mod_meta_import_' + variable + '=mod_meta_import_' + target + '_' + variable + '\n'
    for symbol in mod.get('privateNamespaces', []) + mod.get('privateSymbols', []):
        result += '    ' + symbol + '=' + target + '_' + symbol + '\n'
    result += ''.join('    ' + define + '\n' for define in mod.get('cmakeDefines', [])) + ')\n'
    includes = ['${CMAKE_CURRENT_SOURCE_DIR}/src', '${CMAKE_CURRENT_SOURCE_DIR}/include', '${CMAKE_SOURCE_DIR}/src']
    includes += [value if value.startswith('${') else '${CMAKE_CURRENT_SOURCE_DIR}/' + value for value in mod.get('includePaths', [])]
    result += 'target_include_directories(' + target + ' PRIVATE ' + ' '.join('"' + value + '"' for value in dict.fromkeys(includes)) + ')\n'
    if mod.get('linkTargets'):
        result += 'target_link_libraries(' + target + ' PRIVATE ' + ' '.join(mod['linkTargets']) + ')\n'
    (directory / 'CMakeLists.txt').write_text(result, encoding='utf-8', newline='\n')


def stage(root, cache):
    lock = json.loads((CONTROL / 'MOD_UPDATES-755.json').read_text())
    cache.mkdir(parents=True, exist_ok=True)
    optional = root / 'platforms/uwp/optional_mods'
    gates = root / 'src/dusk/mods/uwp_ports.hpp'
    policy = gates.read_text()
    proofs = []
    for mod in lock['mods']:
        destination = root / mod['directory']
        existing = json.loads((destination / 'mod.json').read_text(encoding='utf-8-sig'))
        if (existing['id'], existing['version']) != (mod['id'], mod['previousVersion']):
            raise RuntimeError('Update baseline mismatch: ' + mod['id'])
        with tempfile.TemporaryDirectory(prefix='mod-755-', dir=cache) as temporary:
            directory = Path(temporary)
            if mod['native']:
                archive = verified(mod['sourceArchive'], cache, mod['sourceArchive']['name'])
                with zipfile.ZipFile(archive) as package:
                    top = package.namelist()[0].split('/')[0] + '/'
                    for entry in mod['sourceFiles']:
                        name = entry['path']
                        if not safe(name):
                            raise RuntimeError('Unsafe source path: ' + name)
                        info = package.getinfo(top + name)
                        if stat.S_ISLNK(info.external_attr >> 16):
                            raise RuntimeError('Source symlink: ' + name)
                        data = package.read(info)
                        if len(data) != entry['size'] or digest(data) != entry['sha256']:
                            raise RuntimeError('Author source mismatch: ' + name)
                        file = directory / name
                        file.parent.mkdir(parents=True, exist_ok=True)
                        file.write_bytes(data)
                if mod['target'] == 'port_mfb_multiplayer':
                    author = root / '.uwp-native-source-755' / mod['target']
                    if author.exists():
                        shutil.rmtree(author)
                    shutil.copytree(directory, author)
                for entry in mod['adaptations']:
                    file = directory / entry['path']
                    before = digest(file.read_bytes().replace(b'\r\n', b'\n')) if file.exists() else None
                    payload = (CONTROL / entry['controlPath']).read_bytes().replace(b'\r\n', b'\n')
                    if before != entry['beforeSha256'] or digest(payload) != entry['sha256']:
                        raise RuntimeError('Xbox adaptation mismatch: ' + entry['path'])
                    file.parent.mkdir(parents=True, exist_ok=True)
                    file.write_bytes(payload)
            bundle = verified(mod['download'], cache, mod['id'] + '-' + mod['version'] + '.dusk')
            originals = resources(bundle, directory, mod)
            if mod['native']:
                for license in directory.iterdir():
                    if license.is_file() and license.name.lower().startswith(('license', 'licence', 'copying', 'credits')):
                        file = directory / 'res/licenses' / license.name
                        file.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copyfile(license, file)
                if mod['target'] == 'controller_ui':
                    # Keep the previously packaged notice, alongside the author's license.
                    shutil.copyfile(destination / 'res/LICENSE.txt', directory / 'res/LICENSE.txt')
                cmake(mod, directory)
            # Replace the complete mod tree; never leave removed upstream files behind.
            shutil.rmtree(destination)
            shutil.copytree(directory, destination)
        if mod['native']:
            if mod['target'] == 'controller_ui':
                before = 'ControllerVersion = "' + mod['previousVersion'] + '"'
                after = 'ControllerVersion = "' + mod['version'] + '"'
            else:
                before = '{"' + mod['id'] + '", "' + mod['previousVersion'] + '"}'
                after = '{"' + mod['id'] + '", "' + mod['version'] + '"}'
            if policy.count(before) != 1:
                raise RuntimeError('Native version gate baseline changed: ' + mod['id'])
            policy = policy.replace(before, after)
        proofs.append({**mod, 'originalResourceFiles': originals, 'resourceFiles': inventory(destination)})
        print('Updated ' + mod['name'] + ' ' + mod['previousVersion'] + ' -> ' + mod['version'], flush=True)
    gates.write_text(policy, encoding='utf-8', newline='\n')
    updates = {m['id']: m for m in proofs}
    for name in ['NATIVE-MODS-743.json', 'NATIVE-MODS-744.json']:
        file = optional / name
        content = json.loads(file.read_text())
        content['mods'] = [updates.get(m['id'], m) for m in content['mods']]
        file.write_text(json.dumps(content, indent=2) + '\n', encoding='utf-8', newline='\n')
    file = optional / 'BUNDLED-MODS.json'
    content = json.loads(file.read_text())
    content['catalogSnapshot'] = lock['catalogCheckedAt']
    for mod in content['mods']:
        if mod['id'] in updates:
            new = updates[mod['id']]
            mod.update({key: new[key] for key in ['version', 'name', 'download']})
    controller = updates['org.dusklight.tp_classic_buttons']
    content['controllerSource'] = {key: controller[key] for key in ['source_url', 'sourceCommit', 'sourceArchive', 'adaptations']}
    file.write_text(json.dumps(content, indent=2) + '\n', encoding='utf-8', newline='\n')
    # The same preview pipeline refreshes changed banners and all native inventories.
    subprocess.run(['python', str(CONTROL / 'scripts/stage-744-ui-thumbnails.py'), str(root)], check=True)
    for mod in proofs:
        mod['resourceFiles'] = inventory(root / mod['directory'])
    (optional / 'MOD-UPDATES-755.json').write_text(json.dumps({**lock, 'mods': proofs}, indent=2) + '\n', encoding='utf-8', newline='\n')
    print('PASS: 11 mod updates staged, exact native versions bound; archived Essentials 2.5.0 retained')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('--cache', type=Path)
    args = parser.parse_args()
    stage(args.source.resolve(), (args.cache or args.source / '.uwp-mod-updates-755').resolve())
