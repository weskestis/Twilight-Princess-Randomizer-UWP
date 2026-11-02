# Pokémon Mode .754 for Xbox

Install `DusklightPokemonPreview_1.4.1.754_x64.msix` over your existing Pokémon
Preview to retain its saves and party. Keep **App type → Game** in Xbox Dev Home.
Add `Microsoft.VCLibs.x64.14.00.appx` only if Device Portal requests the dependency.
This remains the separate Pokémon app; the normal .749 Randomizer app is separate.

1. Choose **Pokemon Vanilla** or **Pokemon Randomizer**, then load a Zelda save.
   The underlying game version display still reports .749.
2. Open **Mods → Pokemon Mode**. If the party is empty, choose Bulbasaur,
   Charmander or Squirtle as your starter.
3. Sheathe Link's weapon and stand on open ground. Hold the GameCube **R binding**
   (normally Xbox RT) and press **D-pad Down** to summon. The panel has the same
   summon/recall button. **R + Left/Right** changes the selected party member.
4. Wild Pokémon appear as you travel through areas containing supported ordinary
   enemies. **Start wild encounter** and **Next encounter species** also let you
   try each of the 20 species. **Toggle wild encounters** pauses automatic spawns.
5. Fight with Link and your companion. Face a living, weakened wild Pokémon and
   press **R + D-pad Up** to throw. Each throw spends one ball. The capture is
   decided after the three shakes; an escape visibly releases the Pokémon.
6. **Save through Zelda's normal save menu before quitting.** Each Zelda save has
   its own six-slot party, HP, PP and ball inventory. Capturing is not an autosave.

Companions recall at room/scene changes and can be summoned again. Fainted party
members remain saved. To heal, choose **Clear encounter and recall**, close the
menu briefly, then choose **Rest party**. The practice panel can restock 24 balls.
Link cannot throw while transformed, mounted, using another item, or in a
cutscene. A scene change cancels an unfinished throw.

The roster is Bulbasaur/Ivysaur/Venusaur, Charmander/Charmeleon/Charizard,
Squirtle/Wartortle/Blastoise, Pikachu/Raichu, Gastly/Haunter/Gengar, and
Eevee/Vaporeon/Jolteon/Flareon/Espeon/Umbreon. All have source battle animations
and authored field movement; Gastly and Haunter hover. Native enemies and bosses
retain their own progression and reward behavior. This build does not include
experience/evolution progression, trainers, abilities or Colosseum story/maps.

## Small content patches

The bundled content is ready to use; no LocalState folder setup is needed.
For a later species `.dpkm` patch, clear active Pokémon/preview actors and use
**Pokemon Model Lab → Load another model pack**. The species is identified and
the selection is remembered across launches. Keep the selected file available.
**Restore selected bundled Pokemon** removes that override.

If uploading a later patch from Android through Xbox Device Portal, use File
Explorer's **DevelopmentFiles**, or the Pokémon Preview package's **LocalState**
folder under LocalAppData. In the file picker, Development Files is under
**All Locations**. Do not look for a folder literally named “Pokemon's preview”.
Native gameplay changes still need a signed app update.

## Xbox check

The user confirmed .753 walking, summoning and real-time combat. The expanded
.754 mode has not yet been console-tested. After installation, check both base
modes, walking/stopping/turning, Link's ball release, capture/escape, summon/recall,
ThunderShock/flame/water/shadow effects, and party persistence after saving and
reloading. PC conversion renders and automated tests are not Xbox screenshots.
