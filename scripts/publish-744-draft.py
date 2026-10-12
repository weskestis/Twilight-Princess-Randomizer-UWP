"""Deliver the verified package and support files through an owner-accessible draft."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('package_directory',type=Path);p.add_argument('evidence',type=Path);a=p.parse_args()
repo=os.environ['GITHUB_REPOSITORY'];run=os.environ['GITHUB_RUN_ID'];revision=os.environ['GITHUB_SHA'];tag='xbox-1.4.1.744-'+run
files=[a.package_directory/'TwilightPrincessRandomizer_1.4.1.744_x64.msix',a.package_directory/'Twilight-Princess-Randomizer-UWP-1.4.1.744-Xbox-Support.zip',a.evidence/'Twilit-Essentials-2.5.0-7682060c-source.zip']
def digest(file):
    with file.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
hashes={f.name:digest(f) for f in files}
notes=a.evidence/'RELEASE-NOTES-744.md'
notes.write_text('Xbox UWP 1.4.1.744, pinned Dusklight 2.0.1.\n\nPickup binding and message retry, lighting-aware pot/pumpkin tints, menu thumbnail/cache/panel changes, configurable DOF/VBAO budgets, Soul-gated combat rewards and release-mode shop resource guards.\n\nAdds signed source ports of Crests of Courage 1.10.0, A Link Between Twilight 0.3.0 and MFB Multiplayer 0.2.0. Native voice reception is included; Xbox microphone input is unavailable. Zora Swim, Ragdoll, Dark Morpheel and hopex Dark Link still require rebuildable author source.\n\nFresh 67-scenario strict generation, production regression/negative controls, all native metadata/symbol/import/resource checks and MSIX signature verified. Console retesting remains pending; the reported shop crash cause is unconfirmed.\n\nMSIX SHA-256: '+hashes[files[0].name]+'\n',encoding='utf-8')
view=subprocess.run(['gh','release','view',tag,'--repo',repo],capture_output=True)
if view.returncode:
    subprocess.run(['gh','release','create',tag,'--repo',repo,'--draft','--target',revision,'--title','Xbox UWP 1.4.1.744','--notes-file',str(notes)],check=True)
else:
    release=json.loads(subprocess.check_output(['gh','api','repos/'+repo+'/releases/tags/'+tag]))
    if not release['draft']:raise RuntimeError('Refusing to replace assets of a published release')
subprocess.run(['gh','release','upload',tag,*[str(f) for f in files],'--repo',repo,'--clobber'],check=True)
release=json.loads(subprocess.check_output(['gh','api','repos/'+repo+'/releases/tags/'+tag]))
assert release['draft'] and release['target_commitish']==revision
assets={x['name']:x for x in release['assets']};proof=[]
for file in files:
    asset=assets[file.name]
    assert asset['size']==file.stat().st_size and asset['digest']=='sha256:'+hashes[file.name],file.name
    proof.append({'name':file.name,'size':asset['size'],'sha256':hashes[file.name],'assetId':asset['id'],'url':asset['browser_download_url']})
receipt={'repository':repo,'tag':tag,'run':run,'sourceRevision':revision,'releaseId':release['id'],'draft':True,'releaseUrl':release['html_url'],'assets':proof}
(a.evidence/'GITHUB-DELIVERY-744.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
print('PASS: GitHub draft MSIX, support and recovered source sizes/digests match the verified local files')
print(proof[0]['url'])
