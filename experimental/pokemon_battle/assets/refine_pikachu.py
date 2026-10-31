#!/usr/bin/env python3
"""Light geometry refinement for the exact CP2 Pikachu Blender source asset.

This is a species-specific offline modelling step, not a general remaster filter
and not a native Dusklight asset exporter. Requires bpy 4.5.7.
"""
import argparse
import hashlib
import json
from pathlib import Path

from import_model import inspect_scene


def hash_animation_and_rig(bpy):
    data = {'rigs': [], 'actions': []}
    for obj in sorted(bpy.data.objects, key=lambda o: o.name):
        if obj.type == 'ARMATURE':
            data['rigs'].append({'name': obj.name, 'bones': [
                {'name': b.name, 'parent': b.parent.name if b.parent else None,
                 'matrix': [list(row) for row in b.matrix_local]}
                for b in obj.data.bones]})
    for action in sorted(bpy.data.actions, key=lambda a: a.name):
        curves = []
        for layer in action.layers:
            for strip in layer.strips:
                for bag in strip.channelbags:
                    for f in bag.fcurves:
                        curves.append({'slot': bag.slot_handle, 'path': f.data_path,
                            'index': f.array_index, 'extrapolation': f.extrapolation,
                            'keys': [{'co': list(k.co), 'left': list(k.handle_left),
                                'right': list(k.handle_right), 'interpolation': k.interpolation}
                                for k in f.keyframe_points]})
        data['actions'].append({'name': action.name,
            'slots': [s.identifier for s in action.slots], 'curves': curves})
    return hashlib.sha256(json.dumps(data, sort_keys=True).encode()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--model', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    source, out = args.model.resolve(), args.out.resolve()
    if out.exists():
        parser.error('Choose a new output directory')
    repo = Path(__file__).resolve().parents[3]
    if out == repo or repo in out.parents:
        parser.error('Keep model assets outside the code repository')
    import bpy
    import bmesh
    from mathutils.kdtree import KDTree
    if bpy.app.version != (4, 5, 7):
        parser.error('Requires Blender/bpy 4.5.7')
    bpy.ops.wm.open_mainfile(filepath=str(source), load_ui=False, use_scripts=False)
    if len(bpy.data.objects) != 28 or 'pikachu_skeleton_0' not in bpy.data.objects:
        parser.error('Expected the unchanged CP2 Pikachu scene')
    fingerprint = hash_animation_and_rig(bpy)
    before = inspect_scene(bpy)
    body_names = [f'pikachu_mesh_{i:02d}' for i in range(17, 23)]
    body = [bpy.data.objects.get(n) for n in body_names]
    if any(o is None or o.type != 'MESH' or len(o.data.materials) != 1 or
           o.data.materials[0].name != 'pikachu_mat_17' for o in body):
        parser.error('Unexpected body layout; this preset is specific to CP2 Pikachu')
    # The GameCube draw batches split a continuous body surface. Join those
    # batches before refinement so shared edges do not shrink independently.
    bpy.ops.object.select_all(action='DESELECT')
    for obj in body:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = body[0]
    bpy.ops.object.join()
    body = bpy.context.view_layer.objects.active
    body.name = 'pikachu_body_refined'
    normals_source = body.copy()
    normals_source.data = body.data.copy()
    bpy.context.scene.collection.objects.link(normals_source)
    original_count = len(body.data.vertices)
    neighbors = KDTree(original_count)
    weights = []
    for vertex in body.data.vertices:
        neighbors.insert(vertex.co, vertex.index)
        weights.append({g.group: g.weight for g in vertex.groups})
    neighbors.balance()
    for vertex in body.data.vertices:
        for _, index, _ in neighbors.find_range(vertex.co, 1e-6):
            if index <= vertex.index:
                continue
            left, right = weights[vertex.index], weights[index]
            if any(abs(left.get(g, 0) - right.get(g, 0)) > 1e-5 for g in left.keys() | right.keys()):
                raise ValueError('Coincident body vertices have different skin weights')
    mesh = bmesh.new()
    mesh.from_mesh(body.data)
    bmesh.ops.remove_doubles(mesh, verts=list(mesh.verts), dist=1e-6)
    mesh.to_mesh(body.data)
    mesh.free()
    welded_count = len(body.data.vertices)
    # Keep open boundaries fixed where separate eyes, mouth, ears and details
    # meet the body. Only the rounded body surface is subdivided.
    mesh = bmesh.new()
    mesh.from_mesh(body.data)
    crease = mesh.edges.layers.float.new('crease_edge')
    for edge in mesh.edges:
        edge[crease] = 1.0 if edge.is_boundary else 0.0
    mesh.to_mesh(body.data)
    mesh.free()
    smooth = body.copy()
    smooth.data = body.data.copy()
    bpy.context.scene.collection.objects.link(smooth)
    for obj, method in [(body, 'SIMPLE'), (smooth, 'CATMULL_CLARK')]:
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        sub = obj.modifiers.new('Light body refinement', 'SUBSURF')
        sub.subdivision_type, sub.levels, sub.render_levels = method, 1, 1
        sub.uv_smooth, sub.boundary_smooth = 'PRESERVE_BOUNDARIES', 'PRESERVE_CORNERS'
        bpy.ops.object.modifier_move_to_index(modifier=sub.name, index=0)
        bpy.ops.object.modifier_apply(modifier=sub.name)
    if len(body.data.vertices) != len(smooth.data.vertices) or (
        [tuple(p.vertices) for p in body.data.polygons] !=
        [tuple(p.vertices) for p in smooth.data.polygons]):
        raise ValueError('Subdivision topology differs; cannot safely blend surfaces')
    # Facial texture cards and ears attach to the rigid head (Bone_19).
    # Refining their underlying surface independently makes visible cracks.
    # Keep the head and rear stripe-card region on the original surface, and
    # blend only the belly/limbs toward the rounded result. Simple subdivision
    # supplies the original UV interpolation and skin weights everywhere.
    head_group = body.vertex_groups['Bone_19'].index
    protected_head, protected_back = 0, 0
    for vertex, rounded in zip(body.data.vertices, smooth.data.vertices):
        head_weight = sum(g.weight for g in vertex.groups if g.group == head_group)
        factor = 0.65
        if head_weight > 0.001:
            factor = 0.
            protected_head += 1
        if vertex.co.y > 0.045 and 0.07 < vertex.co.z < 0.25:
            factor = 0.
            protected_back += 1
        vertex.co = vertex.co.lerp(rounded.co, factor)
    smooth_mesh = smooth.data
    bpy.data.objects.remove(smooth, do_unlink=True)
    bpy.data.meshes.remove(smooth_mesh)
    # Keep the eventual runtime skin within four influences per vertex.
    discarded_weight = 0.0
    for vertex in body.data.vertices:
        groups = sorted([(g.group, g.weight) for g in vertex.groups if g.weight > 0],
                        key=lambda g: g[1], reverse=True)
        discarded_weight = max(discarded_weight, sum(w for _, w in groups[4:]))
        keep, total = groups[:4], sum(w for _, w in groups[:4])
        if total <= 0:
            raise ValueError('Refinement produced an unweighted vertex')
        for group in list(vertex.groups):
            body.vertex_groups[group.group].remove([vertex.index])
        for group, weight in keep:
            body.vertex_groups[group].add([vertex.index], weight / total, 'REPLACE')
    for polygon in body.data.polygons:
        polygon.use_smooth = True
    # The game has authored split normals. Recalculating them independently
    # makes the unchanged facial cards show triangular shading seams. Transfer
    # the source surface normals onto the denser mesh in the rest pose.
    rig = bpy.data.objects['pikachu_skeleton_0']
    rig.data.pose_position = 'REST'
    bpy.context.view_layer.update()
    bpy.ops.object.select_all(action='DESELECT')
    body.select_set(True)
    bpy.context.view_layer.objects.active = body
    transfer = body.modifiers.new('Preserve authored shading', 'DATA_TRANSFER')
    transfer.object = normals_source
    transfer.use_loop_data = True
    transfer.data_types_loops = {'CUSTOM_NORMAL'}
    transfer.loop_mapping = 'POLYINTERP_NEAREST'
    bpy.ops.object.modifier_move_to_index(modifier=transfer.name, index=0)
    bpy.ops.object.modifier_apply(modifier=transfer.name)
    rig.data.pose_position = 'POSE'
    source_mesh = normals_source.data
    bpy.data.objects.remove(normals_source, do_unlink=True)
    bpy.data.meshes.remove(source_mesh)
    # Native Blender asset resampling: preserve atlas layout and normalized UVs.
    # This increases texture resolution without claiming newly authored detail.
    texture_changes = []
    for image in bpy.data.images:
        if image.type != 'IMAGE':
            continue
        width, height = image.size
        image.scale(width * 2, height * 2)
        image.pack()
        texture_changes.append({'name': image.name, 'before': [width, height],
                                'after': list(image.size)})
    after = inspect_scene(bpy)
    if hash_animation_and_rig(bpy) != fingerprint:
        raise ValueError('Refinement changed the original rig or animation curves')
    bounds_delta = max(abs(old_pose[key][axis] - new_pose[key][axis])
        for old_clip, new_clip in zip(before['animations'], after['animations'])
        for old_pose, new_pose in zip(old_clip['sampled_poses'], new_clip['sampled_poses'])
        for key in ('min', 'max') for axis in range(3))
    if bounds_delta > 0.03:
        raise ValueError('Refinement changed sampled model bounds by more than 0.03 model units')
    after.update({'schema': 1, 'source_blend_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
        'preset': 'Pikachu light body refinement and 2x texture resampling',
        'rig_animation_sha256_before_and_after': fingerprint,
        'original_vertices': sum(m['vertices'] for m in before['meshes']),
        'original_triangles': sum(m['triangles'] for m in before['meshes']),
        'welded_body_vertices': original_count - welded_count,
        'body_smoothing_strength': 0.65,
        'protected_head_vertices': protected_head,
        'protected_back_detail_vertices': protected_back,
        'max_discarded_skin_weight': discarded_weight,
        'max_sampled_bounds_delta': bounds_delta,
        'texture_changes': texture_changes, 'runtime_ready': False})
    out.mkdir(parents=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(out / 'model.blend'))
    (out / 'model.json').write_text(json.dumps(after, indent=2) + '\n')
    print(json.dumps({'out': str(out), 'vertices': sum(m['vertices'] for m in after['meshes']),
        'triangles': sum(m['triangles'] for m in after['meshes']),
        'welded_vertices': after['welded_body_vertices'], 'discarded_weight': discarded_weight,
        'rig_and_animations_unchanged': True}), flush=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.wm.quit_blender()


if __name__ == '__main__':
    main()
