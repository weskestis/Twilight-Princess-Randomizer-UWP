# Pikachu preview for Xbox

**Dusklight Pokémon Preview** is a separate test app built from the complete
.749 game. Keep your normal .749 app installed. The preview has its own app
identity, saves and settings. Pikachu's model, textures and all 15 animation
clips are included. Its installer version is 1.4.1.751.

1. Download `DusklightPokemonPreview_1.4.1.751_x64.msix`.
2. Add it through Xbox Device Portal as a new app. It appears as
   **Dusklight Pokémon Preview** alongside your existing app. If Device Portal
   requests VCLibs, select `Microsoft.VCLibs.x64.14.00.appx` from the support ZIP.
3. In Xbox Dev Home, open the preview's **View details** and set **App type**
   to **Game** ([setup reference](https://walbourn.github.io/directx-and-uwp-on-xbox-one/)).
4. Launch the preview and select your supported Twilight Princess GameCube disc
   image during its initial setup. If asked for a data folder, choose a new
   folder for the preview. Start a test save and stand on open ground
   with Link. Existing .749 saves/settings are not copied into this app.
5. Open **Mods → Pokemon Model Lab** and enable the mod. Opening its panel
   loads the included Pikachu automatically.
6. Select **Spawn beside Link**, then close the menu to see the preview.

Use **Next clip** to select each animation or **Play idle** to return to idle;
close the menu to let the game run. Idle loops; the other clips play once and
hold their final pose. **Remove preview** clears the actor. It also
clears when entering a cutscene or changing areas, and can then be spawned again.

The first Xbox check is whether Pikachu appears with correct textures, stays on
the ground, plays its animations, and can be removed and spawned again. Physical
Xbox testing is still required. This build previews the model; combat, ordinary
enemy replacement, catching and companions are later steps.

The .751 preview corrects scene ownership when **Spawn beside Link** is pressed
from the mod menu. The .750 request could place Pikachu on the root layer,
causing two submissions of the same draw packet and a cyclic render queue.
The regression check reproduces that cycle with the game's actual packet-list
code and verifies one draw for the corrected scene-owned spawn. Xbox validation
of this fix is still required.
