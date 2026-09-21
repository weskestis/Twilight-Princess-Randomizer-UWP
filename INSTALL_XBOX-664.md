# Twilight Princess Randomizer 1.4.1.664 — Xbox/UWP

This is an in-place update based on Dusklight v2.0.1 with package identity `TwilightPrincessRandomizer`.

1. Do not uninstall your current build if you want to retain its LocalState data.
2. Trust `TwilightPrincessRandomizer.cer` on the PC used for deployment.
3. In Xbox Device Portal, deploy `TwilightPrincessRandomizer_1.4.1.664_x64.msix` together with the included x64 Microsoft VCLibs dependency.
4. Restart the Xbox after deployment, then launch the app and select a supported Twilight Princess GameCube disc image that you own.

The full Randomizer is installed as a signed in-package module under `mods/randomizer`; Enemy Souls remains included. This build preserves the full Dusklight v2.0.1 and custom Randomizer feature set, loads native mod code through the UWP package graph instead of extracting executable code into LocalState, removes the desktop PSAPI import, and uses the known-good AppContainer SDL3 runtime. The signing certificate is generated solely for this CI build and is not a Microsoft Store certificate.
