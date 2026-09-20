# Twilight Princess Randomizer UWP 1.4.1.660 checkpoint

Checkpoint date: 2026-09-20

## User goal

Finish a playable Xbox/UWP 1.4.1.660 package. Enemy Souls must be included.

## Preserved source

- Control branch: `codex/dusklight2-xbox-660-build`
- Randomizer base: `3a740aafb6ae64e8cc4803eeb83823a1d43a847f`
- Recovered Randomizer checkpoint: `4266aed28e0cf3358bac54bc10b75208e3385fb4`
- Portable patch: `patches/randomizer-660-checkpoint.patch.gz.b64.part00`
- Decoded patch SHA-256: `ebe11e82c1772019bf9e66a12d2242abf25a733caae0778f463a599cf81e6164`
- Gzip SHA-256: `34b6ea88a090bd7358f1815bf068f5e28d86d6477f610691610a2031d38f532c`
- Encoded part SHA-256: `dcc78e0800df0e3c6f320e9d96b594a2500a388f3f1037fde6b5e496a4567b78`

The patch was verified with `git apply --check` against the exact Randomizer base.

## Verified so far

- All 58 generator scenarios passed, including Enemy Souls, enemy first-defeat checks, Boss Souls, cleared-twilight combinations, and maximum entrance randomization.
- The complete Randomizer module compiled and linked successfully as `randomizer.so` against Dusklight 2.
- Compressed seed loading, shop/breakable data, entrance randomization, Enemy Souls, first-defeat rewards, Boss Souls, messages, models, and tracker logic are present in the checkpoint.
- Generic actor lifecycle, disappearance, item sidecar, shop, breakable, starting-Soul, scene teardown, and scripted enemy hooks are now connected through Dusklight 2's public module hook service.
- The UWP layers are preserved as `patches/dusklight2-uwp-660.patch.gz.b64.part00` and `patches/aurora-uwp-660.patch.gz.b64.part00`.
- The full signed Windows/UWP build and binary/package verification are defined by `.github/workflows/build-dusklight2-xbox-660.yml`.

## Important: package verification still required

The runtime integration and local module/generator validation are complete. Do not call the build playable until the Windows workflow has produced a signed MSIX and passed its export, symbol-manifest, native-module, Enemy Souls marker, payload, architecture, and signature checks.

Resume with these tasks:

1. Run the `Build Dusklight 2 Xbox 660` workflow and fix any Windows compiler/link/package failures.
2. Download and retain the verified workflow artifact.
3. Only call the result playable after every workflow verification step passes.

## Relevant pinned repositories

- Dusklight 2 base: `e9b120544cb75e81b5aa36777f1688fa61f2d9e8`
- Aurora base: `7d4484a7abd6a10d77716977d20ff9da0cb67ce5`
- Aurora UWP patch decoded SHA-256: `6bfa10d7cfbc1a537b323a44a039ed8724213bd106133e3857a77a95729325dc`
- Dusklight 2 UWP patch decoded SHA-256: `9dddfbce8e8e04ea3016d162940f05a7f620371aa40b2cfa4d91a3f73cd5c743`
- UWP dependency: `6ba4aad18b0f149151784f31fd339225544e24f2`
- Package identity/version target: `TwilightPrincessRandomizer`, `1.4.1.660`

## Restore the Randomizer checkpoint

Concatenate the numbered part(s), Base64-decode, gunzip, and apply the resulting patch to the exact Randomizer base. The current checkpoint has one part. Verify both hashes above before applying.

## Communication requirement

Keep the user updated with a concrete status at least once per minute while tools/builds are running. Do not silently run long builds. Do not describe the build as playable until the runtime hooks and full UWP package checks pass.
