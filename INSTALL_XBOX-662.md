# Twilight Princess Randomizer 1.4.1.662 — Xbox/UWP

This is an in-place update based on Dusklight v2.0.1 with package identity `TwilightPrincessRandomizer`.

1. Do not uninstall your current build if you want to retain its LocalState data.
2. Trust `TwilightPrincessRandomizer.cer` on the PC used for deployment.
3. In Xbox Device Portal, deploy `TwilightPrincessRandomizer_1.4.1.662_x64.msix` together with the included x64 Microsoft VCLibs dependency.
4. Restart the Xbox after deployment, then launch the app and select a supported Twilight Princess GameCube disc image that you own.

The Randomizer is bundled in `mods/randomizer.dusk`; Enemy Souls is included. This build carries forward the Xbox loader fix from 1.4.1.661 and incorporates the official Dusklight v2.0.1 save, mod, audio, UI, Aurora, and Randomizer fixes. The signing certificate is generated solely for this CI build and is not a Microsoft Store certificate.
