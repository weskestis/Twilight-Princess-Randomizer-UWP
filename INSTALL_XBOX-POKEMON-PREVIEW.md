# Pikachu preview for Xbox

This is the .750 preview installer built from the complete .749 game. Pikachu's
model, textures and all 15 animation clips are included.

1. Download `TwilightPrincessRandomizer_1.4.1.750_PikachuPreview_x64.msix`.
2. Install it over .749 through Xbox Device Portal, using the same app entry.
   The package keeps the existing identity for saves and settings. If Device
   Portal requests VCLibs, select `Microsoft.VCLibs.x64.14.00.appx` from the
   support ZIP.
3. Launch the game and load a save. Stand on open ground with Link.
4. Open **Mods → Pokemon Model Lab** and enable the mod. Opening its panel
   loads the included Pikachu automatically.
5. Select **Spawn beside Link**, then close the menu to see the preview.

Use **Next clip** to select each animation or **Play idle** to return to idle;
close the menu to let the game run. Idle loops; the other clips play once and
hold their final pose. **Remove preview** clears the actor. It also
clears when entering a cutscene or changing areas, and can then be spawned again.

The first Xbox check is whether Pikachu appears with correct textures, stays on
the ground, plays its animations, and can be removed and spawned again. Physical
Xbox testing is still required. This build previews the model; combat, ordinary
enemy replacement, catching and companions are later steps.
