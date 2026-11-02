# Pokémon Mode for Xbox .749

The `.754` candidate expands the separately installed `DusklightPokemonPreview`
app to 20 Colosseum species. The opt-in build offers **Pokemon Vanilla** and
**Pokemon Randomizer**, preserving their existing save identifiers and the .753
party format. The ordinary .749 release branch is unaffected.

## Included

- Bulbasaur, Ivysaur, Venusaur; Charmander, Charmeleon, Charizard; Squirtle,
  Wartortle, Blastoise; Pikachu, Raichu; Gastly, Haunter, Gengar; Eevee,
  Vaporeon, Jolteon, Flareon, Espeon, Umbreon.
- A choice of three starters for an empty party, 24 initial balls, six party
  slots, manual encounters and optional movement-triggered encounters near
  supported ordinary enemies. Up to three wild actors and one companion.
- 258 clips, including source idle/attack/hurt/faint/facial clips and authored
  planted-foot walk/run cycles (hover cycles for Gastly and Haunter). Compact
  four-weight skeletal packs replace per-frame copies of mesh vertices.
- Species stats, four learned moves each, finite PP, 37 learned moves plus
  Struggle, Gen III type matchups/STAB and timed status effects. Real-time damage
  enters TP's collision system; companions also fight supported native enemies.
- Link's native one-handed throw animation, release at its hand-release frame,
  curved ball flight, hinged shells, absorption/release, bounce, three shakes,
  success and escape effects. Summon/recall use the same transfer system.
- Original Colosseum particle masks in authored real-time timelines: two-cheek
  branching ThunderShock, flames, water, shadow/psychic, vines/leaves, physical
  strikes and status effects. Blastoise Hydro Pump uses both animated cannons;
  the Charmander line has tail flames and Gastly has a gas halo.
- Persisted per-species content-patch selections. Only resident actors retain
  their packs. Missing or invalid patches fail explicitly and can be restored
  to the bundled species from the panel.

Hold the GameCube **R binding** plus D-pad Up to throw, Down to summon/recall,
and Left/Right to select a party member. These actions also have panel buttons.
Sheathe Link's weapon before throwing; wolf, mounted, cutscene and item-use
states are excluded. Save using Zelda's normal save menu to persist the party,
HP, PP and balls. Scene changes recall companions. Practice controls heal and
restock; they do not grant randomizer checks.

## Validation and limits

The user confirmed .753 walking, summoning and real-time Pokémon combat on Xbox.
The expanded .754 implementation is **not yet hardware tested**. Portable tests,
native-header compilation and recorded GX calls do not establish console visual
quality or performance. The asset pipeline independently compares C++ deformation
with Blender and checks locomotion seams, limb lengths and planted feet.

The WZX particle VM is not emulated: original texture masks are used by newly
authored effect timelines. This build does not add trainers, experience/evolution
progression, abilities, native item-ring integration, Colosseum maps/story, audio,
or ordinary-enemy replacement. Native enemies and bosses retain their original
quest/reward and room-clear behavior. Capture odds are authored action-game
balance, not the Colosseum catch formula. Status durations adapt turn mechanics
for real-time play. Model lighting is currently a stable directional light.

## Reproduce

The engine/SDK pin is `422d7bb1b6c8d973cccf8b3d0b226a57ac3cc8c7`.
No retail model, animation, particle or disc data belongs in this repository.
The private asset pipeline uses the user's verified GC6E01 disc and the external
Blender importer pinned at `dc71de837978a6a2fe8c5082f2840cfafb135235`.

```sh
python scripts/test-pokemon-battle-cp1.py --sdk /path/to/dusklight/sdk
python scripts/test-pokemon-runtime.py --dusk /path/to/dusklight
cmake -S experimental/pokemon_battle -B build/pokemon
cmake --build build/pokemon
ctest --test-dir build/pokemon --output-on-failure
build/pokemon/pokemon_mode_tests /private/roster/runtime
```

`--sanitize` enables AddressSanitizer/UBSan on Linux. When the host tracing
sandbox prevents LeakSanitizer from reading `/proc`, `ASAN_OPTIONS=detect_leaks=0`
retains address/undefined-behavior checking while disabling leak scanning.

The full Xbox workflow stages this module only on Pokémon branches and verifies
all 36 independently signed implementations. `finalize-pokemon-msix.py` privately
adds the validated 20 species and effect pack, preserves executable bytes and
catalog contents, signs the package and independently checks every installed file.
Keep package identity `DusklightPokemonPreview` when updating from .753.
