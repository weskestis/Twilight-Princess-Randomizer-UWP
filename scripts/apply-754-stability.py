"""Merge pinned Dusklight 2.0.3 into the verified Xbox tree, then install reviewed ports.

The previous recipe remains a reproducible bootstrap. Save/startup and conflicting
ports now have one maintained source per file under ports/stability-754, with
exact before/after hashes. Unreviewed upstream conflicts stop before any writes.
"""
import hashlib, json, os, subprocess, tempfile
from pathlib import Path

root = Path(os.environ['TPR_SRC']).resolve()
control = Path(os.environ['TPR_CONTROL']).resolve()
lock = json.loads((control / 'UPSTREAM-754.json').read_text())
git_executable = os.environ.get('TPR_GIT', 'git')

def git(repo, *args):
    result = subprocess.run([git_executable, '-C', str(repo), *args], capture_output=True)
    if result.returncode:
        raise RuntimeError('git ' + ' '.join(args[:3]) + ': ' + result.stderr.decode(errors='replace'))
    return result.stdout

def normalized(data): return data.replace(b'\r\n', b'\n')
def digest(data): return hashlib.sha256(normalized(data)).hexdigest()
def safe_path(base, relative):
    relative = Path(relative)
    if relative.is_absolute() or '..' in relative.parts: raise RuntimeError('Unsafe source path')
    return base / relative

manifest = root / 'platforms/uwp/Package.appxmanifest'
assert 'Version="1.4.1.749"' in manifest.read_text(), 'Stability integration requires verified .749 bootstrap'
overrides = lock['reviewedPorts']
prepared = {}
originals = {}
for name, entry in overrides.items():
    target = safe_path(root, name)
    old = normalized(target.read_bytes()) if target.exists() else None
    if (digest(old) if old is not None else None) != entry['beforeSha256']:
        raise RuntimeError('Reviewed .749 source changed before stability merge: ' + name)
    new = safe_path(control / 'ports/stability-754', name).read_bytes()
    if digest(new) != entry['sha256']: raise RuntimeError('Reviewed stability source hash mismatch: ' + name)
    prepared[target] = normalized(new)
    originals[target] = old

for entry in lock['repositories']:
    repo = safe_path(root, entry['path'])
    if git(repo, 'rev-parse', 'HEAD').decode().strip() != entry['base']:
        raise RuntimeError('Unexpected bootstrap revision: ' + entry['path'])
    available = subprocess.run([git_executable, '-C', str(repo), 'cat-file', '-e', entry['target'] + '^{commit}'], capture_output=True)
    if available.returncode:
        git(repo, 'fetch', '--depth', '1', 'origin', entry['target'])
    diff = git(repo, 'diff', '--binary', entry['base'], entry['target'])
    if hashlib.sha256(diff).hexdigest() != entry['deltaSha256']:
        raise RuntimeError('Upstream delta changed: ' + entry['path'])
    for change in entry['files']:
        name = change['path']
        relative = (Path(entry['path']) / name).as_posix()
        if relative in overrides: continue
        target = safe_path(repo, name)
        before = git(repo, 'show', entry['base'] + ':' + name) if change['status'] != 'A' else b''
        after = git(repo, 'show', entry['target'] + ':' + name) if change['status'] != 'D' else b''
        before, after = normalized(before), normalized(after)
        ours = normalized(target.read_bytes()) if target.exists() else b''
        originals[target] = ours if target.exists() else None
        if ours == before:
            merged = after
        elif ours == after:
            merged = ours
        else:
            with tempfile.TemporaryDirectory(prefix='merge-754-') as temp:
                paths = [Path(temp) / n for n in ['xbox', 'base', 'upstream']]
                for path, data in zip(paths, [ours, before, after]): path.write_bytes(data)
                result = subprocess.run([git_executable, 'merge-file', '-p', '--diff3', *map(str, paths)], capture_output=True)
                if result.returncode:
                    print(result.stdout.decode(errors='replace'))
                    raise RuntimeError('Upstream conflict needs review: ' + relative)
                merged = result.stdout
        prepared[target] = None if change['status'] == 'D' and not merged else merged

try:
    for path, data in prepared.items():
        if data is None: path.unlink(missing_ok=True)
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
except Exception:
    for path, old in originals.items():
        if old is None: path.unlink(missing_ok=True)
        else: path.write_bytes(old)
    raise

# The working files are the reviewed merge. Advance provenance without replacing
# them, and retain the explicitly pinned custom Randomizer submodule revision.
preserved = {}
for entry in lock['repositories']:
    repo = safe_path(root, entry['path'])
    for name in git(repo, 'ls-files', '-z', '--cached', '--others', '--exclude-standard').split(b'\0'):
        if not name: continue
        path = safe_path(repo, os.fsdecode(name))
        if path.is_file(): preserved[path] = hashlib.sha256(path.read_bytes()).hexdigest()
for entry in reversed(lock['repositories']):
    git(safe_path(root, entry['path']), '-c', 'submodule.recurse=false', 'reset', '--mixed', entry['target'])
for path, expected in preserved.items():
    if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
        raise RuntimeError('Source changed while recording upstream provenance: ' + str(path.relative_to(root)))

report = {
    'bootstrap': '1.4.1.749', 'upstream': '2.0.3',
    'repositories': [{k: v for k, v in e.items() if k != 'files'} for e in lock['repositories']],
    'customRandomizer': lock['customRandomizer'],
    'preservedFilesVerified': len(preserved),
    'changedFiles': {str(p.relative_to(root)).replace('\\', '/'): digest(b) if b is not None else None
                     for p, b in prepared.items()},
}
(root / 'SOURCE-754-INTEGRATION.json').write_text(json.dumps(report, indent=2) + '\n')
print('PASS: integrated pinned Dusklight 2.0.3, Aurora and Borealis; ' + str(len(prepared)) + ' reviewed/merged files')
