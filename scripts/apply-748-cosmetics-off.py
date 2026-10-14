"""Apply explicit Off/Vanilla and manual-color controls after the complete .747 source."""
import hashlib, json, os, subprocess, tempfile
from pathlib import Path

r = Path(os.environ['TPR_SRC'])
c = Path(os.environ['TPR_CONTROL'])
identity = json.loads((c / 'APP_IDENTITY-748.json').read_text())
assert 'Version="1.4.1.747"' in (r / 'platforms/uwp/Package.appxmanifest').read_text(), '.748 requires complete .747'
name = 'xbox-cosmetics-off-748.patch'
data = (c / 'patches' / name).read_bytes().replace(b'\r\n', b'\n')
assert hashlib.sha256(data).hexdigest() == identity['sourcePatches'][name], '.748 cosmetics patch hash mismatch'
originals = {}
try:
    for line in data.decode().splitlines():
        if not line.startswith('+++ b/'):
            continue
        relative = Path(line[6:])
        assert not relative.is_absolute() and '..' not in relative.parts
        path = r / relative
        originals[path] = path.read_bytes() if path.exists() else None
        if originals[path] is not None:
            path.write_bytes(originals[path].replace(b'\r\n', b'\n'))
    with tempfile.TemporaryDirectory(prefix='cosmetics-off-748-') as temporary:
        patch = Path(temporary) / name
        patch.write_bytes(data)
        subprocess.run(['git', '-C', str(r), 'apply', '--check', str(patch)], check=True)
        subprocess.run(['git', '-C', str(r), 'apply', str(patch)], check=True)
except Exception:
    for path, before in originals.items():
        if before is None:
            path.unlink(missing_ok=True)
        else:
            path.write_bytes(before)
    raise
print('Applied explicit Off/Vanilla, persisted vanilla override, and manual-color edits that disable global randomization')
