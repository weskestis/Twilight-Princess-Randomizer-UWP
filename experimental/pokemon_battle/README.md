# Pokemon Battle CP1

Experimental turn-based combat work starts from **Xbox .749**, control commit
`8b1a9bc011032b32b5e123af8d76ba644b28a971`, on `codex/pokemon-battle-cp1`.
The production .749 workflow, application identity, and gameplay patches are unchanged.

## Current checkpoint

- A real, compiled C++ combat core: introduction, player choice, player action,
  enemy action, victory/defeat/escape, and completion.
- Link commands: Sword, Bow, Bomb, Guard, Flee. Ammunition is consumed only after
  a legal command is accepted. Guard and stat reductions affect damage.
- Enemy selection from a weighted move pool with finite PP, deterministic seeded
  randomness, and an exhausted-pool attack. The Pikachu sample uses ThunderShock,
  Quick Attack, Growl, and Tail Whip.
- Every presentation has an increasing identifier. Only its matching completion
  can advance the battle; old callbacks and commands during another action are ignored.
- A native Dusklight UI-service panel, with action availability and a manual
  Continue button. It is a **simulation**, with isolated test HP/ammunition.
- A separately tested host-session contract: world acquisition/release, scene
  invalidation, bounded presentation/settlement waits, and idempotent settlement IDs.
  The concrete Twilight Princess host implementation is **not implemented**.

This is **not an installable Xbox MSIX or a working overworld Pokemon encounter**.
The native panel is compiled and exercised against the upstream SDK headers used
by .749, using a test UI host. No live renderer, controller, Xbox package, or
3D Pokemon model has been tested. CP1 does not modify a game save or grant actual
randomizer rewards. The authored demo balance is not the Colosseum battle formula;
type effectiveness, accuracy, speed priority, abilities, and full move effects
are future work.

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

`native/CMakeLists.txt` follows the upstream mod SDK's `add_mod` interface for
desktop development. Xbox requires additional static registration, unique
metadata sections/context names, packaging, and signed-host linking. A desktop
module cannot simply be copied into Xbox's mods directory. Those additions and
the full .749 host build are **not claimed by these tests**.

## Next playable encounter

1. Obtain a Colosseum/XD disc dump or extracted FSYS/PKX model archives supplied
   by the user. Convert and visually verify one model, its textures/skin weights,
   and the required animation clips. Keep asset provenance separate from code.
2. Bind an eligible ordinary enemy spawn to the Pokemon actor. Preserve the
   user's rule that bosses/minibosses remain vanilla unless changed explicitly.
3. Implement `Host` using .749's actual actor generation, input, pause, camera,
   and save state. Block incompatible cutscenes, transitions, pickups, multiplayer,
   and existing encounters. Restore only the state the encounter owns.
4. Commit victory rewards/room-clear flags once through the existing randomizer
   grant mechanism. Settlement must be idempotent by persistent encounter/check
   identity; the interface alone cannot make a game reward atomic.
5. Connect model animation completion to event acknowledgements, test defeat,
   escape, resets, and transitions, then register/link/package the static UWP module.
6. Verify the signed Xbox build on hardware before calling the encounter playable.

## Capturing and companions — accepted follow-on direction

The user wants a Poké Ball item that captures enemy Pokemon, a maximum carried
party of **six**, and throwing captured Pokemon out to fight other enemies.
The initial request also says **“catch three”**. Its exact meaning is unresolved
(possibly “catch these”); no three-captures-per-ball rule is implemented or locked in.
Keep capture capacity/charges separate from party capacity when implementing it.
This checkpoint intentionally has no nonfunctional capture/party buttons.

Captured identity, move pool, health, ownership, recall, and party persistence
will need a versioned save extension and progression rules. Capture versus defeat
must not duplicate first-defeat rewards or strand room-clear checks. Enemy Souls
integration must preserve the existing no-self-lock logic.

## Uploaded source references

The user's Colosseum and XD source ZIPs were inspected, without executing their
build scripts. `source-intake.json` records archive hashes, counts, and useful
source locations. They contain no `.fsys`, `.pkx`, `.dat`, disc image, or exported
3D model files. Their code explains formats/behavior; it is not the missing model
asset pack. The Colosseum README's matching claims were not independently rebuilt.
No source from either archive has been copied into this module.
