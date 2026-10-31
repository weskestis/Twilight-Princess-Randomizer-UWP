#!/usr/bin/env python3
"""Render two model assets together under the same camera and directional lights."""
import argparse
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--original', required=True, type=Path)
    parser.add_argument('--refined', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--view', choices=['front', 'back'], default='front')
    args = parser.parse_args()
    if args.out.exists():
        parser.error('Output already exists')
    import bpy
    from mathutils import Vector
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.frame_set(1)
    direction = Vector((0.9, -1.7 if args.view == 'front' else 1.7, 0.65))
    rotation = (-direction).to_track_quat('-Z', 'Y')
    right = rotation @ Vector((1, 0, 0))
    roots = []
    for path, offset in [(args.original, -0.46), (args.refined, 0.46)]:
        with bpy.data.libraries.load(str(path.resolve()), link=False) as (source, dest):
            dest.objects = list(source.objects)
        for obj in dest.objects:
            if obj is not None:
                scene.collection.objects.link(obj)
        rigs = [o for o in dest.objects if o is not None and o.type == 'ARMATURE']
        if len(rigs) != 1:
            raise ValueError('Expected one rig in each input model')
        roots.append(rigs[0])
        rigs[0].location += right * offset
    scene.frame_set(1)
    bpy.context.view_layer.update()
    # Match the original's framing; the refined model cannot change its own zoom.
    graph = bpy.context.evaluated_depsgraph_get()
    points = [o.evaluated_get(graph).matrix_world @ Vector(c)
              for o in roots[0].children if o.type == 'MESH'
              for c in o.evaluated_get(graph).bound_box]
    lower = Vector(tuple(min(p[i] for p in points) for i in range(3)))
    upper = Vector(tuple(max(p[i] for p in points) for i in range(3)))
    center = (lower + upper) / 2 + right * 0.46
    camera = bpy.data.objects.new('Comparison camera', bpy.data.cameras.new('Comparison camera'))
    scene.collection.objects.link(camera)
    camera.location, camera.rotation_euler = center + direction, rotation.to_euler()
    camera.data.type, camera.data.ortho_scale = 'ORTHO', 1.82
    scene.camera = camera
    scene.world = bpy.data.worlds.new('Comparison background')
    scene.world.use_nodes = True
    bg = scene.world.node_tree.nodes['Background']
    bg.inputs['Color'].default_value = (0.055, 0.065, 0.085, 1)
    bg.inputs['Strength'].default_value = 0.6
    # Directional lights give both models identical illumination despite offset.
    for name, vector, energy in [('Key', (-1, -2, 3), 1.35),
                                  ('Fill', (2, -1, 1), 0.42), ('Rim', (0, 2, 2), 0.7)]:
        light = bpy.data.objects.new(name, bpy.data.lights.new(name, 'SUN'))
        scene.collection.objects.link(light)
        light.rotation_euler = (-Vector(vector)).to_track_quat('-Z', 'Y').to_euler()
        light.data.energy, light.data.angle = energy, 0.4
    text_material = bpy.data.materials.new('Labels')
    text_material.use_nodes = True
    nodes = text_material.node_tree.nodes
    nodes.clear()
    emission, output = nodes.new('ShaderNodeEmission'), nodes.new('ShaderNodeOutputMaterial')
    emission.inputs['Color'].default_value = (0.85, 0.88, 0.94, 1)
    text_material.node_tree.links.new(emission.outputs[0], output.inputs[0])
    for text, x, y, size in [('Original', -0.46, 0.43, 0.052),
                             ('Lightly refined', 0.46, 0.43, 0.052),
                             ('Original game model', -0.46, -0.41, 0.030),
                             ('Smoother body / 2x textures', 0.46, -0.41, 0.030),
                             ('Same 50-bone rig and 15 animation clips', 0, -0.48, 0.028)]:
        font = bpy.data.curves.new(text, 'FONT')
        font.body, font.align_x, font.size = text, 'CENTER', size
        label = bpy.data.objects.new(text, font)
        scene.collection.objects.link(label)
        label.parent = camera
        label.location = (x, y, -1)
        label.data.materials.append(text_material)
    scene.render.engine = 'CYCLES'
    scene.cycles.device, scene.cycles.samples, scene.cycles.use_denoising = 'CPU', 40, True
    scene.render.threads_mode, scene.render.threads = 'FIXED', 8
    scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage = 1600, 1000, 100
    scene.view_settings.view_transform = 'Standard'
    scene.render.image_settings.file_format = 'PNG'
    args.out.parent.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str(args.out.resolve())
    bpy.ops.render.render(write_still=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.wm.quit_blender()


if __name__ == '__main__':
    main()
