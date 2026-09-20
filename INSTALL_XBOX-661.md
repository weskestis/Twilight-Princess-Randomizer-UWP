# Twilight Princess Randomizer 1.4.1.661 — Xbox/UWP

This is an in-place update with package identity `TwilightPrincessRandomizer`.

1. Do not uninstall your current build if you want to retain its LocalState data.
2. Trust `TwilightPrincessRandomizer.cer` on the PC used for deployment.
3. In Xbox Device Portal, deploy `TwilightPrincessRandomizer_1.4.1.661_x64.msix` together with the included x64 Microsoft VCLibs dependency.
4. Restart the Xbox after deployment, then launch the app and select a supported Twilight Princess GameCube disc image that you own.

The Randomizer is bundled in `mods/randomizer.dusk`; Enemy Souls is included. Version 1.4.1.661 removes a desktop-only PSAPI diagnostic import that prevented 1.4.1.660 from activating on Xbox. The signing certificate is generated solely for this CI build and is not a Microsoft Store certificate.
