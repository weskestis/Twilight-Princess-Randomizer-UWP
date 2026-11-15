# Twilight Princess Randomizer Xbox/UWP 1.4.1.755

Install TwilightPrincessRandomizer_1.4.1.755_x64.msix over the standard .754 app using Xbox Device Portal. Keep the existing app installed to retain LocalState saves and settings. Microsoft.VCLibs.x64.14.00.appx remains the dependency if Device Portal requests it.

The launcher remains Dusklight 2.0.3, the latest published release checked on November 15, 2026. All .754 save handling, Souls, pot/pumpkin rewards, hints, cosmetics, shops, entrance logic and Companion Tracker fixes are retained.

| Mod | Previous | Updated |
|---|---|---|
| TP Classic Modern Controller UI | 1.3.2 | 1.4.0 |
| Enhanced Hero's Tunic | 1.0.7 | 1.1.5 |
| Chinese Translation | 1.0.2 | 1.1.0 |
| Fairy Companion | 1.2.0 | 1.2.1 |
| Zelda on Epona | 1.2.3 | 1.3.2 |
| Cinematic Depth of Field | 1.0.3 | 1.1.0 |
| English Names | 1.0.1 | 1.1.1 |
| Dawnlight - Survivors | 4.0.3 | 5.4.2 |
| TPTracker | 0.4.42 | 0.4.53 |
| Dusklight Item Tracker | 1.1.0 | 1.3.0 |
| MFB Multiplayer | 0.2.0 | 0.3.4 |

Twilit Essentials remains the authentic archived 2.5.0 source port. Its 3.0.2 release is available, but the author and recovery fork source repositories are unavailable and the release contains no rebuildable source. Its version is not relabeled and newer assets are not mixed into its older implementation.

The nine native updates are rebuilt into the signed Xbox executable from pinned author commits; the two asset updates use their original release bytes. Exact manifests, source/resource hashes and Xbox adaptations are recorded in MOD_UPDATES-755.json. Source inputs are preserved with the release to support future rebuilds.

Xbox adaptations retain changeable controller design selection with calibration applied on restart, the 720-pixel default depth-of-field processing limit, native multiplayer voice playback and the Store-compatible SDL/CRT setup. Every optional mod retains its existing saved enable preference.

Before delivery the build runs native SDK/context isolation, exact-version admission, controller selection, render budgets, real Opus playback, the updated MFB author's tests, existing save/gameplay regressions, 67 Randomizer generator scenarios, resource/import audits, full MSVC compilation and MSIX signature verification.

Physical Xbox testing remains pending. Check Play and Randomizer save/reload, toggle the updated mods, test TPTracker alongside Essentials, and test Dawnlight modes and multiplayer joining on the console.
