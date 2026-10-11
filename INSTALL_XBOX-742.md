# Twilight Princess Randomizer 1.4.1.742 — Xbox/UWP

Install this x64 MSIX over .741. The package name and publisher remain the same, preserving LocalState saves, seeds, mods and settings. Install Microsoft.VCLibs.x64.14.00.appx first if the Xbox update removed that dependency.

To move the Pure Item tracker with the controller, unlock its layout in settings, then adjust **Pure Tracker Horizontal Position** and **Pure Tracker Vertical Position**. These percentage controls retain placement after hiding the tracker or reopening the game. Reset Layout restores placement as well as scale. Mouse dragging remains available.

The Mods list uses compact rows, keeps active mods first, and leaves descriptions and author information in the detail pane. Menu icons and previews have bounded image dimensions. The image callback contains read, decode, allocation, cache and logging failures, and missing custom style properties are guarded. Both the launcher and in-game Mods buttons record detailed construction stages.

Vanilla Slot A has a native directory creation fallback when SDL cannot create its GCI folder. Existing cards, saves and obstructing files are retained. A failed fallback reports an I/O error. The initial card probe, check, capacity and sector size are recorded after CARDInit. This addresses a reproduced provisioning failure; the console's remaining card warning still needs verification.

Retest tracker movement, opening Mods both before Play and during a generated game, and vanilla save-file selection followed by save/load. The exact Xbox Mods crash and Slot A cause have not been confirmed from console logs. Keep xbox-startup-stage-742.txt, xbox-runtime-failure.txt, xbox-mod-load.txt and xbox-mod-load-failure.txt if a problem remains; .741–.732 journals remain readable.

The complete .741 source layers remain, including Max sign hints, generation cancellation/retry, native reset, Enemy/Boss Souls, pot/pumpkin markers, shops, cosmetics, signed mod ports, the controller picker, audio, saves and transition diagnostics. Every release runs all 67 strict generator scenarios and the executable, PDB, metadata, UWP imports, resources, dependency and signed-package checks. New native tracker, Mods image and Slot A fault regressions run in the same workflow.
