# Twilight Princess Randomizer 1.4.1.736 — Xbox/UWP

Install `TwilightPrincessRandomizer_1.4.1.736_x64.msix` over the current app. The package identity and publisher remain unchanged, retaining LocalState saves, seeds, mods and preferences.

This build retains the .735 online download writer and idle-shop fixes, the .733 reset/transition lifecycle, and all .734 Enemy/Boss Soul and pot/pumpkin persistence work. Full shop randomization and all existing Randomizer settings remain available. The existing Cosmetics SDK hook state is also separated from Randomizer: their shared hook targets can no longer merge metadata, dispatch contexts or reset/shutdown pointers. The 62 generator scenarios retain up to 50 distinct certification attempts and strict final logic validation.

The app includes 22 optional catalog mods, with exact published versions and hashes recorded in `BUNDLED_MODS-736.json`. The signed package preserves the original texture filenames and resource bytes, including Linkle palette textures with dollar signs. New bundled mods start disabled. Enable the mod you want from the installed Mods list; existing user mod defaults and saved preferences are preserved.

- TP Classic Modern Controller UI 1.3.2 has a source port compiled into the signed Xbox app, with its original controller artwork and layout resources.
- Luau Support 1.0.0 is compiled into the app. Linkle, Play as Dark Link and the Portuguese translation are included as optional script mods.
- All 18 asset-only packages from the verified catalog snapshot are included, covering tunics, swords, shields, Epona, the wolf model, translations and Arachnophobia.
- The customized built-in Randomizer remains intact; the catalog's separate Randomizer package never replaces it.

For matching controller UI 1.3.2 downloads, installation immediately binds the downloaded resources to the implementation already signed into the app. It does not run a downloaded desktop DLL. Other native mods still require an Xbox source port; the 62-package inventory records these requirements. Downloading all packages was completed, but unsupported native packages are not included in the active mod directories.

Hardware verification remains pending. The shared shop array and message lifetime defects are repaired and covered by native tests, but the reported Barnes idle crash needs Xbox testing to confirm its cause. After installing, enter Barnes' shop and stand for at least two minutes, browse each shelf, leave and re-enter, then test scene transitions and Reset Game.

Also verify normal Poe Soul count increments and persists, Poe Enemy Soul remains distinct, randomized pots/pumpkins retain their appearance after loading, and online asset downloads install from `LocalState/mods/.downloads`.

Crash diagnostics remain in `xbox-runtime-failure.txt`, `xbox-runtime-failures.log` and `xbox-startup-stage-736.txt`; .735/.734/.733/.732 journals are retained for upgrades. Optional mods have not been played through on Xbox. Test the baseline before enabling additional mods so any remaining crash has a clear reproduction.
