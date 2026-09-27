# Twilight Princess Randomizer 1.4.1.678 — Xbox/UWP

Revision 678 fixes the Randomizer new-save failure seen after the Link and Epona name screens. A missing, damaged, or newly created virtual memory card reaches name entry through an alternate game path; that path now opens the Randomizer seed-selection window before naming instead of creating a save with no seed. Runtime mod-failure toasts also show the exact reason. The game remains silent because UWP audio is intentionally disabled in this build.

It keeps the existing `TwilightPrincessRandomizer` package identity, Dusklight v2.0.1, the built-in Randomizer, all 58 generator scenarios, and Enemy Souls.

1. Do not uninstall the existing app; updating in place preserves LocalState saves and settings.
2. In Xbox Device Portal, choose **Add** and select `TwilightPrincessRandomizer_1.4.1.678_x64.msix`.
3. Add the x64 Microsoft VCLibs package only if Device Portal explicitly reports that dependency as missing.
4. Let Device Portal update the existing package, then launch it normally.
5. On the Dusklight launcher, use Left/Right on the Play row to choose **Randomizer**, then press the row once.
6. If the game reports that Slot A is damaged, acknowledge it. Before Link naming, the Randomizer window must now appear. Generate a seed under **Seed Management** if needed, select it on **Play**, and choose **Start Randomizer**.

Do not uninstall, clear LocalState, or format unrelated storage while testing this revision. The shader counter begins in the launcher and is independent of the game handoff.

The signing certificate is generated solely for this CI build and is not a Microsoft Store certificate.
