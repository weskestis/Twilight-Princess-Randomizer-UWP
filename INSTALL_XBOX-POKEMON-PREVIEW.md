# Pokemon encounters for Xbox

**Dusklight Pokémon Preview 1.4.1.753** remains a separate app built from the
complete .749 game. Install it over the previous .750/.751/.752 Pokémon Preview to
retain that app's data. Keep the normal Twilight Princess Randomizer installed.
The title screen still reports the underlying .749 game version.

1. Install `DusklightPokemonPreview_1.4.1.753_x64.msix` through Xbox Device Portal.
   If needed, add `Microsoft.VCLibs.x64.14.00.appx` as the dependency.
2. Set the Pokémon Preview's **App type** to **Game** in Xbox Dev Home.
3. Choose **Play**, load a Zelda save, and stand on open ground with Link.
4. Open **Mods → Pokemon Encounters**. The bundled Pikachu loads automatically.
5. Select **Start wild encounter**, then close the menu. Up to three wild Pikachu
   can be present. Fight using Link's weapons; the red bar shows enemy HP.
6. Face a living Pikachu and use **Throw Poke Ball** in the panel, or hold the
   GameCube **R** binding (normally Xbox **RT**) and press **D-pad Up**. Lower HP
   improves capture odds. Fainted Pokémon cannot be caught.
7. Select a party member, then **Summon / recall selected Pokemon**, or hold
   **R + D-pad Down**. **R + Left/Right** changes the selected member.
8. Start another wild encounter. Your companion follows Link and fights nearby
   wild Pokémon and supported ordinary Zelda enemies. Its health bar is green.
9. **Save through Zelda's normal save menu** before quitting. Load the same Zelda
   save to restore the six-slot party, current HP, move PP, and remaining balls.

Each Zelda save slot has its own party. Throwing costs one ball; an area change
interrupts the throw without adding a Pokémon. A full party prevents throwing.
Companions are recalled on room/scene changes and can be summoned again. Fainted
companions remain in the party. Use **Clear encounter and recall**, close the
menu briefly to allow actor removal, then **Rest party** to restore HP and PP.
The practice panel also restocks the starting supply of 24 balls.

The .753 animation update adds authored walking and running on Pikachu's existing
rig. Foot cycles follow actual ground travel, with gradual turning, acceleration
and stopping. Idle, locomotion, attacks and damage blend over four simulation
ticks; attack poses follow their windup, contact and recovery phases. Growl no
longer uses Surf, Tail Whip uses a tail swing, and ThunderShock uses the charge
animation. The original 15 clips remain in the model pack alongside the two new
locomotion clips. Walk/run loop in the preview lab as well.

This milestone uses the supplied, lightly refined XD Pikachu. The existing
**Pokemon Model Lab** preview controls remain
below the encounter controls. Ordinary-enemy replacement, randomizer reward
integration, a new item-ring slot, automatic wilderness population, and
Colosseum story/map import remain future work. Bosses/minibosses retain their
vanilla actors. Combat is real-time with authored action-game balance; it is not
Colosseum's turn-based formula. Growl and Tail Whip affect Pokémon stats directly;
against Link/vanilla actors, their temporary effects apply only within that
Pokémon's own combat interaction.

The user confirmed spawning in .751 and the working encounter loop in .752,
while reporting sliding and poor animations. The .753 animation changes need
a fresh Xbox check after build and package validation: follow Link, stop, turn,
walk into an obstruction, and watch attacks and damage recovery. Save/party
format and capture balance are unchanged.
