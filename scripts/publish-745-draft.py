"""Deliver the verified package and support files through an owner-accessible draft."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('package_directory',type=Path);p.add_argument('evidence',type=Path);a=p.parse_args()
repo=os.environ['GITHUB_REPOSITORY'];run=os.environ['GITHUB_RUN_ID'];revision=os.environ['GITHUB_SHA'];tag='xbox-1.4.1.745-'+run
files=[a.package_directory/'TwilightPrincessRandomizer_1.4.1.745_x64.msix',a.package_directory/'Twilight-Princess-Randomizer-UWP-1.4.1.745-Xbox-Support.zip',a.evidence/'Twilit-Essentials-2.5.0-7682060c-source.zip']
def digest(file):
    with file.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
hashes={f.name:digest(f) for f in files}
notes=a.evidence/'RELEASE-NOTES-745.md'
notes.write_text('Xbox UWP 1.4.1.745, pinned Dusklight 2.0.1.\n\nRestores random appearance colors per scene load, independent adjustable active-game-time overlay and red native death counter, preserving .744 pickup, lighting, performance, Soul-gate and shop fixes.\n\nAdds signed source ports of Crests of Courage 1.10.0, A Link Between Twilight 0.3.0 and MFB Multiplayer 0.2.0. Native voice reception is included; Xbox microphone input is unavailable. Zora Swim, Ragdoll, Dark Morpheel and hopex Dark Link still require rebuildable author source.\n\nFresh 67-scenario strict generation, production regression/negative controls, all native metadata/symbol/import/resource checks and MSIX signature verified. Console retesting remains pending; the reported shop crash cause is unconfirmed.\n\nMSIX SHA-256: '+hashes[files[0].name]+'\n',encoding='utf-8')
def find_release():
    # Drafts may have an untagged browser URL and are absent from /tags/{tag}.
    page=1
    while True:
        releases=json.loads(subprocess.check_output(['gh','api','repos/'+repo+'/releases?per_page=100&page='+str(page)]))
        match=next((item for item in releases if item['tag_name']==tag),None)
        if match:return match
        if len(releases)<100:return None
        page+=1
release=find_release()
if release is None:
    subprocess.run(['gh','release','create',tag,'--repo',repo,'--draft','--target',revision,'--title','Xbox UWP 1.4.1.745','--notes-file',str(notes)],check=True)
    release=find_release()
if not release or not release['draft']:raise RuntimeError('Refusing to replace assets of a published or unresolved release')
assert release['target_commitish']==revision
existing={item['name']:item for item in release['assets']}
pending=[file for file in files if file.name not in existing or existing[file.name]['size']!=file.stat().st_size or existing[file.name].get('digest')!='sha256:'+hashes[file.name]]
if pending:subprocess.run(['gh','release','upload',tag,*[str(f) for f in pending],'--repo',repo,'--clobber'],check=True)
release=json.loads(subprocess.check_output(['gh','api','repos/'+repo+'/releases/'+str(release['id'])]))
assert release['draft'] and release['target_commitish']==revision
assets={x['name']:x for x in release['assets']};proof=[]
for file in files:
    asset=assets[file.name]
    assert asset['size']==file.stat().st_size and asset['digest']=='sha256:'+hashes[file.name],file.name
    proof.append({'name':file.name,'size':asset['size'],'sha256':hashes[file.name],'assetId':asset['id'],'url':asset['browser_download_url']})
receipt={'repository':repo,'tag':tag,'run':run,'sourceRevision':revision,'releaseId':release['id'],'draft':True,'releaseUrl':release['html_url'],'assets':proof}
(a.evidence/'GITHUB-DELIVERY-745.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
print('PASS: GitHub draft MSIX, support and recovered source sizes/digests match the verified local files')
print(proof[0]['url'])
