#!/usr/bin/env python3
"""Author in-place Pikachu walk/run cycles on the inspected XD rig.

These are new locomotion animations, not animations extracted from Colosseum.
The two-bone leg solve preserves limb lengths and keeps stance feet planted.
Original battle curves, materials, geometry and textures remain unchanged.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path

from import_model import bind_action

WALK = ('pikachu_15_Walk', 48, 0.28, 0.60, 0.035)
RUN = ('pikachu_16_Run', 36, 0.40, 0.38, 0.050)


def foot_path(phase, stride, stance, lift):
    if phase < stance:
        return stride * (stance / 2 - phase), 0.0, 0.0
    u = (phase - stance) / (1 - stance)
    # Hermite return matches the stance velocity at both boundaries. The foot
    # decelerates after toe-off and reaches back before landing.
    a, b, tangent = -stride * stance / 2, stride * stance / 2, -stride * (1 - stance)
    forward = (2*u**3-3*u*u+1)*a + (u**3-2*u*u+u)*tangent + (-2*u**3+3*u*u)*b + (u**3-u*u)*tangent
    return forward, lift * math.sin(math.pi*u)**2, 0.22 * math.sin(2*math.pi*u)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--model', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    args = p.parse_args()
    source, out = args.model.resolve(), args.out.resolve()
    repo = Path(__file__).resolve().parents[3]
    if out.exists() or out == repo or repo in out.parents:
        p.error('Choose a new output directory outside the code repository')
    import bpy
    from mathutils import Matrix, Vector
    if bpy.app.version != (4, 5, 7):
        p.error('Requires bpy 4.5.7')
    bpy.ops.wm.open_mainfile(filepath=str(source), load_ui=False, use_scripts=False)
    scene = bpy.context.scene
    originals = sorted(bpy.data.actions, key=lambda a:a.name)
    if len(originals) != 15 or originals[0].name != 'pikachu_00_Idle':
        raise ValueError('Expected the validated 15-clip Pikachu source')
    bind_action(bpy, originals[0])
    scene.frame_set(0)
    rigs = [o for o in scene.objects if o.type == 'ARMATURE']
    if len(rigs) != 1 or len(rigs[0].pose.bones) != 50:
        raise ValueError('Expected the inspected 50-bone Pikachu rig')
    rig = rigs[0]
    bones = list(rig.pose.bones)
    base = {b.name:b.matrix.copy() for b in bones}
    original_basis = {b.name:b.matrix_basis.copy() for b in bones}
    # Derive local transforms from rest space. Inverting the near-zero posed
    # matrices used to hide the Surf prop amplifies floating-point noise and
    # can expose a large red triangle during an otherwise unrelated walk.
    local = {b.name:(b.parent.bone.matrix_local.inverted() @ b.bone.matrix_local @ original_basis[b.name]
                    if b.parent else b.bone.matrix_local @ original_basis[b.name]) for b in bones}
    root = 'Bone_01'
    legs = [('Bone_02','Bone_03','Bone_04','Bone_07_FootLeft'),
            ('Bone_09','Bone_10','Bone_11','Bone_16_FootRight')]
    controlled = {root,'Bone_18_Center','Bone_19','Bone_32','Bone_38','Bone_44','Bone_27','Bone_29',
                  *[n for leg in legs for n in leg[:2]]}
    prop_objects = [bpy.data.objects[f'pikachu_mesh_{i:02d}'] for i in range(2,8)]

    def prop_radius():
        graph = bpy.context.evaluated_depsgraph_get()
        maximum = 0.0
        for obj in prop_objects:
            evaluated = obj.evaluated_get(graph)
            mesh = evaluated.to_mesh()
            points = [evaluated.matrix_world @ v.co for v in mesh.vertices]
            center = sum(points, Vector()) / len(points)
            maximum = max(maximum, max((v-center).length for v in points))
            evaluated.to_mesh_clear()
        return maximum

    hidden_prop_radius = prop_radius()
    report = {'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
              'authored_clips':[], 'original_clips':[a.name for a in originals],
              'method':'Analytical two-bone IK, world-fixed stance feet, alternating arm swing and ear/tail follow-through'}

    def rotate(matrix, angle, axis):
        result = Matrix.Rotation(angle, 4, axis) @ matrix
        result.translation = matrix.translation
        return result

    def align(matrix, original_direction, direction, position):
        rotation = original_direction.rotation_difference(direction).to_matrix().to_4x4()
        result = rotation @ matrix
        result.translation = position
        return result

    for name, frames, stride, stance, lift in (WALK, RUN):
        running = name == RUN[0]
        action = bpy.data.actions.new(name)
        action.use_fake_user = True
        rig.animation_data.action = action
        rig.animation_data.action_slot = action.slots.new(id_type='OBJECT', name=rig.name)
        action['stride_model_units'] = stride
        action['stance_fraction'] = stance
        action['authored_locomotion'] = True
        max_foot_error = max_length_error = max_prop_radius = 0.0
        endpoints = []
        for frame in range(frames+1):
            scene.frame_set(frame)
            phase = frame / frames
            cycle = 2 * math.pi * phase
            desired = {}
            leg_matrices = {}
            for bone in bones:
                n = bone.name
                matrix = (desired[bone.parent.name] @ local[n]) if bone.parent else base[n].copy()
                if n == root:
                    matrix.translation += Vector((0.004*math.sin(cycle), 0, (0.006 if running else 0.004)*(1-math.cos(2*cycle))))
                    matrix = rotate(matrix, 0.025*math.sin(cycle), 'Y')
                elif n == 'Bone_18_Center':
                    matrix = rotate(matrix, (0.20 if running else 0.065) + 0.025*math.sin(2*cycle), 'X')
                    matrix = rotate(matrix, 0.035*math.sin(cycle), 'Z')
                elif n == 'Bone_19':
                    matrix = rotate(matrix, -0.6*(0.20 if running else 0.065), 'X')
                elif n in ('Bone_32','Bone_38'):
                    side = 1 if n == 'Bone_32' else -1
                    matrix = rotate(matrix, side*(0.55 if running else 0.32)*math.cos(cycle), 'X')
                elif n == 'Bone_44':
                    matrix = rotate(matrix, 0.07*math.sin(2*cycle-0.8), 'X')
                    matrix = rotate(matrix, 0.10*math.sin(cycle-0.6), 'Z')
                elif n in ('Bone_27','Bone_29'):
                    matrix = rotate(matrix, 0.06*math.sin(2*cycle-0.5), 'X')
                for side, (upper, lower, ankle, marker) in enumerate(legs):
                    if n != upper:
                        continue
                    foot_phase = (phase + side*0.5) % 1.0
                    forward, height, pitch = foot_path(foot_phase, stride, stance, lift)
                    # Bone_03/10 already deform the foot, and Bone_04/11 and
                    # their children deform its toes. Lock that entire chain,
                    # rather than treating an articulated toe as a shin.
                    anchor = base[upper].translation.copy()
                    anchor.z = base[root].translation.z
                    center_shift = anchor.y-base[lower].translation.y
                    target = base[lower].translation + Vector((0,-forward+center_shift,height))
                    hip = desired[root] @ base[root].inverted() @ anchor
                    original_upper = base[upper].translation-anchor
                    original_lower = base[lower].translation-base[upper].translation
                    l1, l2 = original_upper.length, original_lower.length
                    delta = target - hip
                    distance = delta.length
                    if not abs(l1-l2)+1e-6 < distance < l1+l2-1e-6:
                        raise ValueError(f'{name}/{frame}/{upper}: unreachable foot target {distance}, {l1+l2}')
                    direction = delta.normalized()
                    bend = Vector((0,1,0))
                    bend = (bend-direction*bend.dot(direction)).normalized()
                    along = (l1*l1-l2*l2+distance*distance)/(2*distance)
                    knee = hip + direction*along + bend*math.sqrt(max(0,l1*l1-along*along))
                    matrix = align(base[upper], original_lower, target-knee, knee)
                    foot = rotate(base[lower], pitch, 'X')
                    foot.translation = target
                    leg_matrices[lower] = foot
                    max_length_error = max(max_length_error, abs((knee-hip).length-l1), abs((target-knee).length-l2))
                if n in leg_matrices:
                    matrix = leg_matrices[n]
                desired[n] = matrix
                rest = bone.bone.matrix_local
                parent_space = desired[bone.parent.name] @ bone.parent.bone.matrix_local.inverted() @ rest if bone.parent else rest
                bone.matrix_basis = parent_space.inverted() @ matrix if n in controlled else original_basis[n]
                bone.keyframe_insert('location', frame=frame, group=n)
                rotation_path = 'rotation_quaternion' if bone.rotation_mode == 'QUATERNION' else 'rotation_axis_angle' if bone.rotation_mode == 'AXIS_ANGLE' else 'rotation_euler'
                bone.keyframe_insert(rotation_path, frame=frame, group=n)
                bone.keyframe_insert('scale', frame=frame, group=n)
            bpy.context.view_layer.update()
            max_prop_radius = max(max_prop_radius, prop_radius())
            for side, (upper,lower,ankle,marker) in enumerate(legs):
                phase_foot = (phase+side*0.5)%1
                if phase_foot < stance:
                    forward,_,_ = foot_path(phase_foot,stride,stance,lift)
                    center_shift = base[upper].translation.y-base[lower].translation.y
                    expected = base[marker].translation + Vector((0,-forward+center_shift,0))
                    max_foot_error = max(max_foot_error, (rig.pose.bones[marker].matrix.translation-expected).length)
            if frame in (0,frames):
                endpoints.append({b.name:[v for row in b.matrix for v in row] for b in bones})
        seam_error = max(abs(a-b) for n in endpoints[0] for a,b in zip(endpoints[0][n],endpoints[1][n]))
        if max_foot_error > 1e-4 or seam_error > 1e-4 or max_length_error > 1e-5:
            raise ValueError(f'Locomotion validation failed: feet={max_foot_error}, seam={seam_error}, lengths={max_length_error}')
        if max_prop_radius > max(1e-4, hidden_prop_radius*1.05):
            raise ValueError(f'Hidden Surf prop became visible: {max_prop_radius}, baseline {hidden_prop_radius}')
        for layer in action.layers:
            for strip in layer.strips:
                for bag in strip.channelbags:
                    for curve in bag.fcurves:
                        for point in curve.keyframe_points:
                            point.interpolation = 'LINEAR'
        report['authored_clips'].append({'name':name,'frames':frames+1,'fps':60,'stride_model_units':stride,
            'stance_fraction':stance,'max_stance_foot_error':max_foot_error,
            'max_limb_length_error':max_length_error,'loop_seam_matrix_error':seam_error,
            'max_hidden_prop_radius':max_prop_radius})
        print(json.dumps(report['authored_clips'][-1]), flush=True)
    rig.animation_data.action = None
    for bone in bones:
        bone.matrix_basis = original_basis[bone.name]
    bind_action(bpy, originals[0])
    scene.frame_set(0)
    out.mkdir(parents=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(out/'model.blend'))
    (out/'locomotion.json').write_text(json.dumps(report,indent=2)+'\n')
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.wm.quit_blender()


if __name__ == '__main__':
    main()
