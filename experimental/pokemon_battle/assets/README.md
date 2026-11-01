# Original model intake — CP2

This continues the combat experiment from Xbox **.749**, commit
`8b1a9bc011032b32b5e123af8d76ba644b28a971`.
It adds an offline, repeatable asset import step. It does not change the UWP
build, bind a Twilight Princess actor, or introduce an in-game Pokemon renderer.

## Verified input

The user's two Colosseum RAR volumes and three XD RAR volumes extracted completely.
They contained RVZ images, not raw ISO files. `nodtool` verified both reconstructed
discs against its bundled Redump database:

| Game | ID | Revision | Disc SHA-1 |
| --- | --- | --- | --- |
| Colosseum USA | GC6E01 | 0 | `96b2b686d202a13dd15261fb7cffbcb2f08c0a77` |
| XD USA | GXXE01 | 0 | `c1b5218f832403d15aa500ac4d6aacc8865c792d` |

`intake.json` records source hashes, tool versions, extracted counts, and the
selected model. There are 527 `pkx_*.fsys` archives in Colosseum and 603 in XD.
These counts include other battle models and variants; they are **not species counts**.
The Pikachu model and multiple Pikachu move-effect archives exist in both games.
Move effects have not been converted by this checkpoint.

## First imported model

`files/pkx_pikachu.fsys` from each disc imported into Blender 4.5.7 with:

- 27 mesh objects, 660 stored vertices, and 913 triangles;
- one 50-bone skeleton, up to three bone influences per vertex;
- 14 decoded texture images/atlases;
- 15 named animation clips, including idle, physical/special attacks, damage,
  faint, blink, and sleep-related material subanimations.

The importer validates joint references, nonnegative normalized skin weights,
texture data, and finite evaluated geometry at the beginning, middle, and end of
every clip. It binds material slots along with the skeleton so facial texture
animation is retained in the Blender source asset. This sampled check is not
an exhaustive comparison with original game playback.

The addon reported zero import errors, 44 leniencies, and a summary warning:
16 degenerate-face removals, 27 missing-vertex-color defaults, and one near-zero
bone repair. The full log accompanies each private model. These conversions
must not be described as bit-exact or as verified in the Dusklight renderer.

The XD proof video renders idle, physical attack, damage, and faint in sequence:
67 frames at 12 fps, sampling the original 60 fps timeline. Studio lighting comes
from Blender Cycles. The preview is an asset render, not Xbox gameplay footage.
Shiny variants and move particles are not part of this validation.

## Reproduce

Use the user's image locally. Keep disc images, extracted assets, logs containing
model details, and generated Blender files outside this source checkout.
The checked-in code contains no retail model bytes or external addon implementation.

1. Obtain [`nodtool` v2.0.0-alpha.12](https://github.com/encounter/nod/releases/tag/v2.0.0-alpha.12).
   The tested Linux x86-64 binary SHA-256 is
   `ee8345c004248eb82a86dd0c6068e0fcf3dc75baf68d18fe949bf4589822f5a5`.
2. Verify and extract the image:

   ```sh
   nodtool verify /path/to/game.rvz
   nodtool extract /path/to/game.rvz /path/to/extracted
   ```

3. Clone [StarsMmd's Blender GameCube addon](https://github.com/StarsMmd/Blender-Addon-Gamecube-Models)
   and check out `dc71de837978a6a2fe8c5082f2840cfafb135235`. The wrapper rejects
   a different revision or tracked modifications. This is an external GPL-3.0
   development tool, not a linked UWP dependency.
4. Create a Python 3.11 environment and install `requirements.txt` from this
   directory. With that environment's Python:

   ```sh
   python import_model.py --source /path/to/extracted/files/pkx_pikachu.fsys \
     --addon /path/to/addon --out /path/to/new-model-output
   python preview_model.py --model /path/to/new-model-output/model.blend \
     --out /path/to/new-preview-output --video
   ```

   `--video` also requires `ffmpeg`. Output directories must not already exist.
   The import writes a packed `model.blend`, a machine-readable `model.json`,
   and `import.log`. It cleans up Blender datablocks before shutting down; this
   avoids a bpy interpreter-finalization stall observed during initial testing.

## Runtime work still required

The `.blend` file is the editable source asset. Dusklight does not load it.
The next step must choose and implement the native rendering representation,
preserving skinning, material visibility, texture animation, scale, and clip timing.
Then bind a regular enemy actor to the battle session, handle camera/input ownership
and rewards, register the static UWP module, and test a signed Xbox package.
Bosses/minibosses remain outside the randomization scope. Capture and the six-member
party remain follow-on gameplay work.

## Light Pikachu refinement

The requested modest visual update is implemented by `refine_pikachu.py` for both
CP2 Pikachu imports. This is a species-specific preset; do not apply its named
bone and attachment rules to other models without inspecting their geometry.

- Join the six body draw batches and weld 87 duplicate vertices after verifying
  their skin weights agree.
- Subdivide once, retaining original UV/skin interpolation, and blend the belly
  and limbs 65% toward the rounded surface. The head, ear connections, facial
  cards, and rear stripe attachment region retain their original surface.
- Transfer the game's authored normals to avoid shading seams at the face cards.
  Ear and lightning-tail geometry retains its sharp shape.
- Resample all 14 packed texture images to twice their width and height. Atlas
  layout and normalized UV animation stay intact. This is resampling of the
  supplied art, not newly painted detail or a generative texture replacement.
- Verify that the rig hierarchy/rest matrices and all bone/material animation
  curves have the same hash before and after refinement.

Each refined model has 2,061 vertices and 3,833 triangles (the source has 660 and
913). Skin weights remain normalized, with at most four influences and no weight
discarded in either tested import. Total texture pixels increase from 50,688 to
202,752. This asset cost has not yet been benchmarked inside Dusklight on Xbox.

```sh
python refine_pikachu.py --model /path/to/original/model.blend \
  --out /path/to/new-refined-output
python compare_models.py --original /path/to/original/model.blend \
  --refined /path/to/new-refined-output/model.blend --out /path/to/comparison.png
```

The comparison renders both assets under the same orthographic camera and
directional lighting. `--view back` produces a rear comparison for checking the
stripe and tail attachments. Use `preview_model.py` for an animation preview.
The updated private package contains both original and refined assets; no game
or model bytes are added to this repository.

## Native runtime conversion (CP3)

`export_runtime.py` converts either refined Pikachu into the private DPKM format.
It omits hidden helper meshes and samples original geometry and facial UV tracks.
The production C++ loader can export poses through `runtime/snapshot.cpp`;
`preview_runtime.py` renders those decoded poses for visual QA. See
[the native preview actor, format and limitations](../runtime/README.md).
