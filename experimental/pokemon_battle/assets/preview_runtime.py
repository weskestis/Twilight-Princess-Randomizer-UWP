#!/usr/bin/env python3
"""Render a C++-decoded runtime snapshot for conversion QA, not a game screenshot."""
import argparse
import json
from pathlib import Path


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--snapshot',required=True,type=Path)
    p.add_argument('--out',required=True,type=Path)
    p.add_argument('--size',type=int,default=640)
    a=p.parse_args()
    import bpy
    import numpy as np
    from mathutils import Vector
    bpy.ops.wm.read_factory_settings(use_empty=True)
    data=json.loads((a.snapshot/'pose.json').read_text())
    vertices=np.array(data['vertices'])
    images=[]
    for t in data['textures']:
        w,h=t['width'],t['height']
        raw=np.frombuffer((a.snapshot/t['file']).read_bytes(),dtype=np.uint8)
        pixels=np.empty((h,w,4),dtype=np.float32);offset=0
        for y in range(0,h,4):
            for x in range(0,w,4):
                ar=raw[offset:offset+32].reshape(4,4,2)
                gb=raw[offset+32:offset+64].reshape(4,4,2)
                pixels[y:y+4,x:x+4]=np.stack([ar[:,:,1],gb[:,:,0],gb[:,:,1],ar[:,:,0]],axis=2)/255.
                offset+=64
        image=bpy.data.images.new(t['file'],width=w,height=h,alpha=True)
        image.colorspace_settings.name='sRGB'
        image.pixels.foreach_set(pixels[::-1].ravel());image.pack();images.append(image)
    points=vertices[:,:3][:,[0,2,1]]*np.array([1,-1,1])
    normals=vertices[:,3:6][:,[0,2,1]]*np.array([1,-1,1])
    inds=np.array(data['indices']).reshape(-1,3)
    for batch in data['batches']:
        triangles=inds[batch['first']//3:(batch['first']+batch['count'])//3]
        mesh=bpy.data.meshes.new('Runtime mesh')
        mesh.from_pydata(points.tolist(),[],triangles.tolist());mesh.update()
        obj=bpy.data.objects.new('Runtime draw batch',mesh);bpy.context.scene.collection.objects.link(obj)
        uv=mesh.uv_layers.new(name='UVMap');transform=np.array(batch['uv']).reshape(2,3)
        for loop in mesh.loops:
            v=vertices[loop.vertex_index];uv.data[loop.index].uv=transform@np.array([v[6],v[7],1])
        for polygon in mesh.polygons:polygon.use_smooth=True
        mesh.normals_split_custom_set([normals[loop.vertex_index] for loop in mesh.loops])
        mat=bpy.data.materials.new('Runtime material');mat.use_nodes=True
        node=mat.node_tree.nodes.get('Principled BSDF')
        node.inputs['Specular IOR Level'].default_value=0
        tex=mat.node_tree.nodes.new('ShaderNodeTexImage');tex.image=images[batch['texture']];tex.extension='EXTEND'
        mat.node_tree.links.new(tex.outputs['Color'],node.inputs['Base Color'])
        mat.node_tree.links.new(tex.outputs['Alpha'],node.inputs['Alpha'])
        if batch.get('flags',0)&8:
            mat.node_tree.links.new(tex.outputs['Color'],node.inputs['Emission Color'])
            node.inputs['Emission Strength'].default_value=1
        if batch.get('flags',0)&6:tex.extension='REPEAT'
        obj.data.materials.append(mat)
    scene=bpy.context.scene
    center=Vector((points.min(axis=0)+points.max(axis=0))/2)
    camera=bpy.data.objects.new('Camera',bpy.data.cameras.new('Camera'));scene.collection.objects.link(camera)
    camera.location=center+Vector((.9,-1.7,.65));camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.type='ORTHO';camera.data.ortho_scale=float((points.max(axis=0)-points.min(axis=0)).max())*1.4
    scene.camera=camera
    scene.world=bpy.data.worlds.new('Studio');scene.world.use_nodes=True
    scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.055,.065,.085,1)
    scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.6
    for name,pos,energy,size in [('Key',(-1,-2,3),100,3),('Fill',(2,-1,1),45,2),('Rim',(0,2,2),110,2)]:
        obj=bpy.data.objects.new(name,bpy.data.lights.new(name,'AREA'));scene.collection.objects.link(obj)
        obj.location=center+Vector(pos);obj.rotation_euler=(center-obj.location).to_track_quat('-Z','Y').to_euler()
        obj.data.energy=energy;obj.data.shape='DISK';obj.data.size=size
    scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=16
    scene.render.threads_mode='FIXED';scene.render.threads=8
    scene.render.resolution_x=scene.render.resolution_y=a.size;scene.render.resolution_percentage=100
    scene.view_settings.view_transform='Standard';scene.render.image_settings.file_format='PNG'
    scene.render.filepath=str(a.out.resolve());bpy.ops.render.render(write_still=True)
    bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.wm.quit_blender()


if __name__=='__main__':main()
