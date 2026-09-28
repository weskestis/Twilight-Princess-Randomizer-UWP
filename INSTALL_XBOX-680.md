# Twilight Princess Randomizer 1.4.1.680 — Xbox/UWP

Revision 680 is a targeted runtime hotfix built directly from the green 1.4.1.679 baseline. It does not rebuild or replace the Randomizer settings layer.

The change is limited to the Xbox/UWP DVD-overlay startup path. Initial overlay registration is not allowed to enter Aurora during the startup lifecycle. An empty overlay set is skipped, and a non-empty initial set is deferred until frameEnd when the game runtime is live. This targets the launch failure whose last startup stage was the overlay module lifecycle.

All .679 Randomizer settings, cursor work, texture ZIP support, menu-color features, Souls/breakables work, and save-gate behavior remain inherited from the green .679 baseline.

1. Do not uninstall the existing app; update it in place so LocalState is preserved.
2. In Xbox Device Portal, install `TwilightPrincessRandomizer_1.4.1.680_x64.msix`.
3. Add the included x64 VCLibs dependency only if Device Portal says it is missing.
4. Launch normally and select Randomizer.
5. Confirm that startup gets past the previous overlay-lifecycle failure.
6. If it still fails, photograph the exact startup-stage/error message so the next change stays targeted.

Do not clear LocalState while testing.
