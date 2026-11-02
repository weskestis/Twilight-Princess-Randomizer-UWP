#!/usr/bin/env python3
"""Author field locomotion on explicitly inspected Colosseum skeletons.

Original battle actions stay intact. Grounded limbs use length-preserving IK
with fixed stance feet; ghost movement retains the source hover performance.
Outputs and validation reports belong outside the source repository.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path

from animate_locomotion import foot_path
from import_model import bind_action


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--model',type=Path,required=True)
    p.add_argument('--species',type=int,required=True)
    p.add_argument('--out',type=Path,required=True)
    a=p.parse_args()
    spec=next(s for s in json.loads((Path(__file__).parent/'roster.json').read_text())['species'] if s['id']==a.species)
    cfg=spec['locomotion']
    import bpy
    from mathutils import Matrix,Vector
    bpy.ops.wm.open_mainfile(filepath=str(a.model.resolve()),load_ui=False,use_scripts=False)
    scene=bpy.context.scene
    rig=next(o for o in scene.objects if o.type=='ARMATURE')
    bones=list(rig.pose.bones)
    names={int(b.name.split('_')[1]):b.name for b in bones}
    original_actions=list(bpy.data.actions)
    idle=next(x for x in original_actions if x.name.endswith('_Idle'))
    bind_action(bpy,idle);scene.frame_set(int(idle.frame_range[0]))
    base={b.name:b.matrix.copy() for b in bones}
    basis={b.name:b.matrix_basis.copy() for b in bones}
    local={b.name:(b.parent.bone.matrix_local.inverted()@b.bone.matrix_local@basis[b.name]
                  if b.parent else b.bone.matrix_local@basis[b.name]) for b in bones}
    root=names[cfg['root']]
    chains=[[names[n] for n in chain] for chain in cfg['legs']]
    lengths=[[(base[v].translation-base[u].translation).length for u,v in zip(chain,chain[1:])] for chain in chains]
    if any(min(ls)<1e-5 for ls in lengths):raise ValueError('Degenerate locomotion chain')
    leg_length=min(map(sum,lengths)) if chains else .5
    controlled={root,*[n for chain in chains for n in chain]}
    controlled.update(names[n] for n in cfg['arms'])
    controlled.update(names[n] for n in [cfg['tail'],cfg['head']])
    graph=bpy.context.evaluated_depsgraph_get()
    targets=[]
    # Plant each sole, including the raised feet in some original idle poses.
    for chain in chains:
        foot=chain[-1];descendants={foot}
        for b in bones:
            if b.parent and b.parent.name in descendants:descendants.add(b.name)
        lowest=None
        for obj in scene.objects:
            if obj.type!='MESH' or obj.hide_render:continue
            evaluated=obj.evaluated_get(graph);mesh=evaluated.to_mesh()
            for v in obj.data.vertices:
                if sum(g.weight for g in v.groups if obj.vertex_groups[g.group].name in descendants)>.75:
                    z=(rig.matrix_world.inverted()@evaluated.matrix_world@mesh.vertices[v.index].co).z
                    lowest=z if lowest is None else min(lowest,z)
            evaluated.to_mesh_clear()
        target=base[foot].translation.copy()
        target.z-=lowest if lowest is not None else 0
        target.y=(target.y+base[chain[0]].translation.y)*.5
        targets.append(target)

    def rotate(m,angle,axis):
        result=Matrix.Rotation(angle,4,axis)@m;result.translation=m.translation;return result

    def solve(chain,ls,hip,target):
        points=[base[n].translation.copy() for n in chain]
        if (target-hip).length>=sum(ls)-1e-6:return None
        points=[v+hip-points[0] for v in points]
        # A small stable bend prevents a straight leg from becoming singular.
        for i in range(1,len(points)-1):points[i].y+=leg_length*.02
        for _ in range(256):
            points[-1]=target.copy()
            for i in range(len(points)-2,-1,-1):
                delta=points[i]-points[i+1]
                points[i]=points[i+1]+delta.normalized()*ls[i]
            points[0]=hip.copy()
            for i in range(1,len(points)):
                delta=points[i]-points[i-1]
                points[i]=points[i-1]+delta.normalized()*ls[i-1]
            if (points[-1]-target).length<1e-6:break
        if (points[-1]-target).length>2e-5:return None
        return points

    report={'species':a.species,'source_sha256':hashlib.sha256(a.model.read_bytes()).hexdigest(),
            'original_clips':[x.name for x in original_actions],'authored_clips':[]}
    for running in (False,True):
        count=36 if running else 48
        stance=.40 if running else .64
        stride=leg_length*(2.5 if running else 1.8)
        lift=leg_length*(.20 if running else .12)
        name=spec['asset']+('_FieldRun' if running else '_FieldWalk')
        frames=[]
        for attempt in range(16):
            frames=[];failed=False
            for f in range(count+1):
                phase=f/count;cycle=math.tau*phase
                if cfg['hover']:
                    bind_action(bpy,idle)
                    idle_frame=float(idle.frame_range[0])+phase*float(idle.frame_range[1]-idle.frame_range[0])
                    scene.frame_set(int(idle_frame),subframe=idle_frame%1)
                    source={b.name:b.matrix.copy() for b in bones}
                    # Use one source cycle with a deliberate forward lean.
                    change=Matrix.Translation((.012*math.sin(cycle),0,.018*math.sin(cycle)))
                    change=change@Matrix.Rotation(.14 if running else .07,4,'X')
                    desired={n:change@m for n,m in source.items()}
                    frames.append(desired);continue
                desired={};leg_matrices={}
                offsets=[0,.5,.5,.0] if running else [0,.5,.75,.25]
                for b in bones:
                    n=b.name
                    matrix=desired[b.parent.name]@local[n] if b.parent else base[n].copy()
                    if n==root:
                        matrix.translation+=Vector((leg_length*.012*math.sin(cycle),0,
                            -leg_length*.12+leg_length*.025*(1-math.cos(2*cycle))))
                        matrix=rotate(matrix,.055 if running else .018,'X')
                    elif n==names[cfg['head']]:matrix=rotate(matrix,-.025 if running else -.01,'X')
                    elif n==names[cfg['tail']]:
                        matrix=rotate(matrix,.065*math.sin(cycle-.8),'Z')
                        matrix=rotate(matrix,.04*math.sin(2*cycle-.6),'X')
                    elif n in [names[x] for x in cfg['arms']]:
                        side=cfg['arms'].index(int(n.split('_')[1]))
                        matrix=rotate(matrix,(1 if side==0 else -1)*(.40 if running else .23)*math.cos(cycle),'X')
                    for index,chain in enumerate(chains):
                        if n!=chain[0]:continue
                        forward,height,_=foot_path((phase+offsets[index])%1,stride,stance,lift)
                        target=targets[index]+Vector((0,-forward,height))
                        points=solve(chain,lengths[index],matrix.translation,target)
                        if points is None:failed=True;break
                        for k,joint in enumerate(chain):
                            m=base[joint].copy()
                            if k<len(chain)-1:
                                original=base[chain[k+1]].translation-base[joint].translation
                                m=original.rotation_difference(points[k+1]-points[k]).to_matrix().to_4x4()@m
                            m.translation=points[k];leg_matrices[joint]=m
                    if failed:break
                    desired[n]=leg_matrices.get(n,matrix)
                if failed:break
                frames.append(desired)
            if not failed:break
            stride*=.82;lift*=.92
        if len(frames)!=count+1:raise ValueError('No reachable planted gait for '+spec['name'])
        # Source hover cycles can end in slightly different poses. Ease the
        # final quarter into the first pose so the travelling loop has no pop.
        if cfg['hover']:
            for f in range(count*3//4,count+1):
                t=(f-count*.75)/(count*.25);t=t*t*(3-2*t)
                for n in frames[f]:frames[f][n]=frames[f][n].lerp(frames[0][n],t)
        action=bpy.data.actions.new(name);action.use_fake_user=True
        rig.animation_data.action=action
        rig.animation_data.action_slot=action.slots.new(id_type='OBJECT',name=rig.name)
        action['stride_model_units']=stride;action['authored_locomotion']=True
        error=length_error=0.;seam=[]
        for f,desired in enumerate(frames):
            scene.frame_set(f)
            for b in bones:
                rest=b.bone.matrix_local
                parent_space=desired[b.parent.name]@b.parent.bone.matrix_local.inverted()@rest if b.parent else rest
                # Preserve exact hidden prop scales; never invert their matrices.
                b.matrix_basis=parent_space.inverted_safe()@desired[b.name] if (cfg['hover'] or b.name in controlled) else basis[b.name]
                b.keyframe_insert('location',frame=f,group=b.name)
                path='rotation_quaternion' if b.rotation_mode=='QUATERNION' else 'rotation_axis_angle' if b.rotation_mode=='AXIS_ANGLE' else 'rotation_euler'
                b.keyframe_insert(path,frame=f,group=b.name);b.keyframe_insert('scale',frame=f,group=b.name)
            bpy.context.view_layer.update()
            for chain,ls in zip(chains,lengths):
                error=max(error,(rig.pose.bones[chain[-1]].matrix.translation-desired[chain[-1]].translation).length)
                for i,l in enumerate(ls):
                    length_error=max(length_error,abs((rig.pose.bones[chain[i+1]].matrix.translation-rig.pose.bones[chain[i]].matrix.translation).length-l))
            if f in (0,count):seam.append({b.name:[v for row in b.matrix for v in row] for b in bones})
        seam_error=max(abs(x-y) for n in seam[0] for x,y in zip(seam[0][n],seam[1][n]))
        if max(error,length_error,seam_error)>0.0002:raise ValueError(f'Gait validation failed {error} {length_error} {seam_error}')
        for layer in action.layers:
            for strip in layer.strips:
                for bag in strip.channelbags:
                    for curve in bag.fcurves:
                        for point in curve.keyframe_points:point.interpolation='LINEAR'
        item=dict(name=name,frames=count+1,stride_model_units=stride,max_foot_error=error,max_length_error=length_error,seam_error=seam_error)
        report['authored_clips'].append(item);print(json.dumps(item),flush=True)
    bind_action(bpy,idle);scene.frame_set(int(idle.frame_range[0]))
    a.out.mkdir(parents=True,exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str((a.out/'model.blend').resolve()))
    (a.out/'locomotion.json').write_text(json.dumps(report,indent=2)+'\n')
    bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.wm.quit_blender()


if __name__=='__main__':main()
