# Pokemon Battle — .749 experiment

Experimental turn-based combat work starts from **Xbox .749**, control commit
`8b1a9bc011032b32b5e123af8d76ba644b28a971`, on `codex/pokemon-battle-cp1`.
The production branch still builds .749. The experiment adds an opt-in preview
stage after all .749 source checks, and preserves the inherited gameplay patches.
Private final packaging creates a separate .752 `DusklightPokemonPreview` app,
with its own installation identity and saves/settings, alongside the normal .749 app.

## Current playable milestone

The user confirmed the .751 preview spawns on Xbox and subsequently reported it
seems to work. Specific animation, cleanup, and transition checks were not
individually documented. .752 adds the encounter implementation; hardware
validation of these new features is still required.

- Opt-in real-time encounters with up to three wild Pikachu and one summoned
  companion. Every instance shares the loaded 31 MB model and textures.
- Link's real weapon collisions affect Pokémon HP; Pokémon attacks enter the
  game's collision system. Targets, pauses, cutscenes, room changes, and pending
  actor creation are checked before use. The .751 scene-ownership fix is retained.
- Four randomly selected learned moves with finite PP, windup, strike, recovery,
  damage and faint animations, and an exhausted-PP Struggle attack.
- A visible Poke Ball flight and capture sequence, authored HP-dependent capture
  odds, six individually identified party members, and selected-member recall.
- Per-Zelda-slot party storage through the real SaveService. Explicit portable
  encoding, version and checksum, length/range validation, unique identity checks,
  transactional writes, and session epochs guard save changes and old callbacks.
  The host flushes the staged blob when Zelda is saved; capture is not an autosave.
- Companion combat against wild Pokémon and a conservative ordinary-enemy profile
  allowlist. Zelda enemies retain their native damage and defeat behavior.
- Controller shortcuts plus panel buttons, red/green HP bars, and electric attack
  effects. Practice controls restore HP/PP and ball supplies after an encounter.
- The previous model/clip preview and isolated turn-based CP1 simulation remain.
  The CP1 Host contract is not the live encounter implementation: .752 follows
  the user's original real-time idea and does not acquire a turn-based world lock.

The only imported species is Pikachu. Ordinary enemies are not replaced, bosses
and minibosses retain their vanilla actors, and these added practice encounters
never grant randomizer checks or mark vanilla room-clear flags. Broader enemy
replacement will need idempotent reward settlement and Enemy Souls no-self-lock
logic. Capture is a mod inventory action; no native item-ring slot is installed.
Full Colosseum formulas, type matchups, abilities, roster, original effects/audio,
world/story import, walk cycles, and room lighting remain future work.

The actor reuses the supplied XD Pikachu, original textures/rig/15 clips, and
light refinement from CP2–4. Both Colosseum and XD source assets have also been
processed. See [the asset workflow](assets/README.md) and [the runtime](runtime/README.md).

## Run the code

From the control repository root, with g++ (Linux) or an MSVC developer shell:

```sh
python scripts/test-pokemon-battle-cp1.py --sdk /path/to/dusklight/sdk
```

The SDK checkout must match .749's upstream pin:
`422d7bb1b6c8d973cccf8b3d0b226a57ac3cc8c7` (Dusklight v2.0.1).
The script compiles the actual production CP1 sources, runs battle/session tests,
plays a deterministic demonstration, and tests the native panel through real SDK
types. Omit `--sdk` to run only the core and console demo. Linux accepts
`--sanitize` for AddressSanitizer and UndefinedBehaviorSanitizer. Under tracing
environments where LeakSanitizer cannot access `/proc`, set
`ASAN_OPTIONS=detect_leaks=0`; this disables leak checking, not address/UB checking.

The optional CMake entry point builds the same library, console demo, and tests:

```sh
cmake -S experimental/pokemon_battle -B build/pokemon-battle
cmake --build build/pokemon-battle
ctest --test-dir build/pokemon-battle --output-on-failure
```

The console demo accepts `--autoplay`, or interactively accepts 1–5 and q. Its
output deliberately distinguishes simulated results from actual game rewards.

`native/CMakeLists.txt` builds the combat panel through the desktop SDK. The
Model Lab has a separate Xbox target in `model_lab/CMakeLists.uwp.txt`, staged by
`scripts/stage-pokemon-uwp.py`. The full Xbox workflow checks all 36 independent
signed implementations after linking. Portable unit tests alone do not establish
that the game or its renderer runs correctly on hardware.

## Next integration work

1. Hardware-test the .752 encounter, capture, companion, and same-slot reload loop.
2. Expand the importer/roster and replace baked poses with a scalable shared
   skeletal animation path before adding many species.
3. Connect eligible ordinary-enemy replacement to existing room-clear and
   first-defeat/randomizer reward settlement. Keep bosses/minibosses vanilla.
4. Add authentic battle data, richer world/item integration, and Colosseum maps,
   trainers, Shadow mechanics and story in later milestones.

The earlier phrase “catch three” remains ambiguous; .752 does not assign three
charges per ball. Each throw spends one ball and the carried party has six slots.

## Uploaded source references

The user's Colosseum and XD source ZIPs were inspected, without executing their
build scripts. `source-intake.json` records archive hashes, counts, and useful
source locations. They contain no `.fsys`, `.pkx`, `.dat`, disc image, or exported
3D model files. Their code explains formats/behavior; it is not the missing model
asset pack. The Colosseum README's matching claims were not independently rebuilt.
No source from either archive has been copied into this module. The subsequently
supplied game images contain the missing assets; [CP2](assets/README.md) records
their verification and the first model import.
