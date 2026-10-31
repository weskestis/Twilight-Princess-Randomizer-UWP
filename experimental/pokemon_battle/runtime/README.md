# CP3 runtime model and native preview actor

Base remains Xbox **.749**, control commit `8b1a9bc011032b32b5e123af8d76ba644b28a971`.
Game ABI: Dusklight `422d7bb1b6c8d973cccf8b3d0b226a57ac3cc8c7`;
GX ABI: its Aurora submodule `d0933b745abe0eb9815bedcea8047575da18698d`.

## Implemented

- A C++ DPKM loader with checked sizes, indices, draw batches, textures, names,
  finite numbers, normal lengths, animation budgets and CRC-32. Failed loads
  preserve the previous model. No reinterpret-casting unaligned file structs.
- Animation playback with interpolated positions/normals and stepped material
  UV transforms. Atlas frames never blend through unrelated facial expressions.
  Non-looping clips hold their final pose; idle loops. Large/invalid times are
  handled explicitly.
- Native GX texture creation, opaque depth-tested drawing, texture release, and
  J3D packet submission. Draw packets reset J3D/GX state around custom drawing.
- A real `ActorService` profile and Mods panel in `../model_lab`: load a private
  `.dpkm` through FileService, spawn on nearby ground in front of Link, select
  clips, and remove it. The actor holds its asset until destruction. Cutscenes
  and pending transitions block spawning and remove the preview. Engine room
  cleanup and mod profile unregistration own destruction. No ordinary enemy is
  replaced at this checkpoint.

The desktop module **builds and links** with the pinned minimal SDK on Linux.
The native actor translation unit compiles against real game headers. These are
not an executed game, GPU capture, Xbox package, or proof of hardware behavior.
No signed static UWP registration or release workflow change is included.

## Private asset results

Both refined sources produced a 31,304,388-byte pack (about 29.9 MiB):
2,032 render vertices, 3,749 visible triangles, 15 draw batches, 14 original
texture atlases plus a white fallback, and all 15 clips / 1,218 source frames.
The source Blender file still contains its editable 50-bone rig and curves.
Seven hidden helper meshes (84 triangles) are intentionally omitted; render
vertices also split on UV/normal seams. These counts therefore differ from the
complete source-model inventory.

The first conversion included hidden helper boxes; a render through the C++
loader exposed that error. The final packs omit them. Never use the scratch
`pikachu-runtime` trial outputs; the delivered packs use `pikachu-runtime-final`.
Maximum final position quantization error was approximately `1.98e-5` model
units (about `0.00198` game units at the preview's 100x unit conversion).

This prototype trades memory for dependable conversion: it bakes full poses at
60 Hz, using 12 bytes per vertex/frame, with one owned blob and one current pose.
Two full blobs can temporarily coexist while validating a replacement, plus
textures. Do not scale this representation to an entire roaming roster without
profiling and a streaming or skeletal implementation. Animation updates follow
TP's 30 Hz actor simulation; high-rate presentation interpolation is not added.

Subclips are exported over idle frame zero, not layered over a running skeletal
clip. Move effects, collision, AI, environmental lights/shadows, battle binding,
capture, companions, reward settlement and save changes remain separate work.
The native renderer uses a simple fixed preview light. Blender QA renders use
studio lights and are explicitly **not in-game screenshots**.

## Build and validate

```sh
# Clone/check out the two exact pins above; initialize extern/aurora.
python scripts/test-pokemon-runtime.py --dusk /path/to/dusklight \
  --pack /private/xd/pikachu.dpkm --pack /private/colosseum/pikachu.dpkm
ASAN_OPTIONS=detect_leaks=0 python scripts/test-pokemon-runtime.py \
  --dusk /path/to/dusklight --sanitize

cmake -S experimental/pokemon_battle/model_lab -B build/pokemon-model \
  -DDUSK_DIR=/path/to/dusklight
cmake --build build/pokemon-model
```

The test runner checks the production codec against malformed files, timing and
transactional-load cases; each supplied private pack is sampled at every frame.
A recording GX host verifies real renderer calls, draw coverage and resource
cleanup. It does not execute GPU commands. Public CI uses a synthetic triangle,
not retail assets, on Linux and MSVC. The native actor is compiled, not exercised
by a fake world. LeakSanitizer is disabled only in traced local runs that cannot
inspect `/proc`; address/UB checks remain enabled.

In a desktop development build, load the generated `pokemon_model_lab.dusk`,
open **Pokemon Model Lab**, choose **Load model pack**, then **Spawn beside Link**.
**Next clip**, **Play idle**, and **Remove preview** control the single preview.
This desktop bundle must not be presented as an Xbox-installable module.

## Regenerate private packs and inspect actual C++ output

```sh
/path/to/bpy-python experimental/pokemon_battle/assets/export_runtime.py \
  --model /private/refined/model.blend --out /private/runtime/xd
# Build the root experimental CMake project to get pokemon_model_snapshot.
pokemon_model_snapshot /private/runtime/xd/pikachu.dpkm 11 0.65 /private/damage-pose
/path/to/bpy-python experimental/pokemon_battle/assets/preview_runtime.py \
  --snapshot /private/damage-pose --out /private/damage-pose.png
```

The exporter accepts the inspected Pikachu material subset and fails on unsupported
operations. It samples evaluated geometry and material states, respects hidden
render meshes, converts Blender Z-up to game Y-up, and tiles byte-backed sRGB
textures as GX RGBA8. No retail model data belongs in the source repository.

## DPKM v1 layout

All scalar values are little-endian. No serialized C++ object layouts.

| Record | Fields |
| --- | --- |
| Header | 8-byte `DPKM0001`, u32 total size, u32 CRC-32 of all bytes after header |
| Counts | u32 vertices, indices, batches, textures, clips |
| Quantization | f32 minimum XYZ, f32 span XYZ |
| UVs | two f32 per render vertex |
| Indices | u32, triangles, contiguous batch coverage |
| Batch | u32 first index, count, texture index, flags (must be zero) |
| Texture | u32 width, height, byte count; 4x4 tiled RGBA8 (AR plane then GB), top-left rows |
| Clip | 64-byte NUL-terminated ASCII name, u32 frame count, f32 frames/second |
| Frame geometry | per vertex: 3x u16 position over bounds; 3x i16 unit normal / 32767 |
| Frame materials | per batch: 6x f32 UV affine transform, 4x f32 color |

Each clip contains all of its frames before the next clip header. Frames store
geometry then materials. UVs remain Blender bottom-left until drawing; the native
renderer applies the material transform then flips V once for the GX texture.
The parser rejects trailing bytes. CRC checks accidental corruption, not authenticity.
