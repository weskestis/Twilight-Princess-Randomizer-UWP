# Twilight Princess Randomizer 1.4.1.739 — Xbox/UWP

Install the x64 MSIX as an update to .738. Package identity and publisher remain unchanged, preserving LocalState saves, seeds, mods and preferences. Install Microsoft.VCLibs.x64.14.00.appx first if the Xbox update removed the dependency.

In Xbox Dev Home, set Dusklight's type to **Game**, then fully close and reopen it. The launcher now checks the actual memory budget before Play. It shows a message only when the runtime reports the Xbox App allowance; Game, desktop UWP and an unavailable probe retain the normal launch flow.

.739 prepares each package outside the mod registry and commits it after preparation succeeds. A caught package preparation failure leaves no partial entry or dangling signed-native context. Native context binding happens after its filesystem paths are ready. These changes close verified exception-handling gaps; the cause of the user's .738 hard process termination remains unconfirmed, and the user still reproduced it after changing to Game.

The budget probe uses the direct Windows SDK ABI, avoiding the projection helper's desktop DLL-loading fallback. An early compiled probe import check and the final packaged executable checks reject that fallback.

The startup journal now records the package ID/version, exact operation and measured memory usage/limit. Keep `xbox-startup-stage-739.txt`, `xbox-mod-load.txt`, `xbox-mod-load-failure.txt` if present, and `xbox-runtime-failure.txt` when reporting another Play crash. Earlier .738–.732 journals remain available after upgrading. A hard process termination records the last checkpoint, without proving an exception or out-of-memory cause.

The complete .738 source layers are retained, including Reset Game, transitions, audio, saves, shop text/idle guards, online installation/conversion, the full-size controller picker, Enemy/Boss Souls in reachable enabled checks, normal Poe isolation, first-defeat checks, persistent pots/pumpkins, tracker, cosmetics and the 22 optional catalog packages. No optional-mod defaults or saved preferences change.

The Windows workflow now uses a pinned compiler cache with embedded object debug information. It verifies the actual Ninja launcher/flags, links a fresh final executable/PDB/symbol manifest, checks all four native metadata sections and independent contexts, signs the package and runs all 62 generator scenarios with the existing strict logic validation and up to 50 attempts. The first cache-enabled build fills the cache; later builds benefit only where source, headers, compiler and flags match. Cache setup or server failure falls back to ordinary compilation.

Xbox hardware verification is still required for the remaining Play, Barnes shop and scene-transition crashes.
