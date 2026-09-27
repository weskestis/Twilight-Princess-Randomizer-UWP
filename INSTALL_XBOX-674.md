# Twilight Princess Randomizer 1.4.1.674 — Xbox/UWP

This is the Xbox early-frame trace build. It keeps the existing `TwilightPrincessRandomizer` package identity, Dusklight v2.0.1, the built-in Randomizer, all 58 generator scenarios, and Enemy Souls.

1. Do not uninstall the existing app; updating in place preserves LocalState saves and settings.
2. In Xbox Device Portal, choose **Add** and select `TwilightPrincessRandomizer_1.4.1.674_x64.msix`.
3. Add the x64 Microsoft VCLibs package only if Device Portal explicitly reports that dependency as missing.
4. Let Device Portal update the existing `TwilightPrincessRandomizer` package, then launch it normally.
5. On the visible Dusklight launcher, use Left/Right on the Play row to choose **Randomizer**, then press the row once.

Revision 674 retains revision 673's selection-only mode carousel and direct Play handoff. The startup journal now remains armed across the first 12 completed game frames instead of clearing after the first frame. It records bounded checkpoints around mod updates, actor and scene management, Randomizer game-mode updates, particles, achievements, and pointer/input-frame completion.

If Randomizer freezes or closes the app, force-close it and launch revision 674 once more without uninstalling or clearing app data. Before selecting Randomizer again, report the exact `Previous Xbox stage: ...` line shown on the launcher.

Revision 674 retains revision 672's AppContainer JIT allocation/protection path and bounded journal handle, plus revision 670's temporary quarantine of optional mod overlay, texture, and audio lifecycle synchronization. Core Randomizer logic, runtime hooks, menus, the generator, and Enemy Souls remain included.

The signing certificate is generated solely for this CI build and is not a Microsoft Store certificate.
