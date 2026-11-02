#!/usr/bin/env python3
"""Export inspected source rigs to compact DPKM0002 with pose verification.

All source frames are retained. Matrices include evaluated constraints, original
bone visibility, facial atlases and source scale. Retail bytes stay external.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import zlib
from import_model import bind_action


def linear(v, np):
    return np.where(v <= .04045, v/12.92, ((v+.055)/1.055)**2.4)


def encoded(v, np):
    v=np.maximum(v,0)
    return np.where(v <= .0031308, v*12.92, 1.055*v**(1/2.4)-.055)


def evaluate(socket, pixels, np):
    if not socket.is_linked:
        return np.asarray(socket.default_value, dtype=np.float64)
    link=socket.links[0]; node=link.from_node
    if node.type in ('RGB','VALUE'):
        return np.asarray(node.outputs[0].default_value,dtype=np.float64)
    if node.type=='TEX_IMAGE':
        return pixels[...,3:4] if link.from_socket.name=='Alpha' else pixels
    if node.type=='MIX_RGB':
        factor=evaluate(node.inputs[0],pixels,np)
        a=evaluate(node.inputs[1],pixels,np); b=evaluate(node.inputs[2],pixels,np)
        if node.blend_type=='MIX': result=a*(1-factor)+b*factor
        elif node.blend_type=='MULTIPLY': result=a*(1-factor)+a*b*factor
        elif node.blend_type=='ADD': result=a+b*factor
        elif node.blend_type=='SUBTRACT': result=a-b*factor
        else: raise ValueError('Unsupported color operation: '+node.blend_type)
        return np.clip(result,0,1) if node.use_clamp else result
    if node.type=='MATH':
        a=evaluate(node.inputs[0],pixels,np);b=evaluate(node.inputs[1],pixels,np)
        if node.operation=='MULTIPLY':return a*b
        if node.operation=='ADD':return a+b
    raise ValueError('Unsupported color node: '+node.name+' '+node.type)


def material_texture(mat, np):
    nodes=mat.node_tree.nodes
    images=[n for n in nodes if n.type=='TEX_IMAGE']
    if len(images)>1: raise ValueError('Multi-texture material requires separate conversion')
    if images:
        image=images[0].image; w,h=image.size
        pixels=np.empty(w*h*4,dtype=np.float32);image.pixels.foreach_get(pixels)
        pixels=pixels.reshape(h,w,4).astype(np.float64)
        pixels[...,:3]=linear(pixels[...,:3],np)
    else:
        w=h=4;pixels=np.ones((h,w,4))
    principled=next((n for n in nodes if n.type=='BSDF_PRINCIPLED'),None)
    emissions=[n for n in nodes if n.type=='EMISSION' and n.name!='dat_ambient_emission']
    emissive=bool(emissions)
    # Source gas meshes use an emission + transparency shader. Its actual
    # texture colors/alpha are baked; the separate GPT1 emitter stays separate.
    color=evaluate(emissions[0].inputs['Color'] if emissive else principled.inputs['Base Color'],pixels,np)
    alpha=evaluate(principled.inputs['Alpha'],pixels,np)
    if emissive:
        shader=next((n for n in nodes if n.type=='MIX_SHADER'),None)
        if shader:alpha=evaluate(shader.inputs[0],pixels,np)
    rgba=np.ones((h,w,4),dtype=np.float64)
    rgba[...,:3]=np.broadcast_to(color,rgba.shape)[...,:3]
    if np.ndim(alpha)>0 and np.shape(alpha)[-1] in (3,4): alpha=np.mean(alpha[...,:3],axis=-1,keepdims=True)
    rgba[...,3:4]=alpha
    rgba[...,:3]=encoded(rgba[...,:3],np)
    rgba=np.rint(np.clip(rgba,0,1)*255).astype(np.uint8)[::-1]
    flags=int(np.any(rgba[...,3]!=255)) | (8 if emissive else 0)
    if images and images[0].extension=='REPEAT':flags|=6
    for axis,bit in [('s',32),('t',64)]:
        if nodes.get('wrap_pp_'+axis+'_0'):flags|=bit
    if any(n.type=='TEX_COORD' and any(l.from_node==n and l.from_socket.name=='Reflection' for l in mat.node_tree.links) for n in nodes):
        flags|=16
    if w%4 or h%4 or min(w,h)<4:
        # GX still allocates a 4x4 tile for 1x1/2x2 source textures. Expand
        # those texels while preserving normalized UVs and repeat behavior.
        nw=max(4,(w+3)//4*4);nh=max(4,(h+3)//4*4)
        rgba=rgba[(np.arange(nh)*h//nh)[:,None],(np.arange(nw)*w//nw)[None,:]]
        w,h=nw,nh
    data=bytearray()
    for y in range(0,h,4):
        for x in range(0,w,4):
            tile=rgba[y:y+4,x:x+4].reshape(16,4)
            data+=tile[:,[3,0]].tobytes()+tile[:,[1,2]].tobytes()
    return w,h,bytes(data),flags


def uv_state(mat):
    mapping=mat.node_tree.nodes.get('TexMapping_0')
    if not mapping or any(n.type=='TEX_COORD' for n in mat.node_tree.nodes):return [1,0,0,0,1,0,1,1,1,1]
    loc,rot,scale=[mapping.inputs[n].default_value for n in ('Location','Rotation','Scale')]
    if abs(rot.x)>1e-5 or abs(rot.y)>1e-5:raise ValueError('Unsupported 3D UV mapping')
    c,s=math.cos(rot.z),math.sin(rot.z)
    uv=[c*scale.x,-s*scale.y,loc.x,s*scale.x,c*scale.y,loc.y]
    if mapping.vector_type=='TEXTURE':
        if abs(scale.x)<1e-8 or abs(scale.y)<1e-8:raise ValueError('Singular UV mapping')
        uv=[c/scale.x,s/scale.x,-(c*loc.x+s*loc.y)/scale.x,-s/scale.y,c/scale.y,(s*loc.x-c*loc.y)/scale.y]
    elif mapping.vector_type!='POINT':raise ValueError('Unsupported mapping type')
    return uv+[1,1,1,1]


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--model',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    a=p.parse_args()
    import bpy
    import numpy as np
    bpy.ops.wm.open_mainfile(filepath=str(a.model.resolve()),load_ui=False,use_scripts=False)
    rigs=[o for o in bpy.context.scene.objects if o.type=='ARMATURE']
    if len(rigs)!=1:raise ValueError('Expected one Pokemon rig')
    rig=rigs[0];scene=bpy.context.scene
    bones=list(rig.data.bones);joint_ids={b.name:i for i,b in enumerate(bones)}
    actions=sorted(bpy.data.actions,key=lambda x:x.name)
    idle=next(x for x in actions if x.name.endswith('_Idle'))
    bind_action(bpy,idle);scene.frame_set(int(idle.frame_range[0]))
    rotation=np.array([[1,0,0,0],[0,0,1,0],[0,-1,0,0],[0,0,0,1]],dtype=np.float64)
    rig_world=rotation@np.array(rig.matrix_world)
    rig_world_inv=np.linalg.inv(rig_world)
    inverse_bind=[np.linalg.inv(np.array(b.matrix_local)) for b in bones]
    rig.data.pose_position='REST';bpy.context.view_layer.update()
    graph=bpy.context.evaluated_depsgraph_get()
    vertices=[];weights=[];indices=[];batches=[];materials=[];textures=[];checks=[]
    objects=sorted((o for o in scene.objects if o.type=='MESH' and not o.hide_render),key=lambda x:x.name)
    for obj in objects:
        if any(m.type!='ARMATURE' for m in obj.modifiers):raise ValueError('Apply non-skin modifiers before skeletal export')
        if not any(m.type=='ARMATURE' and m.object==rig for m in obj.modifiers):raise ValueError('Missing rig binding')
        evaluated=obj.evaluated_get(graph);mesh=evaluated.to_mesh(preserve_all_data_layers=True,depsgraph=graph);mesh.calc_loop_triangles()
        transform=rotation@np.array(evaluated.matrix_world)
        normal_transform=np.linalg.inv(transform[:3,:3]).T
        if len(mesh.materials)!=1:raise ValueError('Expected one material per draw batch')
        material=obj.data.materials[0];tex=material_texture(material,np)
        tex_index=next((i for i,t in enumerate(textures) if t[:3]==tex[:3]),len(textures))
        if tex_index==len(textures):textures.append(tex)
        first=len(indices);uv_layer=mesh.uv_layers.active
        unique={};representatives=[]
        for triangle in mesh.loop_triangles:
            for li in triangle.loops:
                vi=mesh.loops[li].vertex_index
                uv=tuple(uv_layer.data[li].uv) if uv_layer else (0.,0.)
                normal=normal_transform@np.array(mesh.corner_normals[li].vector);normal/=np.linalg.norm(normal)
                key=(vi,*np.round(normal,6),*uv)
                if key not in unique:
                    pos=(transform@np.array([*mesh.vertices[vi].co,1]))[:3]
                    groups=[(joint_ids[obj.vertex_groups[g.group].name],g.weight) for g in obj.data.vertices[vi].groups if g.weight>1e-8]
                    if not 1<=len(groups)<=4:raise ValueError('Unsupported skin influence count')
                    total=sum(w for _,w in groups)
                    if abs(total-1)>1e-4:raise ValueError('Unnormalized weights')
                    groups += [(0,0)]*(4-len(groups))
                    unique[key]=len(vertices);representatives.append((unique[key],vi,li))
                    vertices.append([*pos,*normal,*uv,1,1,1,1])
                    weights.append(groups)
                indices.append(unique[key])
        if len(indices)>first:
            batches.append((first,len(indices)-first,tex_index,tex[3]));materials.append(material)
            checks.append((obj,representatives))
        evaluated.to_mesh_clear()
    rig.data.pose_position='POSE';bpy.context.view_layer.update()
    verts=np.array(vertices,dtype=np.float64)
    joints=np.array([[j for j,w in a] for a in weights],dtype=np.uint16)
    ws=np.array([[w for j,w in a] for a in weights],dtype=np.float64)
    lower=np.min(verts[:,:3],axis=0);span=np.maximum(np.ptp(verts[:,:3],axis=0),1e-6)
    body=bytearray(struct.pack('<5I6fI',len(vertices),len(indices),len(batches),len(textures),len(actions),*lower,*span,len(bones)))
    for bone in bones:
        position=rig_world@np.array([*bone.matrix_local.translation,1])
        body+=struct.pack('<64s3f',bone.name.encode(),*position[:3])
    for i,v in enumerate(vertices):body+=struct.pack('<12f4H4f',*v,*joints[i],*ws[i])
    body+=np.array(indices,dtype='<u4').tobytes()+np.array(batches,dtype='<u4').tobytes()
    for w,h,data,_ in textures:body+=struct.pack('<3I',w,h,len(data))+data
    report=[];max_error=0;total_frames=0
    for action in actions:
        bind_action(bpy,idle);scene.frame_set(int(idle.frame_range[0]));bind_action(bpy,action)
        lo,hi=map(int,action.frame_range);count=hi-lo+1
        if not 1<=count<=4096:raise ValueError('Invalid source timeline')
        body+=struct.pack('<64sIf',action.name.encode(),count,60.)
        for frame in range(lo,hi+1):
            scene.frame_set(frame);graph=bpy.context.evaluated_depsgraph_get()
            evaluated_rig=rig.evaluated_get(graph)
            matrices=[]
            for i,b in enumerate(bones):
                matrix=rig_world@np.array(evaluated_rig.pose.bones[b.name].matrix)@inverse_bind[i]@rig_world_inv
                normal=np.linalg.pinv(matrix[:3,:3]).T
                if not np.isfinite(matrix).all() or not np.isfinite(normal).all() or max(abs(matrix).max(),abs(normal).max())>100000:
                    raise ValueError('Invalid evaluated bone matrix')
                matrices.append(matrix)
                body+=np.concatenate((matrix[:3].reshape(-1),normal.reshape(-1))).astype('<f4').tobytes()
            for mat in materials:body+=struct.pack('<10f',*uv_state(mat.evaluated_get(graph)))
            # Compare independent Blender deformation with the exported skin path
            # at the start/middle/end of every clip, including collapsed props.
            if frame in (lo,(lo+hi)//2,hi):
                matrices=np.array(matrices)
                source=np.column_stack((verts[:,:3],np.ones(len(verts))))
                predicted=np.sum(np.einsum('nvij,nj->nvi',matrices[joints],source)*ws[...,None],axis=1)[:,:3]
                for obj,representatives in checks:
                    evaluated=obj.evaluated_get(graph);mesh=evaluated.to_mesh();world=rotation@np.array(evaluated.matrix_world)
                    for index,vi,_ in representatives:
                        expected=(world@np.array([*mesh.vertices[vi].co,1]))[:3]
                        max_error=max(max_error,float(np.linalg.norm(expected-predicted[index])))
                    evaluated.to_mesh_clear()
                if max_error>0.0002:raise ValueError(f'Skinning differs from Blender: {max_error}')
        total_frames+=count;report.append(dict(name=action.name,frames=count,fps=60))
        print(action.name,count,flush=True)
    data=b'DPKM0002'+struct.pack('<II',len(body)+16,zlib.crc32(body))+body
    if len(data)>128*1024*1024:raise ValueError('Model exceeds runtime budget')
    a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_bytes(data)
    metadata=dict(format='DPKM0002',bytes=len(data),sha256=hashlib.sha256(data).hexdigest(),
                  vertices=len(vertices),bones=len(bones),triangles=len(indices)//3,batches=len(batches),
                  clips=report,total_frames=total_frames,max_blender_position_error=max_error,
                  source_blend_sha256=hashlib.sha256(a.model.read_bytes()).hexdigest())
    a.out.with_suffix('.json').write_text(json.dumps(metadata,indent=2)+'\n')
    print(json.dumps({k:v for k,v in metadata.items() if k!='clips'}),flush=True)
    bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.wm.quit_blender()

if __name__=='__main__':main()
