"""Deliver exact verified assets through a release ID, including fresh drafts."""
import argparse,hashlib,json,os,subprocess,time
from pathlib import Path
from urllib.parse import quote
p=argparse.ArgumentParser();p.add_argument('package_directory',type=Path);p.add_argument('evidence',type=Path);p.add_argument('--release-id',type=int);a=p.parse_args()
repo=os.environ['GITHUB_REPOSITORY'];run=os.environ['GITHUB_RUN_ID'];revision=os.environ['GITHUB_SHA'];tag='xbox-1.4.1.748-'+run
files=[a.package_directory/'TwilightPrincessRandomizer_1.4.1.748_x64.msix',a.package_directory/'Twilight-Princess-Randomizer-UWP-1.4.1.748-Xbox-Support.zip',a.evidence/'Twilit-Essentials-2.5.0-7682060c-source.zip']
def digest(file):
    with file.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
hashes={f.name:digest(f) for f in files}
def api(endpoint,*args):
    return json.loads(subprocess.check_output(['gh','api',endpoint,*args]))
notes=a.evidence/'RELEASE-NOTES-748.md'
notes.write_text('Xbox UWP 1.4.1.748: explicit Off - Keep My Colors and Vanilla Colors choices. Manual equipment/HUD/character color edits turn global randomization off automatically, including selecting the same saved color again. Vanilla mode persists, bypasses saved Rainbow and per-color seed options, and keeps individual colors saved.\n\nRetains every .744-.747 runtime/mod fix, run overlays, Mods-menu navigation improvements and Companion Tracker bridge. Pinned Dusklight 2.0.1.\n\nProduction mode/UI/getter and real ConfigVar regression tests, 4000 manual-red-cap scene loads and 1000 vanilla scene loads; fresh 67 strict generation scenarios; native metadata/import/resource gates and MSIX signature verification. General-region pot/pumpkin hints in six languages (new seeds), native common-group Soul item-get text registration for 85 Enemy and eight Boss Souls, including legacy Huge Poe shop rewards; plus native pickup recovery for bound Soul and container rewards that survives optimized checkEndMessage calls and handles destroyed message actors. Exact-once item/Soul/save regressions remain passed. Physical Xbox testing remains pending. Some existing HUD recolors refresh on the next area load.\n\nMSIX SHA-256: '+hashes[files[0].name]+'\n',encoding='utf-8')
release=None
if a.release_id:
    release=api('repos/'+repo+'/releases/'+str(a.release_id))
else:
    page=1
    while True:
        releases=api('repos/'+repo+'/releases?per_page=100&page='+str(page))
        release=next((item for item in releases if item['tag_name']==tag),None)
        if release or len(releases)<100:break
        page+=1
    if release is None:
        request=a.evidence/'DRAFT-REQUEST-748.json'
        request.write_text(json.dumps({'tag_name':tag,'target_commitish':revision,'name':'Xbox UWP 1.4.1.748','draft':True,'body':notes.read_text()}),encoding='utf-8')
        # The creation response is authoritative; no immediate list/tag lookup.
        release=api('repos/'+repo+'/releases','--method','POST','--input',str(request))
if not release or not release['draft'] or release['tag_name']!=tag or release['target_commitish']!=revision:
    raise RuntimeError('Refusing to change a published, wrong-tag or wrong-revision release')
release_id=release['id'];existing={x['name']:x for x in release['assets']}
for file in files:
    asset=existing.get(file.name)
    if asset and asset['size']==file.stat().st_size and asset.get('digest')=='sha256:'+hashes[file.name]:continue
    if asset:subprocess.run(['gh','api','repos/'+repo+'/releases/assets/'+str(asset['id']),'--method','DELETE'],check=True)
    endpoint='https://uploads.github.com/repos/'+repo+'/releases/'+str(release_id)+'/assets?name='+quote(file.name,safe='')
    api(endpoint,'--method','POST','--input',str(file),'-H','Content-Type: application/octet-stream')
for attempt in range(6):
    release=api('repos/'+repo+'/releases/'+str(release_id));assets={x['name']:x for x in release['assets']}
    if all(f.name in assets and assets[f.name]['size']==f.stat().st_size and assets[f.name].get('digest')=='sha256:'+hashes[f.name] for f in files):break
    if attempt==5:raise RuntimeError('Uploaded asset receipt did not match the verified local files')
    time.sleep(2)
assert release['draft'] and release['tag_name']==tag and release['target_commitish']==revision
proof=[{'name':f.name,'size':assets[f.name]['size'],'sha256':hashes[f.name],'assetId':assets[f.name]['id'],'url':assets[f.name]['browser_download_url']} for f in files]
receipt={'repository':repo,'tag':tag,'run':run,'sourceRevision':revision,'releaseId':release_id,'draft':True,'releaseUrl':release['html_url'],'assets':proof}
(a.evidence/'GITHUB-DELIVERY-748.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
print('PASS: draft release ID, source revision and every MSIX/support/source asset size and digest match')
print(proof[0]['url'])
