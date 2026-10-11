"""Verify restored custom files and all source-built native resources in the installed MSIX."""
import argparse,hashlib,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('package',type=Path);p.add_argument('source',type=Path);a=p.parse_args();control=Path(__file__).resolve().parents[1];root=a.package/'mods';staged=a.source/'platforms/uwp/optional_mods'
custom=json.loads((staged/'CUSTOM-MODS-743.json').read_text());native=json.loads((staged/'NATIVE-MODS-743.json').read_text())
for name,expected in [('CUSTOM-MODS-743.json',custom),('NATIVE-MODS-743.json',native)]:
    assert json.loads((root/name).read_text())==expected,'Packaged source/payload inventory changed: '+name
assert len(custom['mods'])==35
lock=json.loads((control/'NATIVE_MODS-743.json').read_text());assert len(native['mods'])==len(lock['mods'])
def verify(folder,files):
    count=0
    expected={item['path'] for item in files}
    actual={str(f.relative_to(folder)).replace('\\','/') for f in folder.rglob('*') if f.is_file()}
    assert actual==expected,'Packaged file set changed: '+str(folder)+'; missing='+repr(sorted(expected-actual))+'; unexpected='+repr(sorted(actual-expected))
    for item in files:
        path=folder/item['path'];assert path.stat().st_size==item['size'] and hashlib.file_digest(path.open('rb'),'sha256').hexdigest()==item['sha256'],'Packaged mod bytes changed: '+str(path)
        count+=1
    return count
count=0
for m in custom['mods']:
    folder=root/m['id'];manifest=json.loads((folder/'mod.json').read_text());assert manifest['id']==m['id'];assert manifest.get('exclusive_group')==m['exclusive_group'];count+=verify(folder,m['files'])
    print('PASS: restored '+m['name']+' with every texture/overlay/manifest byte intact')
for m in native['mods']:
    folder=root/m['target'];manifest=json.loads((folder/'mod.json').read_text());assert manifest['id']==m['id'] and manifest['version']==m['version'];count+=verify(folder,m['resourceFiles'])
    print('PASS: source-built '+m['name']+' '+m['version']+' resources and source provenance')
for file in root.rglob('*'):
    assert file.suffix.lower() not in ['.dll','.exe','.so','.dylib','.pdb','.lib'],'Unexpected native module in installed resources: '+str(file)
print('PASS: all 35 custom mods and '+str(len(native['mods']))+' additional native source ports, '+str(count)+' hash-verified installed files')
