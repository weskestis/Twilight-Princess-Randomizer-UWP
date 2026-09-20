# Twilight Princess Randomizer UWP 1.4.1.660 checkpoint

Checkpoint date: 2026-09-20

## User goal

Finish a playable Xbox/UWP 1.4.1.660 package. Enemy Souls must be included.

## Preserved source

- Control branch: `codex/dusklight2-xbox-660-build`
- Randomizer base: `3a740aafb6ae64e8cc4803eeb83823a1d43a847f`
- Recovered Randomizer checkpoint: `4266aed28e0cf3358bac54bc10b75208e3385fb4`
- Portable patch: `patches/randomizer-660-checkpoint.patch.gz.b64.part00`
- Decoded patch SHA-256: `3469c06249482fe9b77fa93efeaefa87fc170e420410c52342ace3c7ca626a8c`
- Gzip SHA-256: `371dfcdf5bcabdb2a47f53d7942020bc272cbc825bfa01445f867c2c2db1bfd4`
- Encoded part SHA-256: `70bcd80e5f39321c19eb8aa9aaad69cdf1642b1b46bc7ae2f244e469ce8e637f`

The patch was verified with `git apply --check` against the exact Randomizer base.

## Verified so far

- All 58 generator scenarios passed, including Enemy Souls, enemy first-defeat checks, Boss Souls, cleared-twilight combinations, and maximum entrance randomization.
- The complete Randomizer module compiled and linked successfully as `randomizer.so` against Dusklight 2.
- Compressed seed loading, shop/breakable data, entrance randomization, Enemy Souls, first-defeat rewards, Boss Souls, messages, models, and tracker logic are present in the checkpoint.

## Important: not playable yet

The integration audit found that several runtime calls from the older monolithic port have not yet been connected to Dusklight 2's public hook service. The feature code is present and compiles, but Enemy Souls and related systems are not yet fully exercised by live game events.

Resume with these tasks:

1. Add module hooks for generic actor execution/deletion and disappearance so Enemy Soul gates and first-defeat rewards fire at runtime.
2. Wire extended Soul sidecars through freestanding items, chest/demo items, life containers, small keys, pickup commit, and deletion cleanup.
3. Grant `mStartingEnemySouls` during new-save setup and clear Enemy Soul, breakable, and shop scene state during seed deactivation/scene teardown.
4. Finish or deliberately proxy custom Boss/Enemy Soul display models where the module API cannot safely replace engine heap creation.
5. Port remaining shop and breakable runtime hooks, then rebuild the complete Dusklight executable and Xbox/UWP package.
6. Re-run generator tests, module build, full engine build, package smoke checks, asset checks, and binary-marker checks before calling the build playable.

## Relevant pinned repositories

- Dusklight 2 base: `e9b120544cb75e81b5aa36777f1688fa61f2d9e8`
- Aurora base: `7d4484a7abd6a10d77716977d20ff9da0cb67ce5`
- Expected Aurora patched commit: `17dec044286a818fd72900035ac2cebbc107b647ce5`
- UWP dependency: `6ba4aad18b0f149151784f31fd339225544e24f2`
- Package identity/version target: `TwilightPrincessRandomizer`, `1.4.1.660`

## Restore the Randomizer checkpoint

Concatenate the numbered part(s), Base64-decode, gunzip, and apply the resulting patch to the exact Randomizer base. The current checkpoint has one part. Verify both hashes above before applying.

## Communication requirement

Keep the user updated with a concrete status at least once per minute while tools/builds are running. Do not silently run long builds. Do not describe the build as playable until the runtime hooks and full UWP package checks pass.
