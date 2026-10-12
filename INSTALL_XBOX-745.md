Install TwilightPrincessRandomizer_1.4.1.745_x64.msix through Xbox Developer Mode.

This update keeps pinned Dusklight 2.0.1 and every .744 fix and supplied mod. It restores three pre-2.0 features:

- Colors / Cosmetics → Cosmetics → Randomize Appearance: choose **Randomize All Colors on Every Load Screen**. Equipment, buttons, HUD, hearts, character colors and menu colors change once per actual area load. Rainbow is included; fixed colors stay unchanged in the scene. Turn it off to restore saved individual colors. Randomize All Per Seed and Rainbow All remain separate choices. The same master controls also appear in Menu Colors.
- Settings → Interface → Transparent Run Overlays → **Show Game Timer**. Transparent HH:MM:SS.hh text uses the existing per-save/per-seed 30 Hz active-game-time data. Pauses, loading and game-over do not advance it. Enable, resize, position horizontally/vertically, lock and reset the layout independently.
- The same section → **Show Death Counter**. Red transparent text reads the game's native per-save death total. It has its own size, position, lock and reset-layout controls. Reset Layout does not erase deaths or elapsed time.

Original preference keys and overlay window IDs are retained. Controller position controls place the overlays without a mouse. Rendering does not capture game input or repeatedly write configuration.

The .744 pickup/message retry, lighting-aware randomized pots/pumpkins, Soul-gated grotto combat rewards, shop guards, menu cache/panel changes and adjustable DOF/VBAO budgets are retained. Crests of Courage 1.10.0, A Link Between Twilight 0.3.0 and MFB Multiplayer 0.2.0 remain signed source ports; voice reception remains available and Xbox microphone input remains unavailable. The 35 original supplied asset mods, both Fierce Deity sets, all 31 additional native ports and the complete 91-mod inventory are retained.

Zora Swim, Link Ragdoll, Dark Morpheel and hopex Dark Link still need rebuildable author source. The shop crash cause and console performance remain unconfirmed.

Release gates exercise actual scene palette code, production game-time storage and pause/load/game-over predicates, and the complete tracker/overlay renderer against pinned ImGui at 720p, 1080p and 4K. The full existing strict generator and native/resource/signature checks also run. Xbox console retesting remains pending.
