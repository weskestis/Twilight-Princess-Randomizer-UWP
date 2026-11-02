#!/usr/bin/env python3
"""Package selected original particle masks for the real-time effect renderer."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

SOURCES=[
    ('Glow','wzx_kaenhousya_attack/00-05.png'),
    ('Fire','pkx_hitokage/00-01.png'),
    ('Gas','pkx_ghos/00-00.png'),
    ('Bolt','wzx_denkishock_pikachu/00-00.png'),
    ('BoltBranch','wzx_denkishock_pikachu/00-01.png'),
    ('Spark','wzx_denkishock_pikachu/00-03.png'),
    ('Vortex','wzx_denkishock_pikachu/00-07.png'),
    ('Ring','wzx_monsterball_out/00-06.png'),
    ('Flash','wzx_monsterball_open/01-03.png'),
    ('Rays','wzx_monsterball_open/01-04.png'),
    ('Mist','wzx_mizudeppou_attack/00-00.png'),
    ('Droplet','wzx_mizudeppou_attack/00-01.png'),
    ('Bubble','wzx_mizudeppou_attack/00-17.png'),
    ('Star','wzx_kaenhousya_attack/00-12.png'),
    ('Needle','wzx_monsterball_open/01-07.png'),
    ('Pulse','wzx_denkishock_pikachu/00-10.png'),
]

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    import numpy as np
    from PIL import Image
    body=bytearray(struct.pack('<I',len(SOURCES)));report=[]
    for name,source in SOURCES:
        path=a.source/source;image=Image.open(path).convert('RGBA');w,h=image.size
        data=np.asarray(image,dtype=np.float32)/255
        mask=np.mean(data[...,:3],axis=2)*data[...,3]
        if name=='Rays':
            # This source mask is one quarter of a radial flash. The original
            # particle system mirrors it about its bottom-right corner.
            mask=np.block([[mask,mask[:,::-1]],[mask[::-1],mask[::-1,::-1]]]);h,w=mask.shape
        rgba=np.full((h,w,4),255,dtype=np.uint8);rgba[...,3]=np.rint(mask*255)
        nw=max(4,(w+3)//4*4);nh=max(4,(h+3)//4*4)
        rgba=rgba[(np.arange(nh)*h//nh)[:,None],(np.arange(nw)*w//nw)[None,:]];w,h=nw,nh
        tiled=bytearray()
        for y in range(0,h,4):
            for x in range(0,w,4):
                tile=rgba[y:y+4,x:x+4].reshape(16,4);tiled+=tile[:,[3,0]].tobytes()+tile[:,[1,2]].tobytes()
        body+=struct.pack('<32s3I',name.encode(),w,h,len(tiled))+tiled
        report.append({'name':name,'source':source,'source_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'width':w,'height':h})
    out=b'DPFX0001'+struct.pack('<II',len(body)+16,zlib.crc32(body))+body
    a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_bytes(out)
    a.out.with_suffix('.json').write_text(json.dumps({'format':'DPFX0001','bytes':len(out),'sha256':hashlib.sha256(out).hexdigest(),'textures':report},indent=2)+'\n')
    print(len(out),'bytes,',len(SOURCES),'source particle masks')

if __name__=='__main__':main()
