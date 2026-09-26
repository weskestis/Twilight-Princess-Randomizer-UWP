# Twilight Princess Randomizer 1.4.1.670 — Xbox/UWP

This is the Xbox startup-isolation build. It keeps the existing `TwilightPrincessRandomizer` package identity, Dusklight v2.0.1, the built-in Randomizer, all 58 generator scenarios, and Enemy Souls.

1. Do not uninstall the existing app; updating in place preserves LocalState saves, settings, and the previous startup marker.
2. In Xbox Device Portal, choose **Add** and select `TwilightPrincessRandomizer_1.4.1.670_x64.msix`.
3. The supplied x64 Microsoft VCLibs package is only needed if Device Portal explicitly reports that dependency as missing.
4. Let Device Portal update the existing `TwilightPrincessRandomizer` package, restart the Xbox, and launch it normally.

Revision 670 removes both recovery modal/toast paths from startup. A previous crash stage is written to the normal log, then the instrumented launch continues without forcing a recovery screen or skipping the built-in Randomizer.

The pre-frame asset lifecycle batch identified by revision 668 is quarantined on Xbox. That bypass covers only mod-provided overlay, texture, and audio synchronization. The Randomizer executable code, runtime hooks, menus, generator, Enemy Souls, standard Dusklight UI, and normal user texture-replacement loader remain present. This isolation is deliberate so the three optional asset callbacks can be restored one at a time after Xbox confirms a stable first frame.

The signing certificate is generated solely for this CI build and is not a Microsoft Store certificate.
