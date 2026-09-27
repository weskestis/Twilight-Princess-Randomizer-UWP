# Twilight Princess Randomizer 1.4.1.677 — Xbox/UWP

This is the Xbox logo-audio-safe build. Revision 677 keeps UWP audio disabled and also bypasses the logo scene's first-wave wait, static-wave load, and reset-time audio call. Those calls remained reachable in revision 676 and could stop the first process creation at `fpc.create-handler`. Logo and standard-create checkpoints are included in the bounded startup journal. The game will be silent in this build.

It keeps the existing `TwilightPrincessRandomizer` package identity, Dusklight v2.0.1, the built-in Randomizer, all 58 generator scenarios, and Enemy Souls.

1. Do not uninstall the existing app; updating in place preserves LocalState saves and settings.
2. In Xbox Device Portal, choose **Add** and select `TwilightPrincessRandomizer_1.4.1.677_x64.msix`.
3. Add the x64 Microsoft VCLibs package only if Device Portal explicitly reports that dependency as missing.
4. Let Device Portal update the existing `TwilightPrincessRandomizer` package, then launch it normally.
5. On the visible Dusklight launcher, use Left/Right on the Play row to choose **Randomizer**, then press the row once.

The shader counter begins in the launcher and is independent of the game handoff. If the app freezes or closes again, force-close it and relaunch once without uninstalling or clearing app data, then report the exact `Previous Xbox stage: ...` line.

The signing certificate is generated solely for this CI build and is not a Microsoft Store certificate.
