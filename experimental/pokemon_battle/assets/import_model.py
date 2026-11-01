#!/usr/bin/env python3
"""Import a user-supplied Pokemon model through the pinned external Blender addon.

Run with Python 3.11 and bpy 4.5.7. No game bytes or addon code are bundled here.
The output is a Blender source asset; it is not a Dusklight runtime model.
"""
import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import shutil
import subprocess
import sys

ADDON_COMMIT = 'dc71de837978a6a2fe8c5082f2840cfafb135235'


def bind_action(bpy, action):
    """Bind the bone and material slots together, preserving facial animation."""
    for slot in action.slots:
        targets = bpy.data.objects if slot.target_id_type == 'OBJECT' else (
            bpy.data.materials if slot.target_id_type == 'MATERIAL' else None)
        if targets is None:
            raise ValueError(f'Unsupported action slot: {slot.target_id_type}')
        target = targets.get(slot.identifier[2:])
        if target is None:
            raise ValueError(f'Missing action target: {slot.identifier}')
        target.animation_data_create()
        target.animation_data.action = action
        target.animation_data.action_slot = slot
        target.animation_data.use_nla = False


def sample_geometry(bpy):
    graph = bpy.context.evaluated_depsgraph_get()
    points = []
    for obj in bpy.context.scene.objects:
        if obj.type != 'MESH':
            continue
        evaluated = obj.evaluated_get(graph)
        mesh = evaluated.to_mesh()
        try:
            points.extend(tuple(evaluated.matrix_world @ v.co) for v in mesh.vertices)
        finally:
            evaluated.to_mesh_clear()
    if not points or any(not math.isfinite(v) for p in points for v in p):
        raise ValueError('Empty or nonfinite evaluated geometry')
    return points


def inspect_scene(bpy):
    scene = bpy.context.scene
    meshes = [o for o in scene.objects if o.type == 'MESH']
    rigs = [o for o in scene.objects if o.type == 'ARMATURE']
    if not meshes or not rigs or not bpy.data.actions:
        raise ValueError('Expected skinned model and animation clips')
    report = {'meshes': [], 'rigs': [], 'textures': [], 'animations': [],
              'validation': 'Skin weights and finite geometry; three poses per clip. Not in-engine validation.'}
    for rig in rigs:
        report['rigs'].append({'name': rig.name, 'bones': len(rig.data.bones)})
    for obj in meshes:
        armatures = [m.object for m in obj.modifiers if m.type == 'ARMATURE' and m.object]
        if len(armatures) != 1:
            raise ValueError(f'{obj.name}: expected one skin modifier')
        bones = set(armatures[0].data.bones.keys())
        if any(g.name not in bones for g in obj.vertex_groups):
            raise ValueError(f'{obj.name}: unknown skin joint')
        max_error, max_influences = 0., 0
        for v in obj.data.vertices:
            weights = [g.weight for g in v.groups]
            if not weights or any(not math.isfinite(w) or w < 0 for w in weights):
                raise ValueError(f'{obj.name}: missing or invalid vertex weights')
            max_error = max(max_error, abs(sum(weights) - 1))
            max_influences = max(max_influences, len(weights))
        if max_error > 1e-4:
            raise ValueError(f'{obj.name}: skin weights do not sum to one')
        obj.data.calc_loop_triangles()
        report['meshes'].append({'name': obj.name, 'vertices': len(obj.data.vertices),
            'triangles': len(obj.data.loop_triangles), 'max_influences': max_influences,
            'max_weight_error': max_error, 'materials': [m.name for m in obj.data.materials]})
    for img in bpy.data.images:
        if img.type != 'IMAGE':
            continue
        # Packed images are lazy-loaded when a saved .blend is reopened.
        if not img.has_data and len(img.pixels):
            _ = img.pixels[0]
        if not img.has_data or min(img.size) < 1:
            raise ValueError(f'Missing texture data: {img.name}')
        report['textures'].append({'name': img.name, 'width': img.size[0], 'height': img.size[1]})
    for action in sorted(bpy.data.actions, key=lambda a: a.name):
        bind_action(bpy, action)
        lo, hi = action.frame_range
        frames = sorted(set([float(lo), float((lo + hi) / 2), float(hi)]))
        samples, poses = [], []
        for frame in frames:
            scene.frame_set(math.floor(frame), subframe=frame % 1)
            points = sample_geometry(bpy)
            poses.append(points)
            samples.append({'frame': frame,
                'min': [min(p[i] for p in points) for i in range(3)],
                'max': [max(p[i] for p in points) for i in range(3)]})
        displacement = max(math.dist(a, b) for pose in poses[1:] for a, b in zip(poses[0], pose)) if len(poses) > 1 else 0.
        report['animations'].append({'name': action.name, 'frame_start': lo, 'frame_end': hi,
            'slots': [s.identifier for s in action.slots], 'sampled_poses': samples,
            'max_sampled_vertex_displacement': displacement})
        action.use_fake_user = True
    idle = next((a for a in bpy.data.actions if a.name.endswith('_Idle')), bpy.data.actions[0])
    bind_action(bpy, idle)
    scene.frame_set(int(idle.frame_range[0]))
    scene.render.fps, scene.render.fps_base = 60, 1.0
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True, help='Extracted .fsys, .pkx or .dat')
    parser.add_argument('--addon', type=Path, required=True, help='Pinned external addon git checkout')
    parser.add_argument('--out', type=Path, required=True, help='New output directory outside the code checkout')
    args = parser.parse_args()
    source, addon, out = (p.resolve() for p in (args.source, args.addon, args.out))
    if not source.is_file() or source.suffix.lower() not in {'.fsys', '.pkx', '.dat'}:
        parser.error('Supply one extracted model file')
    repo = Path(__file__).resolve().parents[3]
    if out == repo or repo in out.parents:
        parser.error('Keep model assets outside the code repository')
    if out.exists():
        parser.error('Output directory already exists; choose a new directory')
    revision = subprocess.check_output(['git', '-C', str(addon), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != ADDON_COMMIT:
        parser.error(f'Expected addon commit {ADDON_COMMIT}, got {revision}')
    if subprocess.check_output(['git', '-C', str(addon), 'status', '--porcelain', '--untracked-files=no'], text=True).strip():
        parser.error('Addon has tracked modifications')
    import bpy
    if bpy.app.version != (4, 5, 7):
        parser.error('This checkpoint is verified with Blender/bpy 4.5.7 only')
    spec = importlib.util.spec_from_file_location('gamecube_dat_model', addon / '__init__.py', submodule_search_locations=[str(addon)])
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    module.register()
    from gamecube_dat_model.importer.importer import Importer
    from gamecube_dat_model.shared.helpers.logger import Logger
    bpy.ops.wm.read_factory_settings(use_empty=True)
    logger = Logger(model_name=source.stem)
    options = dict(ik_hack=True, max_frame=100000, section_names=[], filepath=str(source),
        game='COLO_XD', colo_xd_kind='PKX_POKEMON', include_shiny=False, import_cameras=False, import_lights=False)
    status = Importer.run(bpy.context, source.read_bytes(), source.name, options, logger)
    if status != {'FINISHED'} or logger.error_count:
        raise RuntimeError('Model import failed; see importer log')
    report = inspect_scene(bpy)
    report.update({'schema': 1, 'source_name': source.name,
        'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
        'addon_commit': revision, 'blender_version': bpy.app.version_string,
        'import_warnings': logger.warning_count, 'import_errors': logger.error_count,
        'import_leniencies': logger.leniency_categories,
        'runtime_ready': False, 'shiny_variant_enabled': False})
    out.mkdir(parents=True)
    for img in bpy.data.images:
        if img.type == 'IMAGE':
            img.pack()
    bpy.ops.wm.save_as_mainfile(filepath=str(out / 'model.blend'))
    shutil.copyfile(logger.log_path, out / 'import.log')
    (out / 'model.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'out': str(out), 'meshes': len(report['meshes']),
        'bones': sum(r['bones'] for r in report['rigs']), 'clips': len(report['animations']),
        'warnings': logger.warning_count, 'runtime_ready': False}), flush=True)
    # Release imported datablocks and callbacks while Blender is still alive.
    # Leaving them to Python's interpreter finalizer can stall the bpy wheel.
    bpy.ops.wm.read_factory_settings(use_empty=True)
    module.unregister()
    import gc
    gc.collect()
    bpy.ops.wm.quit_blender()


if __name__ == '__main__':
    main()
