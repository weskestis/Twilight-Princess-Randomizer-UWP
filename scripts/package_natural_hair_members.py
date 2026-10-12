#!/usr/bin/env python3
"""Package the validated head using Dusklight's archive-member overlay route."""
from pathlib import Path
import hashlib,json,zipfile

ROOT=Path(__file__).resolve().parent
INPUT=ROOT/'hair-host-inputs'
OUTPUT=ROOT/'output/natural-hair'
OVERLAY='overlay/res/Object/Kmdl/Kmdl/bmwr/al_head.bmd'
NAMES=['SSB4-Fierce-Deity-Realistic.dusk','Fierce-Deity-Tunic-Pack-Realistic.dusk']

def main():
    OUTPUT.mkdir(parents=True,exist_ok=True)
    report=json.loads((ROOT/'natural-hair-work/native-export-validation.json').read_text())
    head=(ROOT/'natural-hair-work/al_head.bmd').read_bytes()
    assert report['status']=='NATIVE_ROUNDTRIP_PASS'
    assert hashlib.sha256(head).hexdigest()==report['sha256']
    atlas=(ROOT/'natural-hair-work/textures/Silver-Hair-Strand-Atlas.png').read_bytes()
    texture='textures/'+report['hair_texture']['replacement_filename']
    proof={'status':'PACKAGE_CHECKS_PASS','console_tested':False,'original_archive_upload_required':False,
           'archive_member_overlay':OVERLAY,'head_sha256':report['sha256'],
           'skeleton_sections_preserved':['EVP1','DRW1','JNT1'],
           'face_and_outfit_models_changed':False,
           'motion':'Existing game hair motion; enhanced spring motion requires the separate signed Xbox port',
           'appearance':'Native layered-card prototype; the approved AI concept is a visual target', 'mods':[]}
    for name in NAMES:
        with zipfile.ZipFile(INPUT/name) as z:
            assert z.testzip() is None
            assert len(z.namelist())==len(set(z.namelist()))
            payload={n:z.read(n) for n in z.namelist()}
        assert not any(n.startswith('overlay/') for n in payload)
        original=dict(payload)
        manifest=json.loads(payload['mod.json'])
        assert manifest['version']=='1.1.1-test1'
        manifest.update(version='1.2.0-test1',name=manifest['name'].split(' (')[0]+' (Natural Hair)',
            description='Refined Fierce Deity face and outfit with a new layered silver hair mesh. Uses the game\'s original body and hair skeleton. Enable one Link appearance, then fully quit and restart the game. Xbox gameplay validation pending.',
            geometry='Head hair mesh replaced through an archive-member overlay; face and outfit meshes unchanged',
            artwork_method='Refined face/outfit textures preserved; new alpha-masked hair cards and native hair atlas',
            hair_revision='Layered silver hair cards with separated ends weighted to the original hair joints',
            hardware_validation='Native geometry, skinning and package checks pass; console gameplay pending')
        payload['mod.json']=(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode()
        payload[OVERLAY]=head;payload[texture]=atlas
        target=OUTPUT/name
        with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
            for n,data in payload.items():z.writestr(n,data)
        with zipfile.ZipFile(target) as z:
            assert z.testzip() is None
            assert set(z.namelist())==set(payload)
            for n,data in original.items():
                if n!='mod.json':assert z.read(n)==data,n
            assert z.read(OVERLAY)==head and z.read(texture)==atlas
            assert 'overlay/res/Object/Kmdl.arc' not in z.namelist()
        proof['mods'].append({'file':name,'id':manifest['id'],'version':manifest['version'],
            'bytes':target.stat().st_size,'sha256':hashlib.sha256(target.read_bytes()).hexdigest(),
            'original_texture_files_preserved':sum(n.startswith('textures/') for n in original)})
    (OUTPUT/'VALIDATION.json').write_text(json.dumps(proof,indent=2)+'\n')
    (OUTPUT/'README.txt').write_text('Fierce Deity Natural Hair — Xbox test packages\n\n'
        'These packages replace only al_head.bmd through the native archive-member overlay route. '
        'The installed game supplies the original Link archive. No ISO or asset-export upload is needed.\n\n'
        'Extract this ZIP, then upload the two .dusk files through the app Mods > Import workflow, '
        'or Xbox Device Portal > File explorer > Twilight Princess Randomizer > '
        'LocalState/TwilitRealm/Dusklight/mods. Enable ONE Link appearance. Fully quit and restart the game.\n\n'
        'Use the current Dusklight 2.0.1 based Xbox build. Legacy 1.4 builds may not support this overlay route.\n'
        'Refined face/outfit texture files are preserved byte for byte. The new head keeps the original '
        'ten-joint skeleton and can use existing game hair motion. The separate enhanced spring-physics '
        'port requires the signed hair test app; source code alone is not an Xbox installable native mod.\n\n'
        'Validation: native mesh/material/skinning decode PASS; all ZIP CRCs and retained texture bytes PASS. '
        'Console loading, appearance, clipping and performance are pending. The mesh is a layered-card '
        'prototype; the previously approved AI picture is a design reference.\n')
    manifest=''.join(m['sha256']+'  '+m['file']+'\n' for m in proof['mods'])
    (OUTPUT/'SHA256SUMS.txt').write_text(manifest)
    with zipfile.ZipFile(OUTPUT/'Fierce-Deity-Realistic-Mods.zip','w',zipfile.ZIP_STORED) as z:
        for name in NAMES+['README.txt','VALIDATION.json','SHA256SUMS.txt']:z.write(OUTPUT/name,name)
    with zipfile.ZipFile(OUTPUT/'Fierce-Deity-Realistic-Mods.zip') as z:assert z.testzip() is None
    print(json.dumps(proof,indent=2))

if __name__=='__main__':main()
