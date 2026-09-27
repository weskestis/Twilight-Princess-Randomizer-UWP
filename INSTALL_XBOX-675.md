# Twilight Princess Randomizer 1.4.1.675 — Xbox/UWP

This is the Xbox audio-safe boot build. Revision 674 proved that the first game tick deadlocks inside `mDoAud_Execute()`. Revision 675 disables the UWP game-audio engine so the launcher can hand off to the game without entering that deadlock. The game will be silent in this build.

It keeps the existing `TwilightPrincessRandomizer` package identity, Dusklight v2.0.1, the built-in Randomizer, all 58 generator scenarios, and Enemy Souls.

1. Do not uninstall the existing app; updating in place preserves LocalState saves and settings.
2. In Xbox Device Portal, choose **Add** and select `TwilightPrincessRandomizer_1.4.1.675_x64.msix`.
3. Add the x64 Microsoft VCLibs package only if Device Portal explicitly reports that dependency as missing.
4. Let Device Portal update the existing `TwilightPrincessRandomizer` package, then launch it normally.
5. On the visible Dusklight launcher, use Left/Right on the Play row to choose **Randomizer**, then press the row once.

The shader counter begins in the launcher and is independent of the audio-safe handoff. Revision 675 retains the bounded startup journal. If the app freezes or closes again, force-close it and relaunch once without uninstalling or clearing app data, then report the exact `Previous Xbox stage: ...` line.

The signing certificate is generated solely for this CI build and is not a Microsoft Store certificate.
