#!/usr/bin/env python3
"""Inspect original Colosseum effects with the pinned external asset tooling.

Exports private textures, emitter reports and any embedded geometry. This does
not claim that the WZX virtual machine has been ported to Twilight Princess.
"""
import argparse
import dataclasses
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import struct
import sys
from types import SimpleNamespace

from import_model import ADDON_COMMIT


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--addon',type=Path,required=True)
    p.add_argument('--source',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('patterns',nargs='+')
    a=p.parse_args();addon=a.addon.resolve()
    assert subprocess.check_output(['git','-C',str(addon),'rev-parse','HEAD'],text=True).strip()==ADDON_COMMIT
    spec=importlib.util.spec_from_file_location('gamecube_dat_model',addon/'__init__.py',submodule_search_locations=[str(addon)])
    module=importlib.util.module_from_spec(spec);sys.modules[spec.name]=module;spec.loader.exec_module(module)
    from gamecube_dat_model.importer.phases.extract.helpers.fsys import parse_fsys
    from gamecube_dat_model.shared.helpers.pkx import PKXContainer
    from gamecube_dat_model.shared.helpers.wzx import extract_wzx
    from gamecube_dat_model.importer.phases.describe.helpers.particles import describe_particles
    from gamecube_dat_model.shared.gx_texture import decode_texture,FORMAT_INFO
    from PIL import Image
    sources=sorted({p for pattern in a.patterns for p in a.source.glob(pattern)})
    a.out.mkdir(parents=True,exist_ok=True);reports=[]
    for path in sources:
        out=a.out/path.stem;out.mkdir(exist_ok=True)
        report={'source':path.name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'entries':[]}
        for data,ext,name in parse_fsys(path.read_bytes(),path.name):
            payloads=[(b'',PKXContainer(data).gpt1_data)] if ext=='pkx' else extract_wzx(data) if ext=='wzx' else []
            if ext=='wzx':
                # The third-party WZX walker stops at specialized Effect
                # records. GPT1 containers have their own bounded offsets;
                # recover those without guessing the WZX command semantics.
                existing={hashlib.sha256(g).hexdigest() for _,g in payloads if g}
                offset=0
                while True:
                    start=data.find(b'GPT1',offset)
                    if start<0:break
                    offset=start+4
                    if start%32 or start+32>len(data):continue
                    _,ptl,txg,tex_len,ref=struct.unpack_from('>5I',data,start)
                    if not 32<=ptl<txg<=ref<len(data)-start or start+ptl+12>len(data):continue
                    count=struct.unpack_from('>I',data,start+ptl+8)[0]
                    end=ref+count*4
                    if not 0<count<=1024 or end>len(data)-start or tex_len>ref-txg:continue
                    gpt=data[start:start+end]
                    digest=hashlib.sha256(gpt).hexdigest()
                    if digest not in existing:payloads.append((b'',gpt));existing.add(digest)
            for dat,gpt in payloads:
                index=len(report['entries']);entry={'name':name,'textures':[],'generators':[]}
                if dat:
                    entry['model']=f'{index:02d}.dat';(out/entry['model']).write_bytes(dat)
                particles=describe_particles(gpt) if gpt else None
                if particles:
                    # Verified against supplied particle.c: texTable pointers
                    # are relative to texBank, and +0x08 is the palette format,
                    # NOT a pixel offset. Correct the external inspector here.
                    txg=struct.unpack_from('>I',gpt,8)[0]
                    groups=struct.unpack_from('>I',gpt,txg)[0];tid=0
                    for group in range(groups):
                        ptr=txg+struct.unpack_from('>I',gpt,txg+4+group*4)[0]
                        num,fmt,tlut,w,h=struct.unpack_from('>5I',gpt,ptr)
                        palnum,palflags=struct.unpack_from('>2H',gpt,ptr+20)
                        if fmt not in FORMAT_INFO or not 0<w<=2048 or not 0<h<=2048 or num>256:raise ValueError('Invalid particle texture')
                        bpp,tw,th,_=FORMAT_INFO[fmt];size=((w+tw-1)//tw)*((h+th-1)//th)*tw*th*bpp//8
                        for texture in range(num):
                            start=txg+struct.unpack_from('>I',gpt,ptr+24+texture*4)[0]
                            if start+size>len(gpt):raise ValueError('Particle pixels out of bounds')
                            palette=None
                            if fmt in (8,9,10):
                                palette_index=num+(0 if palflags&1 or palnum else texture)
                                palette_start=txg+struct.unpack_from('>I',gpt,ptr+24+palette_index*4)[0]
                                palette_size={8:32,9:512,10:32768}[fmt]
                                palette=SimpleNamespace(raw_data=gpt[palette_start:palette_start+palette_size],format=tlut)
                            pixels=decode_texture(gpt[start:start+size],w,h,fmt,palette)
                            if pixels is None:raise ValueError('Particle decode failed')
                            filename=f'{index:02d}-{tid:02d}.png';tid+=1
                            Image.frombytes('RGBA',(w,h),bytes(pixels)).save(out/filename)
                            entry['textures'].append({'file':filename,'width':w,'height':h,'group':group,'frame':texture})
                    for generator in particles.generators:
                        g=dataclasses.asdict(generator)
                        # Preserve meaningful source terminology for later
                        # timing work; max_particles was a parser misnomer.
                        g['particle_lifetime']=g.pop('max_particles')
                        for ins in g['instructions']:ins['raw_bytes']=ins['raw_bytes'].hex()
                        entry['generators'].append(g)
                report['entries'].append(entry)
        (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');reports.append(report)
        print(path.name,'models',sum('model' in e for e in report['entries']),
              'textures',sum(len(e['textures']) for e in report['entries']),flush=True)
    (a.out/'index.json').write_text(json.dumps(reports,indent=2)+'\n')


if __name__=='__main__':main()
