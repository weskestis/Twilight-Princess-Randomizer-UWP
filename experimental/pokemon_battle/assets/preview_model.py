#!/usr/bin/env python3
"""Render a real imported model and a short animation proof with Blender on CPU.

Outputs use preview lighting, not the original GameCube or Dusklight renderer.
Requires bpy 4.5.7, a model.blend from import_model.py, and ffmpeg for video.
"""
import argparse
import json
import math
from pathlib import Path
import shutil
import subprocess
import tempfile

from import_model import bind_action, sample_geometry


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--model', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path, help='New preview output directory')
    parser.add_argument('--video', action='store_true')
    parser.add_argument('--clips', nargs='+', default=['Idle', 'Physical_B', 'Damage', 'Faint'])
    args = parser.parse_args()
    out = args.out.resolve()
    if out.exists():
        parser.error('Choose a new output directory')
    if args.video and not shutil.which('ffmpeg'):
        parser.error('ffmpeg is required for --video')
    import bpy
    from mathutils import Vector
    if bpy.app.version != (4, 5, 7):
        parser.error('Requires Blender/bpy 4.5.7')
    bpy.ops.wm.open_mainfile(filepath=str(args.model.resolve()), load_ui=False, use_scripts=False)
    clips = []
    for name in args.clips:
        matches = [a for a in bpy.data.actions if a.name.endswith('_' + name)]
        if len(matches) != 1:
            parser.error(f'Expected exactly one animation matching {name!r}')
        clips.append(matches[0])
    scene = bpy.context.scene
    bounds = []
    for action in clips:
        bind_action(bpy, action)
        lo, hi = action.frame_range
        for frame in (lo, (lo + hi) / 2, hi):
            scene.frame_set(math.floor(frame), subframe=frame % 1)
            bounds.extend(sample_geometry(bpy))
    lower = Vector(tuple(min(p[i] for p in bounds) for i in range(3)))
    upper = Vector(tuple(max(p[i] for p in bounds) for i in range(3)))
    center = (lower + upper) / 2
    diameter = max((Vector(p) - center).length for p in bounds) * 2
    scene.world = bpy.data.worlds.new('Preview studio')
    scene.world.use_nodes = True
    bg = scene.world.node_tree.nodes['Background']
    bg.inputs['Color'].default_value = (0.055, 0.065, 0.085, 1)
    bg.inputs['Strength'].default_value = 0.6
    camera = bpy.data.objects.new('Preview camera', bpy.data.cameras.new('Preview camera'))
    scene.collection.objects.link(camera)
    camera.location = center + Vector((0.9, -1.7, 0.65))
    camera.rotation_euler = (center - camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera.data.type, camera.data.ortho_scale = 'ORTHO', diameter * 1.18
    scene.camera = camera
    for name, pos, energy, size in [('Key', (-1, -2, 3), 100, 3),
                                  ('Fill', (2, -1, 1), 45, 2), ('Rim', (0, 2, 2), 110, 2)]:
        obj = bpy.data.objects.new(name, bpy.data.lights.new(name, 'AREA'))
        scene.collection.objects.link(obj)
        obj.location = center + Vector(pos)
        obj.rotation_euler = (center - obj.location).to_track_quat('-Z', 'Y').to_euler()
        obj.data.energy, obj.data.shape, obj.data.size = energy, 'DISK', size
    scene.render.engine = 'CYCLES'
    scene.render.threads_mode, scene.render.threads = 'FIXED', 8
    scene.cycles.device, scene.cycles.use_denoising = 'CPU', True
    scene.render.resolution_x = scene.render.resolution_y = 900
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.view_settings.view_transform = 'Standard'
    out.mkdir(parents=True)
    bind_action(bpy, clips[0])
    scene.frame_set(1)
    video_location = camera.location.copy()
    video_rotation = camera.rotation_euler.copy()
    video_scale = camera.data.ortho_scale
    idle_points = sample_geometry(bpy)
    idle_min = Vector(tuple(min(p[i] for p in idle_points) for i in range(3)))
    idle_max = Vector(tuple(max(p[i] for p in idle_points) for i in range(3)))
    idle_center = (idle_min + idle_max) / 2
    camera.location = idle_center + Vector((0.9, -1.7, 0.65))
    camera.rotation_euler = (idle_center - camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera.data.ortho_scale = max(idle_max - idle_min) * 1.4
    scene.cycles.samples = 32
    scene.render.filepath = str(out / 'Pikachu_CP2_Preview.png')
    bpy.ops.render.render(write_still=True)
    if not args.video:
        return
    camera.location, camera.rotation_euler, camera.data.ortho_scale = video_location, video_rotation, video_scale
    scene.render.resolution_x = scene.render.resolution_y = 640
    scene.cycles.samples = 8
    rendered, segments = 0, []
    with tempfile.TemporaryDirectory(prefix='pokemon-preview-') as temp:
        for action in clips:
            bind_action(bpy, action)
            first = rendered
            # Source animation runs at 60 fps. Sample at 12 fps without changing duration.
            lo, hi = action.frame_range
            for frame in range(math.floor(lo), math.ceil(hi) + 1, 5):
                scene.frame_set(frame)
                scene.render.filepath = str(Path(temp) / f'{rendered:04d}.png')
                bpy.ops.render.render(write_still=True)
                rendered += 1
            segments.append({'clip': action.name, 'first_frame': first, 'last_frame': rendered - 1})
            print(f'Rendered {action.name}: {rendered - first} frames', flush=True)
        subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-framerate', '12',
            '-i', str(Path(temp) / '%04d.png'), '-c:v', 'libx264', '-crf', '18',
            '-pix_fmt', 'yuv420p', '-movflags', '+faststart', str(out / 'Pikachu_CP2_Preview.mp4')], check=True)
    (out / 'preview.json').write_text(json.dumps({'fps': 12, 'source_fps': 60,
        'rendered_frames': rendered, 'segments': segments,
        'renderer': 'Blender Cycles CPU; studio lighting; not an in-game capture'}, indent=2) + '\n')
    print(f'Preview saved: {out}', flush=True)


if __name__ == '__main__':
    main()
