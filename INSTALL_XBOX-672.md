# Twilight Princess Randomizer 1.4.1.672 — Xbox/UWP

This is the Xbox explicit-launch and W^X hook build. It keeps the existing `TwilightPrincessRandomizer` package identity, Dusklight v2.0.1, the built-in Randomizer, all 58 generator scenarios, and Enemy Souls.

1. Do not uninstall the existing app; updating in place preserves LocalState saves and settings.
2. In Xbox Device Portal, choose **Add** and select `TwilightPrincessRandomizer_1.4.1.672_x64.msix`.
3. Add the x64 Microsoft VCLibs package only if Device Portal explicitly reports that dependency as missing.
4. Let Device Portal update the existing `TwilightPrincessRandomizer` package, then launch it normally.
5. On the visible Dusklight launcher, use Left/Right on the Play row to choose **Randomizer**, then press **Play**.

Revision 672 no longer activates Randomizer behind a blank transition frame. It finishes mod registration, presents a responsive launcher on Vanilla, and waits for explicit Randomizer selection. Its hook trampolines use the Windows AppContainer allocation/protection APIs and never request simultaneous writable-and-executable memory.

The startup journal now keeps one bounded LocalState handle instead of reopening the file more than 600 times during hook activation. If selecting Randomizer still freezes or closes the app, close it and launch revision 672 once more. Report the exact `Previous Xbox stage: ...` line shown on the launcher.

Revision 672 retains revision 670's temporary quarantine of optional mod overlay, texture, and audio lifecycle synchronization. Core Randomizer logic, runtime hooks, menus, the generator, and Enemy Souls remain included.

The signing certificate is generated solely for this CI build and is not a Microsoft Store certificate.
