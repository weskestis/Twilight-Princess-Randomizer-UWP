"""Exercise the production lighting WGSL with deterministic world color and depth inputs."""

from pathlib import Path
import numpy as np
import wgpu

W, H = 128, 80
adapter = wgpu.gpu.request_adapter_sync()
device = adapter.request_device_sync()
shader = device.create_shader_module(
    code=(
        Path(__file__).resolve().parents[1] / "ports/lighting-756/world_lighting.wgsl"
    ).read_text()
)
print(adapter.info, shader.get_compilation_info_sync(), flush=True)
bind_layout = device.create_bind_group_layout(
    entries=[
        dict(
            binding=0,
            visibility=wgpu.ShaderStage.FRAGMENT,
            buffer={"type": "uniform", "min_binding_size": 896},
        ),
        *[
            dict(
                binding=i,
                visibility=wgpu.ShaderStage.FRAGMENT,
                texture={"sample_type": "unfilterable-float"},
            )
            for i in (1, 2, 3)
        ],
    ]
)
layout = device.create_pipeline_layout(bind_group_layouts=[bind_layout])


def make_pipeline(entry, mask):
    return device.create_render_pipeline(
        layout=layout,
        vertex={"module": shader, "entry_point": "vs"},
        primitive={"topology": "triangle-list"},
        fragment={
            "module": shader,
            "entry_point": entry,
            "targets": [{"format": "rgba8unorm", "write_mask": mask}],
        },
    )


gather = make_pipeline("gather", 15)
composite = make_pipeline("composite", 7)


def texture(w, h, fmt):
    return device.create_texture(
        size=(w, h, 1),
        format=fmt,
        usage=(wgpu.TextureUsage.RENDER_ATTACHMENT if fmt != "r32float" else 0)
        | wgpu.TextureUsage.TEXTURE_BINDING
        | wgpu.TextureUsage.COPY_DST
        | wgpu.TextureUsage.COPY_SRC,
    )


scene = texture(W, H, "rgba8unorm")
z = texture(W, H, "r32float")
uniform = device.create_buffer(
    size=896, usage=wgpu.BufferUsage.UNIFORM | wgpu.BufferUsage.COPY_DST
)
u, v = np.meshgrid((np.arange(W) + 0.5) / W, (np.arange(H) + 0.5) / H)
rx = (2 * u - 1) * 1.6
ry = 1 - 2 * v
far = 1500.0
near = 1.0
d = np.full_like(u, far)
floor = ry < 0
d[floor] = np.minimum(far, -70 / ry[floor])
wall = (np.abs(rx * 250) < 70) & (ry * 250 + 70 < 165) & (ry * 250 + 70 > 0)
pillar = (rx * 140 > 25) & (rx * 140 < 35) & (ry * 140 + 70 < 135) & (ry * 140 + 70 > 0)
d[wall] = np.minimum(d[wall], 250)
d[pillar] = np.minimum(d[pillar], 140)
pixels = np.zeros((H, W, 4), np.uint8)
pixels[:] = [115, 160, 220, 231]
pixels[d < far] = [64, 62, 58, 231]
pixels[wall & (d == 250)] = [182, 54, 30, 231]
pixels[pillar & (d == 140)] = [130, 140, 55, 231]
# Add high frequency detail to detect full-scene smearing.
pixels[::2, ::2, :3] += 6


def write(tex, a):
    device.queue.write_texture(
        {"texture": tex},
        a.tobytes(),
        {"bytes_per_row": W * 4, "rows_per_image": H},
        (W, H, 1),
    )


write(scene, pixels)
base = np.zeros((56, 4), np.float32)
base[0] = [1 / 1.6, 1, 0, 0]
base[1] = [1 / (far - 1), far / (far - 1), near, far]
base[2] = [0, 1, 0, 70]
base[3] = [0.26, 0.22, -0.94, 1]
base[4] = [0.36, 0.45, 0.6, 30]
base[5] = [0.64, 0.72, 0.85, 1]
base[7, 1] = 0.002
base[8] = [1, 0, 0, 0]
base[9] = [0, 0, 1, 0]
base[10] = [-1000, -1000, 640, 1]
base[11] = [8, 16, 1, 12]
base[12] = [-135, -10, -105, 320]
base[13] = [1, 0.4, 0.12, 0]
base[28:32, :] = 12.0


def render(frame, rev=True, low=40):
    frame = frame.copy()
    if not rev:
        frame[1, 0] = -far / (far - 1)
        frame[1, 1] = -far / (far - 1)
    raw = frame[1, 1] / d - frame[1, 0]
    write(z, raw.astype(np.float32))
    device.queue.write_buffer(uniform, 0, frame.tobytes())
    light = texture(int(low * W / H), low, "rgba8unorm")
    out = texture(W, H, "rgba8unorm")
    encoder = device.create_command_encoder()
    for pip, target, extra in [(gather, light, scene), (composite, out, light)]:
        bg = device.create_bind_group(
            layout=bind_layout,
            entries=[
                {
                    "binding": 0,
                    "resource": {"buffer": uniform, "offset": 0, "size": 896},
                },
                *[
                    {"binding": i + 1, "resource": t.create_view()}
                    for i, t in enumerate([scene, z, extra])
                ],
            ],
        )
        p = encoder.begin_render_pass(
            color_attachments=[
                {
                    "view": target.create_view(),
                    "resolve_target": None,
                    "load_op": "clear",
                    "store_op": "store",
                    "clear_value": (0, 0, 0, 231 / 255),
                }
            ]
        )
        p.set_pipeline(pip)
        p.set_bind_group(0, bg)
        p.draw(3)
        p.end()
    device.queue.submit([encoder.finish()])
    result = (
        np.frombuffer(
            device.queue.read_texture(
                {"texture": out},
                {"bytes_per_row": W * 4, "rows_per_image": H},
                (W, H, 1),
            ),
            np.uint8,
        )
        .reshape(H, W, 4)
        .astype(int)
    )
    assert np.all(result[:, :, 3] == 231)
    return result


initial = render(base)
assert np.max(np.abs(initial - pixels.astype(int))) <= 1
print(
    "Zero strengths preserve scene and alpha:",
    np.max(np.abs(initial - pixels.astype(int))),
    flush=True,
)
effects = [
    ("bounce", 6, 0),
    ("local", 6, 1),
    ("refl", 6, 2),
    ("filmic", 6, 3),
    ("fog", 7, 0),
    ("shafts", 7, 2),
    ("contact", 7, 3),
]
all_on = base.copy()
for name, row, col in effects:
    frame = base.copy()
    frame[row, col] = 1
    all_on[row, col] = 1
    result = render(frame)
    difference = result[:, :, :3] - initial[:, :, :3]
    changed = int((abs(difference).max(axis=2) > 1).sum())
    print(name, changed, difference.min(), difference.max(), flush=True)
    assert changed > 10, (name, changed)
    if name == "local":
        assert difference[:, :, 0].sum() > 2 * difference[:, :, 2].sum()
    if name == "refl":
        frame[5, 3] = 0
        frame[11, 2] = 0
        assert np.abs(render(frame) - initial).max() <= 1
    if name == "shafts":
        frame[3, 3] = 0
        assert np.abs(render(frame) - initial).max() <= 1
# A secondary source survives without any direct light in the camera frame.
probe = base.copy()
probe[6, 0] = 1
probe[5, :3] = 0
without = render(probe)
probe[32] = [-40, -25, -130, 260]
probe[33] = [0.9, 0.2, 0.05, 0]
probe[34] = [0, -1, 0, 0]
with_bounce = render(probe)
delta = with_bounce - without
assert delta[:, :, 0].max() > 4 and delta[:, :, 0].sum() > 2 * delta[:, :, 2].sum()
print(
    "World bounce without primary light:",
    (delta[:, :, 0] > 1).sum(),
    "pixels",
    flush=True,
)
for low in (20, 40, 80):
    a = render(all_on, True, low)
    b = render(all_on, False, low)
    print(
        "Resolution and depth convention:",
        low,
        "max difference",
        np.abs(a - b).max(),
        flush=True,
    )
    assert np.abs(a - b).max() <= 3
