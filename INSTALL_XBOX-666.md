# Twilight Princess Randomizer 1.4.1.666 — Xbox/UWP

This is an in-place Xbox loader hotfix based on Dusklight v2.0.1. It keeps the existing package identity `TwilightPrincessRandomizer`, the complete bundled Randomizer, and Enemy Souls.

1. Do not uninstall the existing app if you want to retain its LocalState data.
2. In Xbox Device Portal, choose **Add**, select `TwilightPrincessRandomizer_1.4.1.666_x64.msix`, and include the supplied x64 Microsoft VCLibs dependency if Device Portal requests a dependency.
3. Let Device Portal update the existing `TwilightPrincessRandomizer` package in place.
4. Restart the Xbox after deployment, then launch the app.
5. If a launch still returns to Home, launch it once more. The retained startup journal may then report the last completed stage; photograph any message before dismissing it.

Revision 666 removes Aurora's optional processor-group affinity path only for UWP. That path imported desktop-only processor-topology APIs before the app could enter its own startup diagnostics. The runtime hook engine, packaged native Randomizer module, full Dusklight v2.0.1 feature set, and Enemy Souls are retained. The signing certificate is generated solely for this CI build and is not a Microsoft Store certificate.
