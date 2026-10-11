"""Restore every verified CP20 user mod as an optional directory-backed UWP mod."""
import argparse,hashlib,json,os,re,shutil,stat,subprocess,sys,zipfile
from pathlib import Path,PurePosixPath
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('--cache',type=Path);a=p.parse_args();source=a.source.resolve();control=Path(__file__).resolve().parents[1]
lock=json.loads((control/'CUSTOM_MODS-743.json').read_text());cache=(a.cache or source/'.uwp-custom-downloads-743').resolve();cache.mkdir(parents=True,exist_ok=True)
def verify(path,entry):
    return path.is_file() and path.stat().st_size==entry['size'] and hashlib.file_digest(path.open('rb'),'sha256').hexdigest()==entry['sha256']
def download(entry):
    target=cache/entry['name']
    if not verify(target,entry):
        if target.exists():target.unlink()
        subprocess.run(['gh','release','download',lock['release'],'--repo',os.environ['GITHUB_REPOSITORY'],'--pattern',entry['name'],'--dir',str(cache)],check=True)
        if not verify(target,entry):raise RuntimeError('Custom source hash/size mismatch: '+entry['name'])
    return target
checkpoint=download(lock['checkpoint']);work=cache/'cp20';work.mkdir(exist_ok=True)
def safe_extract(package,destination,allowed=None):
    names=set()
    for item in package.infolist():
        name=item.filename;path=PurePosixPath(name)
        if '\\' in name or path.is_absolute() or '..' in path.parts or ':' in name or stat.S_ISLNK(item.external_attr>>16):raise RuntimeError('Unsafe custom package path')
        if name.casefold() in names:raise RuntimeError('Duplicate custom package path')
        names.add(name.casefold())
        if item.is_dir() or (allowed and not allowed(name)):continue
        if item.file_size>128*1024**2:raise RuntimeError('Unexpected checkpoint member size')
        target=destination/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(package.read(item))
with zipfile.ZipFile(checkpoint) as package:
    safe_extract(package,work,lambda n:any('/'+part+'/' in n for part in ('scripts','optional_mod_inputs','texture_audit','mod_audit')))
project=work/'Twilight-Princess-Randomizer-UWP';inputs=project/'optional_mod_inputs'
aliases={'HourOfTwilight.-.Patron.Build.v1.GC.zip':'HourOfTwilight - Patron Build v1 GC.zip','woof_woof.1.zip':'woof_woof (1).zip','tunic.packs.zip':'tunic packs.zip'}
for entry in lock['external']:
    original=download(entry);shutil.copyfile(original,inputs/aliases.get(entry['name'],entry['name']))
generated=project/'bundled_mods';report=project/'mod_audit/CP20_RESTORED_MOD_REPORT.json'
subprocess.run([sys.executable,str(project/'scripts/build_optional_mod_catalog.py'),'--input-dir',str(inputs),'--output-dir',str(generated),'--inventory',str(project/'texture_audit/CP20_TEXTURE_INVENTORY.csv'),'--report',str(report)],check=True)
inventory=json.loads(report.read_text());expected=json.loads((project/'mod_audit/CP20_OPTIONAL_MOD_REPORT.json').read_text())
if inventory!=expected:raise RuntimeError('Restored custom catalog differs from the original audited catalog')
counts=inventory['counts']
if (counts['selectable_mod_entries'],counts['texture_files'],counts['overlay_files'])!=(35,335,421):raise RuntimeError('Incomplete custom catalog')
optional=source/'platforms/uwp/optional_mods';optional.mkdir(parents=True,exist_ok=True)
for mod in inventory['mods']:
    if not re.fullmatch(r'[a-z0-9_]+(?:\.[a-z0-9_]+)+',mod['id']):raise RuntimeError('Unsafe custom mod id')
    target=optional/mod['id'];origin=generated/mod['file']
    if target.exists():shutil.rmtree(target)
    if origin.is_dir():shutil.copytree(origin,target)
    else:
        if hashlib.file_digest(origin.open('rb'),'sha256').hexdigest()!=mod['sha256']:raise RuntimeError('Custom generated package hash mismatch')
        with zipfile.ZipFile(origin) as package:safe_extract(package,target)
    manifest=json.loads((target/'mod.json').read_text())
    if manifest['id']!=mod['id']:raise RuntimeError('Custom manifest identity mismatch')
    payload=sorted(str(f.relative_to(target)).replace('\\','/') for f in target.rglob('*') if f.is_file() and f.name!='mod.json')
    if payload!=sorted(mod['payload_files']):raise RuntimeError('Custom payload set mismatch: '+mod['id'])
    if any(name.lower().endswith(('.dll','.exe','.so','.dylib')) for name in payload):raise RuntimeError('Custom resource contains native binary')
    print('Restored '+mod['name']+' as optional directory resources',flush=True)
summary={**lock,'mods':[]}
for mod in inventory['mods']:
    target=optional/mod['id']
    summary['mods'].append({'id':mod['id'],'name':mod['name'],'exclusive_group':mod['exclusive_group'],'files':[{'path':str(f.relative_to(target)).replace('\\','/'),'size':f.stat().st_size,'sha256':hashlib.file_digest(f.open('rb'),'sha256').hexdigest()} for f in sorted(target.rglob('*')) if f.is_file()]})
(optional/'CUSTOM-MODS-743.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
print('PASS: all 35 original custom mods restored with source and payload hash verification')
