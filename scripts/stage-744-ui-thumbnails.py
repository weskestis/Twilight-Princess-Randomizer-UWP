"""Prepare small UI-only PNGs; retain every original manifest/game resource byte."""
import argparse, hashlib, io, json
from pathlib import Path, PurePosixPath
from PIL import Image

p=argparse.ArgumentParser();p.add_argument('source',type=Path);a=p.parse_args()
root=a.source.resolve();control=Path(__file__).resolve().parents[1]
optional=root/'platforms/uwp/optional_mods'
native=json.loads((control/'NATIVE_MODS-743.json').read_text())['mods']
native += json.loads((control/'NATIVE_MODS-744.json').read_text())['mods']
catalog=json.loads((control/'BUNDLED_MODS-736.json').read_text())['mods']
directories=[root/'mods'/n for n in ['randomizer','cosmetics','controller_ui','luau_runtime']]
directories += [root/'mods/uwp_ports'/m['target'] for m in native]
directories += [optional/m['id'] for m in catalog if m['bundle'] and m['id']!='org.dusklight.tp_classic_buttons']
custom_inventory=json.loads((optional/'CUSTOM-MODS-743.json').read_text()) if (optional/'CUSTOM-MODS-743.json').is_file() else {'mods':[]}
directories += [optional/m['id'] for m in custom_inventory['mods']]
proof=[]
for directory in directories:
    manifest=directory/'mod.json'
    if not manifest.is_file():continue
    metadata=json.loads(manifest.read_text(encoding='utf-8-sig'))
    paths={}
    for field,default in [('icon','res/icon.png'),('banner','res/banner.png')]:
        path=metadata.get(field,default)
        if isinstance(path,str) and path:paths[path]=128 if field=='icon' else 768
    for relative,limit in paths.items():
        path=PurePosixPath(relative)
        if path.is_absolute() or '..' in path.parts or '\\' in relative or ':' in relative:raise RuntimeError('Unsafe UI image path')
        original=directory/relative
        if not original.is_file():continue
        data=original.read_bytes()
        if not data.startswith(b'\x89PNG\r\n\x1a\n'):continue
        with Image.open(io.BytesIO(data)) as opened:
            if opened.width>8192 or opened.height>8192 or opened.width*opened.height>64*1024**2:raise RuntimeError('UI image exceeds build decode guard')
            image=opened.convert('RGBA');image.thumbnail((limit,limit),Image.Resampling.LANCZOS)
            relative_out='res/uwp-ui-744/'+relative.encode('utf-8').hex()+'.png'
            target=directory/relative_out;target.parent.mkdir(parents=True,exist_ok=True)
            image.save(target,format='PNG',optimize=True)
            entry={'mod':metadata['id'],'packageDirectory':directory.name,'directory':directory.relative_to(root).as_posix(),
                   'originalPath':relative,'originalSha256':hashlib.sha256(data).hexdigest(),
                   'path':relative_out,'width':image.width,'height':image.height,'limit':limit,
                   'size':target.stat().st_size,'sha256':hashlib.sha256(target.read_bytes()).hexdigest()}
            proof.append(entry)
        assert hashlib.sha256(original.read_bytes()).hexdigest()==entry['originalSha256']
if not proof:raise RuntimeError('No packaged UI images were prepared')
for filename,list_field in [('NATIVE-MODS-743.json','resourceFiles'),('NATIVE-MODS-744.json','resourceFiles'),('CUSTOM-MODS-743.json','files')]:
    file=optional/filename
    if not file.is_file():continue
    inventory=json.loads(file.read_text())
    for mod in inventory['mods']:
        extra=[{k:x[k] for k in ['path','size','sha256']} for x in proof if x['mod']==mod['id']]
        by_path={f['path']:f for f in mod[list_field]};by_path.update({f['path']:f for f in extra})
        mod[list_field]=[by_path[k] for k in sorted(by_path)]
    file.write_text(json.dumps(inventory,indent=2)+'\n',encoding='utf-8',newline='\n')
optional.mkdir(parents=True,exist_ok=True)
(optional/'UI-THUMBNAILS-744.json').write_text(json.dumps({'images':proof},indent=2)+'\n',encoding='utf-8',newline='\n')
print('PASS: prepared '+str(len(proof))+' UI thumbnails, with original resource hashes preserved')
