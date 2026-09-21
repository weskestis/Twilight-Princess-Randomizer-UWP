# Twilight Princess Randomizer 1.4.1.665 — Xbox/UWP

This is an in-place diagnostic update based on Dusklight v2.0.1 with package identity `TwilightPrincessRandomizer`.

1. Do not uninstall your current build if you want to retain its LocalState data.
2. Trust `TwilightPrincessRandomizer.cer` on the PC used for deployment.
3. In Xbox Device Portal, deploy `TwilightPrincessRandomizer_1.4.1.665_x64.msix` together with the included x64 Microsoft VCLibs dependency.
4. Restart the Xbox after deployment and launch the app.
5. If the first launch returns to Home, launch it once more. The second launch should open **Xbox Startup Recovery** and display the exact phase where the first launch stopped. Photograph that message before pressing OK.

The full Randomizer remains installed as a signed in-package module under `mods/randomizer`; Enemy Souls remains included. The normal launch path preserves the complete Dusklight v2.0.1 and custom Randomizer feature set. Only an automatically detected recovery launch skips native mod activation so the launcher can render the recorded failure phase instead of repeating a silent crash. The signing certificate is generated solely for this CI build and is not a Microsoft Store certificate.
