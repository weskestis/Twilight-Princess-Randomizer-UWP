"""Apply the navigation fix after the complete .745 restoration layer."""
import hashlib,json,os,subprocess,tempfile
from pathlib import Path
r=Path(os.environ['TPR_SRC']);c=Path(os.environ['TPR_CONTROL']);identity=json.loads((c/'APP_IDENTITY-746.json').read_text())
if 'Version="1.4.1.745"' not in (r/'platforms/uwp/Package.appxmanifest').read_text():raise RuntimeError('.746 requires complete .745 source first')
name='xbox-menu-navigation-746.patch';data=(c/'patches'/name).read_bytes().replace(b'\r\n',b'\n')
if hashlib.sha256(data).hexdigest()!=identity['sourcePatches'][name]:raise RuntimeError('.746 navigation patch hash mismatch')
originals={}
try:
 for line in data.decode().splitlines():
  if not line.startswith('+++ b/'):continue
  relative=Path(line[6:]);assert not relative.is_absolute() and '..' not in relative.parts
  path=r/relative;originals[path]=path.read_bytes() if path.exists() else None
  if originals[path] is not None:path.write_bytes(originals[path].replace(b'\r\n',b'\n'))
 with tempfile.TemporaryDirectory(prefix='navigation-746-') as tmp:
  patch=Path(tmp)/name;patch.write_bytes(data)
  subprocess.run(['git','-C',str(r),'apply','--check',str(patch)],check=True)
  subprocess.run(['git','-C',str(r),'apply',str(patch)],check=True)
except Exception:
 for path,before in originals.items():
  if before is None:path.unlink(missing_ok=True)
  else:path.write_bytes(before)
 raise
print('Applied .746 settled detail previews, immediate controller entry, lazy/cullable list icons and stale-action/generation guards')
