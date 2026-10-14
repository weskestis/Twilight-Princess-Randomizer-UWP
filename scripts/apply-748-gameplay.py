"""Apply vague container hints and a direct native pickup guard to the complete .747 stack."""
import hashlib, json, os, subprocess, tempfile
from pathlib import Path
r=Path(os.environ['TPR_SRC']); c=Path(os.environ['TPR_CONTROL'])
identity=json.loads((c/'APP_IDENTITY-748.json').read_text())
assert 'Version="1.4.1.747"' in (r/'platforms/uwp/Package.appxmanifest').read_text()
names=['xbox-breakable-hints-748.patch','xbox-native-pickup-recovery-748.patch']
originals={}
try:
    with tempfile.TemporaryDirectory(prefix='gameplay-748-') as temp:
        patches=[]
        for name in names:
            data=(c/'patches'/name).read_bytes().replace(b'\r\n',b'\n')
            assert hashlib.sha256(data).hexdigest()==identity['sourcePatches'][name],name
            for line in data.decode().splitlines():
                if not line.startswith('+++ b/'): continue
                relative=Path(line[6:]); assert not relative.is_absolute() and '..' not in relative.parts
                path=r/relative
                if path not in originals:
                    originals[path]=path.read_bytes() if path.exists() else None
                    if originals[path] is not None: path.write_bytes(originals[path].replace(b'\r\n',b'\n'))
            patch=Path(temp)/name; patch.write_bytes(data); patches.append(str(patch))
        subprocess.run(['git','-C',str(r),'apply','--check',*patches],check=True)
        subprocess.run(['git','-C',str(r),'apply',*patches],check=True)
except Exception:
    for path,before in originals.items():
        if before is None: path.unlink(missing_ok=True)
        else: path.write_bytes(before)
    raise
print('Applied general-region pot/pumpkin hints and native missing/destroyed-message completion guard')
