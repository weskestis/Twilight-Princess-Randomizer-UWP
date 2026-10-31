#!/usr/bin/env python3
"""Bake the validated Pikachu source to DPKM v1 for the native GX model lab.

This deliberately uses sampled vertex animation, not an invented J3D conversion.
Original editable rig/curves remain in the source .blend. All source frames and
material UV translations are sampled at 60 Hz; normals and positions quantize to
16 bits. No external addon code or retail bytes are included in this script.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import tempfile
import zlib

from import_model import bind_action


def srgb(v):
    return 12.92 * v if v <= 0.0031308 else 1.055 * v ** (1 / 2.4) - 0.055


def material_state(material):
    nodes = material.node_tree.nodes
    textures = [n for n in nodes if n.type == 'TEX_IMAGE']
    mapping = nodes.get('TexMapping_0')
    if len(textures) > 1:
        raise ValueError('Only one texture per material is supported')
    # This exporter intentionally accepts the inspected Pikachu shader subset.
    if textures:
        mix = nodes.get('_n2')
        if not mix or mix.blend_type != 'MIX' or mix.inputs[0].default_value != 1:
            raise ValueError('Unsupported texture combine operation')
        if textures[0].extension != 'EXTEND' or textures[0].interpolation != 'Linear':
            raise ValueError('Expected clamp + linear texture sampling')
        if mapping.vector_type not in ('POINT', 'TEXTURE'):
            raise ValueError('Unsupported UV mapping type')
        loc, rot, scale = [mapping.inputs[n].default_value for n in ('Location', 'Rotation', 'Scale')]
        if abs(rot.x) > 1e-6 or abs(rot.y) > 1e-6:
            raise ValueError('Unsupported out-of-plane UV rotation')
        c, s = math.cos(rot.z), math.sin(rot.z)
        uv = [c * scale.x, -s * scale.y, loc.x, s * scale.x, c * scale.y, loc.y]
        if mapping.vector_type == 'TEXTURE':
            if abs(scale.x) < 1e-8 or abs(scale.y) < 1e-8:
                raise ValueError('Singular texture mapping')
            uv = [c/scale.x, s/scale.x, -(c*loc.x+s*loc.y)/scale.x,
                  -s/scale.y, c/scale.y, (s*loc.x-c*loc.y)/scale.y]
        color = [1., 1., 1., 1.]  # MIX factor=1 selects the texture, not diffuse tint.
    else:
        uv = [1., 0., 0., 0., 1., 0.]
        color = [srgb(max(0., min(1., v))) for v in nodes['DiffuseColor'].outputs[0].default_value[:3]] + [1.]
    alpha = nodes['AlphaValue'].outputs[0].default_value
    if abs(alpha - 1) > 1e-6:
        raise ValueError('This opaque Pikachu profile does not support alpha animation')
    return uv + color


def tiled_rgba(image, np):
    if image is None:
        return 4, 4, bytes([255] * 64)
    width, height = image.size
    if image.is_float or image.colorspace_settings.name != 'sRGB':
        raise ValueError('Expected byte-backed sRGB textures')
    if width % 4 or height % 4 or max(width, height) > 2048:
        raise ValueError('GX texture dimensions must be multiples of four, <= 2048')
    # Byte-backed Blender images expose their encoded byte values through pixels;
    # keep those values. Rows are flipped once to top-left; GX V is flipped below.
    pixels = np.empty(width * height * 4, dtype=np.float32)
    image.pixels.foreach_get(pixels)
    pixels = np.rint(np.clip(pixels.reshape(height, width, 4)[::-1], 0, 1) * 255).astype(np.uint8)
    if np.any(pixels[:, :, 3] != 255):
        raise ValueError('Expected opaque Pikachu textures')
    chunks = []
    for y in range(0, height, 4):
        for x in range(0, width, 4):
            tile = pixels[y:y+4, x:x+4].reshape(16, 4)
            chunks.extend((tile[:, [3, 0]].tobytes(), tile[:, [1, 2]].tobytes()))
    return width, height, b''.join(chunks)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--model', required=True, type=Path)
    p.add_argument('--out', required=True, type=Path)
    args = p.parse_args()
    source, out = args.model.resolve(), args.out.resolve()
    repo = Path(__file__).resolve().parents[3]
    if out.exists() or out == repo or repo in out.parents:
        p.error('Choose a new output directory outside the code repository')
    import bpy
    import numpy as np
    if bpy.app.version != (4, 5, 7):
        p.error('Requires bpy 4.5.7')
    bpy.ops.wm.open_mainfile(filepath=str(source), load_ui=False, use_scripts=False)
    scene = bpy.context.scene
    actions = sorted(bpy.data.actions, key=lambda a: a.name)
    if len(actions) != 15 or not all(a.name.startswith('pikachu_') for a in actions):
        raise ValueError('This checkpoint accepts the validated Pikachu imports only')
    idle = actions[0]
    bind_action(bpy, idle)
    scene.frame_set(0)
    graph = bpy.context.evaluated_depsgraph_get()
    hidden = sorted(o.name for o in scene.objects if o.type == 'MESH' and o.hide_render)
    objects = sorted((o for o in scene.objects if o.type == 'MESH' and not o.hide_render), key=lambda o: o.name)
    slots, base_uv, indices, batches, materials = [], [], [], [], []
    for obj in objects:
        evaluated = obj.evaluated_get(graph)
        mesh = evaluated.to_mesh(preserve_all_data_layers=True, depsgraph=graph)
        mesh.calc_loop_triangles()
        if len(mesh.materials) != 1:
            raise ValueError('Expected one material per source draw batch')
        material = obj.data.materials[0]
        materials.append(material)
        material_state(material)
        start = len(indices)
        uv_layer = mesh.uv_layers.active
        unique, representatives = {}, []
        for triangle in mesh.loop_triangles:
            for loop in triangle.loops:
                vertex = mesh.loops[loop].vertex_index
                uv = tuple(uv_layer.data[loop].uv) if uv_layer else (0., 0.)
                normal = tuple(mesh.corner_normals[loop].vector)
                key = (vertex, *(round(v, 6) for v in normal), *uv)
                if key not in unique:
                    unique[key] = len(base_uv)
                    representatives.append((vertex, loop))
                    base_uv.append(uv)
                indices.append(unique[key])
        batches.append([start, len(indices) - start, 0, 0])
        slots.append((obj, representatives, len(mesh.vertices), len(mesh.loops)))
        evaluated.to_mesh_clear()
    images = [None] + sorted({n.image for m in materials for n in m.node_tree.nodes if n.type == 'TEX_IMAGE'}, key=lambda i:i.name)
    textures = [tiled_rgba(image, np) for image in images]
    for batch, material in zip(batches, materials):
        tex = next((n.image for n in material.node_tree.nodes if n.type == 'TEX_IMAGE'), None)
        batch[2] = images.index(tex)
    lower, upper = np.full(3, np.inf), np.full(3, -np.inf)
    clips, frame_total = [], 0
    # World-space Blender Z-up -> game Y-up, preserving handedness.
    rotation = np.array([[1,0,0],[0,0,1],[0,-1,0]], dtype=np.float64)
    with tempfile.TemporaryDirectory(prefix='pikachu-runtime-') as tmp:
        raw = Path(tmp) / 'frames.f32'
        with raw.open('wb') as stream:
            for action in actions:
                # Subclips are exported over idle frame zero, not a previous clip's last pose.
                bind_action(bpy, idle)
                scene.frame_set(0)
                bind_action(bpy, action)
                lo, hi = action.frame_range
                if lo != int(lo) or hi != int(hi) or hi - lo > 1000:
                    raise ValueError('Unexpected source frame range')
                count = int(hi - lo) + 1
                changed_uv = set()
                for frame in range(int(lo), int(hi) + 1):
                    scene.frame_set(frame)
                    graph = bpy.context.evaluated_depsgraph_get()
                    vertices = []
                    states = []
                    for obj, representatives, nv, nl in slots:
                        evaluated = obj.evaluated_get(graph)
                        mesh = evaluated.to_mesh(preserve_all_data_layers=True, depsgraph=graph)
                        if len(mesh.vertices) != nv or len(mesh.loops) != nl:
                            raise ValueError('Animation changed topology')
                        transform = np.array(evaluated.matrix_world, dtype=np.float64)
                        ntransform = np.linalg.inv(transform[:3,:3]).T
                        for vi, li in representatives:
                            pos = rotation @ (transform[:3,:3] @ np.array(mesh.vertices[vi].co) + transform[:3,3])
                            nrm = rotation @ (ntransform @ np.array(mesh.corner_normals[li].vector))
                            nrm /= np.linalg.norm(nrm)
                            vertices.append([*pos, *nrm])
                        evaluated.to_mesh_clear()
                    for m in materials:
                        state = material_state(m.evaluated_get(graph))
                        states.append(state)
                        changed_uv.add(tuple(state[:6]))
                    array = np.asarray(vertices, dtype='<f4')
                    if not np.isfinite(array).all():
                        raise ValueError('Nonfinite animation sample')
                    lower = np.minimum(lower, array[:,:3].min(axis=0))
                    upper = np.maximum(upper, array[:,:3].max(axis=0))
                    stream.write(array.tobytes())
                    stream.write(np.asarray(states, dtype='<f4').tobytes())
                clips.append({'name':action.name, 'frames':count, 'fps':60, 'uv_states':len(changed_uv)})
                frame_total += count
                print('Baked', action.name, count, flush=True)
        span = np.maximum(upper - lower, 1e-6).astype('<f4')
        lower = lower.astype('<f4')
        body = bytearray(struct.pack('<5I6f', len(base_uv), len(indices), len(batches), len(textures), len(clips), *lower, *span))
        body += np.asarray(base_uv,dtype='<f4').tobytes()
        body += np.asarray(indices,dtype='<u4').tobytes()
        body += np.asarray(batches,dtype='<u4').tobytes()
        for width, height, data in textures:
            body += struct.pack('<3I', width, height, len(data)) + data
        max_error = 0.
        with raw.open('rb') as stream:
            for clip in clips:
                body += struct.pack('<64sIf', clip['name'].encode(), clip['frames'], 60.)
                for _ in range(clip['frames']):
                    values = np.frombuffer(stream.read(len(base_uv)*24),dtype='<f4').reshape(-1,6)
                    pos = np.rint(np.clip((values[:,:3]-lower)/span,0,1)*65535).astype('<u2')
                    normal = np.rint(np.clip(values[:,3:],-1,1)*32767).astype('<i2')
                    packed = np.empty((len(base_uv),6),dtype='<u2')
                    packed[:,:3], packed[:,3:] = pos, normal.view('<u2')
                    body += packed.tobytes()
                    body += stream.read(len(batches)*40)
                    max_error=max(max_error,float(np.abs(pos.astype(np.float64)/65535*span+lower-values[:,:3]).max()))
        data = b'DPKM0001' + struct.pack('<II',len(body)+16,zlib.crc32(body)) + body
    if len(data) > 128*1024*1024:
        raise ValueError('Runtime pack exceeds 128 MiB limit')
    out.mkdir(parents=True)
    (out/'pikachu.dpkm').write_bytes(data)
    report={'schema':1,'format':'DPKM0001','source_blend_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
        'pack_sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),'vertices':len(base_uv),
        'hidden_helper_meshes_omitted':hidden,
        'triangles':len(indices)//3,'batches':len(batches),'textures':len(textures),
        'clips':clips,'total_frames':frame_total,'bounds_min':lower.tolist(),'bounds_max':upper.tolist(),
        'max_position_quantization_error':max_error,
        'runtime_strategy':'60 Hz baked vertices; interpolate geometry, step material UV atlas',
        'limitations':['Standalone subclips bake over idle start; no layered skeletal mixing',
        'Preview lighting; not original GameCube TEV or Twilight Princess environmental lighting',
        'Native game and Xbox runtime not yet exercised']}
    (out/'runtime.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:report[k] for k in ('bytes','vertices','triangles','total_frames','max_position_quantization_error')}),flush=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.wm.quit_blender()


if __name__ == '__main__':
    main()
