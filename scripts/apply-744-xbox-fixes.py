"""Apply the pinned fixes to the complete .743 source before release certification."""
import hashlib,json,os,subprocess,tempfile
from pathlib import Path
root=Path(os.environ['TPR_SRC']);control=Path(os.environ.get('TPR_CONTROL',Path(__file__).resolve().parents[1]));identity=json.loads((control/'APP_IDENTITY-744.json').read_text())
names=['xbox-pickup-lifecycle-744.patch','xbox-menu-performance-744.patch','xbox-render-budget-744.patch','xbox-grotto-rewards-744.patch','xbox-shop-resource-guards-744-host.patch']
if 'Version="1.4.1.743"' not in (root/'platforms/uwp/Package.appxmanifest').read_text():
    raise RuntimeError('.744 requires the complete verified .743 source first')
originals={}
with tempfile.TemporaryDirectory(prefix='xbox-fixes-744-') as temp:
    patches=[]
    try:
        for name in names:
            payload=(control/'patches'/name).read_bytes().replace(b'\r\n',b'\n')
            if hashlib.sha256(payload).hexdigest()!=identity['sourcePatches'][name]:raise RuntimeError('Pinned source patch changed: '+name)
            for line in payload.decode().splitlines():
                if not line.startswith('+++ b/'):continue
                relative=Path(line[6:])
                if relative.is_absolute() or '..' in relative.parts:raise RuntimeError('Unsafe source patch path')
                path=root/relative
                if path not in originals:
                    originals[path]=path.read_bytes() if path.exists() else None
                    if originals[path] is not None:path.write_bytes(originals[path].replace(b'\r\n',b'\n'))
            patch=Path(temp)/name;patch.write_bytes(payload);patches.append(str(patch))
        subprocess.run(['git','-C',str(root),'apply','--check',*patches],check=True)
        subprocess.run(['git','-C',str(root),'apply',*patches],check=True)
    except Exception:
        for path,before in originals.items():
            if before is None:path.unlink(missing_ok=True)
            else:path.write_bytes(before)
        raise
print('Applied .744 pickup/text retry, menu caching, scene-lit pot/pumpkin tints, effect budgets, Soul-gated rewards and release-mode shop resource guards')
