"""Prepare independently verifiable MSIX transfer parts and exact recovered source."""
import argparse,hashlib,json,shutil
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('package_directory',type=Path);p.add_argument('source',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
out=a.output;out.mkdir(parents=True,exist_ok=True);evidence=out/'evidence';evidence.mkdir(exist_ok=True)
package=a.package_directory/'TwilightPrincessRandomizer_1.4.1.747_x64.msix'
def digest(file):
    with file.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
parts=[]
with package.open('rb') as stream:
    for i in range(1,4):
        data=stream.read(384*1024**2)
        if not data:break
        folder=out/('part'+str(i));folder.mkdir(exist_ok=True);file=folder/(package.name+'.part'+str(i));file.write_bytes(data)
        parts.append({'name':file.name,'size':len(data),'sha256':digest(file),'artifactName':'Xbox747-MSIX-Part-'+str(i)})
    if stream.read(1):raise RuntimeError('MSIX exceeds the three verified transfer-part allowance')
assert sum(x['size'] for x in parts)==package.stat().st_size
for file in a.package_directory.iterdir():
    if file.is_file() and file.suffix!='.msix':shutil.copyfile(file,evidence/file.name)
control=Path(__file__).resolve().parents[1];lock=json.loads((control/'NATIVE_MODS-743.json').read_text())
essentials=next(m for m in lock['mods'] if m['target']=='port_twilit_essentials')['sourceArchive']
recovered=a.source/'.uwp-native-downloads-743'/essentials['name']
assert recovered.stat().st_size==essentials['size'] and digest(recovered)==essentials['sha256']
source=evidence/'Twilit-Essentials-2.5.0-7682060c-source.zip';shutil.copyfile(recovered,source)
receipt={'package':package.name,'size':package.stat().st_size,'sha256':digest(package),'parts':parts,
         'recoveredSource':{'name':source.name,'size':source.stat().st_size,'sha256':digest(source)}}
(evidence/'MSIX-TRANSFER-747.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
print('Prepared '+str(len(parts))+' verified MSIX parts and the hash-pinned recovered Twilit Essentials source')
