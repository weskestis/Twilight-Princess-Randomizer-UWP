# Twilight Princess Randomizer 1.4.1.673 — Xbox/UWP

This is the Xbox pending-mode handoff build. It keeps the existing `TwilightPrincessRandomizer` package identity, Dusklight v2.0.1, the built-in Randomizer, all 58 generator scenarios, and Enemy Souls.

1. Do not uninstall the existing app; updating in place preserves LocalState saves and settings.
2. In Xbox Device Portal, choose **Add** and select `TwilightPrincessRandomizer_1.4.1.673_x64.msix`.
3. Add the x64 Microsoft VCLibs package only if Device Portal explicitly reports that dependency as missing.
4. Let Device Portal update the existing `TwilightPrincessRandomizer` package, then launch it normally.
5. On the visible Dusklight launcher, use Left/Right on the Play row to choose **Randomizer**. The launcher must remain responsive after the word Randomizer appears. Then press **Play** once.

Revision 673 makes the mode carousel selection-only on Xbox. Left/Right updates a pending mode without installing any Randomizer hooks. Pressing Play completes launcher audio, card, and menu work first, activates the selected mode once, closes the launcher, and hands control directly to the game without presenting another launcher frame with those hooks active.

The startup journal remains armed after activation and records the handoff through the first successfully presented game frame. If Play still freezes or closes the app, close it and launch revision 673 once more. Report the exact `Previous Xbox stage: ...` line shown on the launcher.

Revision 673 retains revision 672's AppContainer JIT allocation/protection path and bounded journal handle, plus revision 670's temporary quarantine of optional mod overlay, texture, and audio lifecycle synchronization. Core Randomizer logic, runtime hooks, menus, the generator, and Enemy Souls remain included.

The signing certificate is generated solely for this CI build and is not a Microsoft Store certificate.
