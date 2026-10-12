"""Restore the byte-identical removed upstream dependency from Xbox .745 evidence."""
from pathlib import Path
import hashlib,json,os,shutil
root=Path(os.environ['TPR_SRC']);control=Path(os.environ['TPR_CONTROL'])
recovery=Path(os.environ['TMP'])/'hair-source-745'
mods=json.loads((control/'NATIVE_MODS-743.json').read_text())['mods']
entry=next(m['sourceArchive'] for m in mods if m['target']=='port_twilit_essentials')
source=recovery/'Twilit-Essentials-2.5.0-7682060c-source.zip'
assert source.is_file(),'Recovered .745 source artifact is missing'
assert source.stat().st_size==entry['size'],'Recovered source size differs'
assert hashlib.file_digest(source.open('rb'),'sha256').hexdigest()==entry['sha256'],'Recovered source checksum differs'
destination=root/'.uwp-native-downloads-743'/entry['name'];destination.parent.mkdir(parents=True,exist_ok=True)
shutil.copyfile(source,destination)
print('PASS: restored exact Twilit Essentials source; original size and SHA-256 pins retained')
