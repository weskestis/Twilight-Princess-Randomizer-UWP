# Twilight Princess Randomizer 1.4.1.735 — Xbox/UWP

Built forward from green .734 commit `df77b287d8f8893dc0b119cdcfafbb17d1f0155c` on `codex/xbox-680-from-679`.

Install `TwilightPrincessRandomizer_1.4.1.735_x64.msix` over the current app to retain LocalState saves and mods.

Changes:
- Idle shop setup no longer sorts through array index -1. Duplicate stock slots, invalid indices, missing shelves and missing cameras cannot lead to unchecked shelf-array access.
- Randomized shelf text is copied into the existing message-control lifetime binding, so another shelf's longer text cannot leave a dangling message pointer. Controls release those copies through their existing reset/destructor path.
- All deferred shelf selections clear during scene teardown. The Barnes crash after entering and standing still motivates these shared shop repairs; matching Xbox crash-log/hardware validation remains pending.
- The actual Borealis file-writing target now receives the UWP compile flag. The earlier .732 source bridge was compiled out of that dependency, leaving online downloads on SDL's WinRT datastream backend.
- Writable UWP files now use `SDL_OpenIO` callbacks backed by `CreateFile2`, `WriteFile` and `FlushFileBuffers`. The pinned SDL DLL does not export `SDL_IOFromFP`, so the old bridge could not be activated as written. No CRT FILE pointer crosses a DLL boundary.
- Downloads still stage in `LocalState/mods/.downloads`. Binary bytes, append/resume, metadata and staging remain intact. Genuine disk-full/access errors retain their Windows error code, and failed opens/binds/seeks/flushes/closes release handles.
- Online Mods distinguishes asset packages from native code. Texture, model and resource packages can use the existing installation path. Desktop DLL packages need a source port bundled into the signed Xbox app; the browser labels those packages "UWP port required". They remain visible when browsing other platforms.

The reported TP Classic Modern Controller UI v1.3.2 is a native-code mod, not a texture-only pack. Its upstream supported platform list does not include Xbox UWP. Source: <https://github.com/XandasL/TP-CM-Controller-UI>. A full UWP source port remains separate work; this storage fix does not claim to convert desktop DLLs.

All .734 Enemy/Boss Soul placement, logical reward IDs, Poe isolation, gold/green breakable markers, first-defeat checks, tracker, cosmetics, audio, saves and .733 reset/transition work are retained. Strict logic and the 62-scenario generator suite retain up to 50 distinct attempts per scenario.

Validation includes compiled production UWP callbacks and File/open methods: binary/Unicode paths, partial writes, resume after a partial disk-full failure, metadata/staged bytes, no-progress failure, move ownership, destructor cleanup, and injected open/bind/seek/flush/close errors. CI verifies the actual `borealis_io` compile flag, the pinned `SDL_OpenIO` export, and the .733/.734 native regressions before building.

Compiled shop regression tests cover every shelf-distance permutation for 1–7 shelves, 10,000 idle updates, duplicate/partial stock, invalid shelf indices, independent retained text, binary control commands, and message-control release. ASan/UBSan reproduce the original sort failure and pass the repaired functions locally.

Xbox checks:
0. Enter Barnes, stand still for at least two minutes, browse shelves, then leave/reenter. Repeat in other randomized shops; report any new crash diagnostics.
1. Download a small asset-only mod from Online Mods, retry/resume it, install it and restart if requested.
2. Verify downloads and resume metadata appear beneath `LocalState/mods/.downloads`, with installed data beneath `LocalState/mods`.
3. Check native-code packages show "UWP port required" rather than attempting to install a desktop DLL.
4. Recheck .734 pots/pumpkins, Enemy/Boss Souls, normal Poe count, Ordon shop text, Reset Game and scene transitions.

Hardware validation remains pending. A genuine full app storage area still needs free space; the writer preserves that failure rather than hiding it. Crash diagnostics continue under `xbox-runtime-failure.txt`, `xbox-runtime-failures.log` and `xbox-startup-stage-735.txt`, with .734/.733/.732 checkpoints retained for upgrades.
