# Combat and model intake validation

Base: Xbox .749 control commit `8b1a9bc011032b32b5e123af8d76ba644b28a971`.
Date: 2026-10-31.

Reproduced locally:

- C++20 compilation of the combat core, session contract, console demo, and
  native UI adapter with warnings treated as errors.
- All **24 battle/session checks** passed, including a **3,000-seed simulation**.
- Native panel tests passed against Dusklight SDK commit
  `422d7bb1b6c8d973cccf8b3d0b226a57ac3cc8c7`: menu lifecycle, available actions,
  victory, escape, cancellation, panel rebuild handles, no idle text churn,
  and a UI builder failure.
- The console autoplay produced a complete victory with the expected sequence:
  introduction, Link's action, enemy action, subsequent actions, and completion.
- The same suite passed AddressSanitizer and UndefinedBehaviorSanitizer.
  LeakSanitizer was disabled because this traced runtime cannot inspect the
  required `/proc` process information. No leak-test pass is claimed.

Commands:

```sh
python scripts/test-pokemon-battle-cp1.py --sdk /path/to/dusklight/sdk
ASAN_OPTIONS=detect_leaks=0 python scripts/test-pokemon-battle-cp1.py --sdk /path/to/dusklight/sdk --sanitize
git diff --check
```

A dedicated workflow runs the suite on Linux and Windows/MSVC. Its actual run
status is authoritative for CI; the local results above do not claim a Windows
or Xbox pass. The unchanged .749 production release workflow is not dispatched
by this experimental branch.

Not validated or implemented: live game/actor hooks, a Pokemon model in Dusklight,
controller behavior on Xbox, capture/companions, save changes, real randomizer
rewards, static Xbox registration, signing, MSIX packaging, or console performance.
The host-session tests use an instrumented fake host, not Twilight Princess.

The two uploaded Pokemon source archives were inspected for files and relevant
source interfaces. Their retail matching builds were not reproduced. Source
archive inspection is not a model-conversion test.

## CP2 model intake

The later uploads supplied complete Colosseum and XD RVZ images in multipart RAR
archives. Both extracted fully and passed `nodtool verify` against its bundled
Redump database. Both Pikachu imports passed the reproducible asset workflow in
`assets/import_model.py`, using the pinned addon and Blender 4.5.7.

Each import has 27 meshes, 660 vertices, 913 triangles, 50 bones, 14 texture images,
and 15 clips. Joint references, normalized nonnegative weights, texture data, and
finite geometry at three sampled poses per clip passed. Maximum skin-weight sum
error for the first XD import was approximately `2.24e-8`, with at most three
influences per vertex. Material-only subanimations need not move vertices.

Both imports reported zero errors and the same 44 documented leniencies plus a
summary warning. `assets/README.md` describes those adjustments. Generated assets
and detailed per-model reports stay outside the source repository.

A 900-pixel still and a 67-frame, 640-pixel animation proof were rendered from the
XD import. Frames spanning idle, attack, damage, and faint were inspected visually.
This validates an offline import, not equivalence with every original game frame,
move-effect rendering, or an Xbox/Dusklight encounter. Model files remain editable
Blender assets; no runtime-loader integration is claimed.
