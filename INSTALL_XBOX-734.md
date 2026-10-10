# Twilight Princess Randomizer 1.4.1.734 — Xbox/UWP

Built forward from green .733 commit `d24abf80168c1bd5d5c9c64b2977f04fd5e3fb51` on `codex/xbox-680-from-679`.

Install `TwilightPrincessRandomizer_1.4.1.734_x64.msix` over the current app to retain LocalState saves and mods.

Changes:
- Enemy Souls can be placed at any enabled randomized item check that satisfies reachability and settings, including pots, pumpkins, shops, NPC gifts, Poes, bugs, Sky Characters and Hidden Skills. Boss Souls keep the same unrestricted placement eligibility.
- Reward metadata retains full logical Soul IDs through seed writing/loading, previews, grants, tracker lookups and native check completion. Enemy Souls do not award or increment normal Poe Souls; their pickup text uses the distinct Soul identity.
- Save-loaded and save-written notifications preserve live pot/pumpkin identities and shelf bindings. Ordinary writes do not reset the collection cache. Collected rewards still persist per seed and save slot, and actor/scene deletion still retires transient bindings.
- All 85 Soul ownership bits survive save/reload. First-defeat reward associations retain their source identity during repeated resolution.
- Four added generator scenarios cover every new reward category with All Locations Reachable, Beatable Only, cleared Twilight provinces and shuffled entrances. Certification retains up to 50 distinct attempts per scenario and the existing final validation.
- All .733 deferred reset, DSP/audio progress, scene recovery guards and crash tracing are retained. Previous .733 and .732 startup journals remain readable.

Existing seeds keep their existing item placements. Generate a new seed to use the expanded placement space.

Xbox checks:
1. Load a seed with Pots/Pumpkins enabled, wait several seconds and verify the gold/green markers remain. Collect a drop, save/reload and verify it stays collected.
2. Test Soul pickups from the newly available check types. Check the Soul label and permissions; the normal Poe count should change only when obtaining a normal Poe Soul.
3. Repeat Reset Game and previously troublesome scene transitions. Recheck Ordon shelf text and Online Mods installation.

Crash diagnostics remain in LocalState as `xbox-runtime-failure.txt`, `xbox-runtime-failures.log` and `xbox-startup-stage-734.txt`. Hardware validation is pending Xbox testing.
