# Twilight Princess Randomizer 1.4.1.667 — Xbox/UWP

This is an in-place Xbox loader-compatibility build based on Dusklight v2.0.1. It keeps the existing package identity `TwilightPrincessRandomizer`, the complete bundled Randomizer, and Enemy Souls.

1. Do not uninstall the existing app if you want to retain its LocalState data.
2. In Xbox Device Portal, choose **Add**, select `TwilightPrincessRandomizer_1.4.1.667_x64.msix`, and include the supplied x64 Microsoft VCLibs dependency if Device Portal requests a dependency.
3. Let Device Portal update the existing `TwilightPrincessRandomizer` package in place.
4. Restart the Xbox after deployment, then launch the app.
5. If a launch still returns to Home, launch it once more. The retained startup journal may then report the last completed stage; photograph any message before dismissing it.

Revision 667 retains revision 666's UWP-only removal of Aurora's optional processor-group affinity path. It also removes Funchook's `GetModuleHandleEx` loader dependency on UWP and obtains mapped-image bases through the `VirtualQuery` API already used by the known-launching revision 658 executable. The runtime hook engine, packaged native Randomizer module, full Dusklight v2.0.1 feature set, and Enemy Souls remain enabled. CI verifies every packaged Visual C++ runtime import against the exact VCLibs framework included beside the MSIX. The signing certificate is generated solely for this CI build and is not a Microsoft Store certificate.
