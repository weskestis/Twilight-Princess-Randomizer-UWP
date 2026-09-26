# Twilight Princess Randomizer 1.4.1.669 — Xbox/UWP

This build keeps the existing `TwilightPrincessRandomizer` package identity, the complete Dusklight v2.0.1 feature set, the bundled Randomizer, and Enemy Souls. The Randomizer code is linked directly into the signed game executable; `mods/randomizer` contains its manifest and resources but no separately loaded `mod.dll`.

1. Do not uninstall the existing app if you want to retain its LocalState saves and settings.
2. In Xbox Device Portal, choose **Add** and select `TwilightPrincessRandomizer_1.4.1.669_x64.msix`.
3. Include the supplied x64 Microsoft VCLibs package if Device Portal requests a dependency. If the signing certificate is not already trusted, install `TwilightPrincessRandomizer.cer` first.
4. Let Device Portal update the existing `TwilightPrincessRandomizer` package in place.
5. Restart the Xbox after deployment, then launch the app.

If revision 668 left a startup-stage marker in LocalState, the first 669 launch intentionally enters recovery mode. Its warning is non-blocking: wait at least ten seconds, close the app normally, and launch it again. The following launch activates the built-in Randomizer and exercises the corrected lifecycle path.

Revision 669 preserves the previous crash stage throughout recovery, replaces the unusable Xbox recovery modal with a timed warning, reports every lifecycle service separately, and skips Aurora's initial DVD-overlay registration when the package has no overlays. It retains revision 668's single-image static Randomizer integration, runtime hook engine, Xbox thread-affinity and Funchook compatibility fixes, all Randomizer generator scenarios, and Enemy Souls. It also pins the current Dusklight UWP SDL3/Dawn dependency pair and verifies every packaged Visual C++ runtime import against the exact VCLibs framework shipped beside the MSIX.

The signing certificate is generated solely for this CI build and is not a Microsoft Store certificate.
