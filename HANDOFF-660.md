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

## Verified final build

- All 58 generator scenarios passed, including Enemy Souls, enemy first-defeat checks, Boss Souls, cleared-twilight combinations, and maximum entrance randomization.
- The complete Randomizer module compiled and linked successfully as `randomizer.so` against Dusklight 2.
- Compressed seed loading, shop/breakable data, entrance randomization, Enemy Souls, first-defeat rewards, Boss Souls, messages, models, and tracker logic are present in the checkpoint.
- Generic actor lifecycle, disappearance, item sidecar, shop, breakable, starting-Soul, scene teardown, and scripted enemy hooks are now connected through Dusklight 2's public module hook service.
- The UWP layers are preserved as `patches/dusklight2-uwp-660.patch.gz.b64.part00` and `patches/aurora-uwp-660.patch.gz.b64.part00`.
- The full signed Windows/UWP build and binary/package verification are defined by `.github/workflows/build-dusklight2-xbox-660.yml`.
- GitHub Actions run `35533851194` passed all 17 steps from source replay through artifact upload at source revision `a4a0c2053c72657cb5ceeead6571d1a9575ba6b2`.
- The final signed MSIX is `TwilightPrincessRandomizer_1.4.1.660_x64.msix`, SHA-256 `6f3585a92d3fe9e5dc142d79023b8d203ad855deba1ff5b80b7abbf428680db3`.
- The final handoff ZIP is `Twilight-Princess-Randomizer-UWP-1.4.1.660-Xbox-final.zip`, 88,598,077 bytes, SHA-256 `e35c67096c9005659286e531c7c606b25c97c0a913f17b4e9ab0e207636a1e7b`.
- All six files covered by the included `SHA256SUMS.txt` passed an independent local checksum verification after download.
- The packaged executable is PE32+ x86-64 and has a real embedded `.symdb`: RVA `0x1c32000`, 4,634,628 bytes, 228,124 symbols, symgen manifest version 2 with zstd compression.
- The bundled x86-64 `mod.dll` imports `TwilightPrincessRandomizer.exe` and contains the required Enemy Souls, scripted-soul, generic actor-create, and fail-closed runtime markers.
- All eight Boss Soul model resources are present in the bundled Randomizer module.
- `SignTool` identified the primary SHA-256 signature as issued to/by `TwilightPrincessRandomizer`; the only trust warning is expected until the included self-signed sideload certificate is installed.

## Package status

The requested full build and static/runtime-package verification are complete. The package meets the checkpoint's playable-build gate and is ready for Xbox/UWP sideload testing. A physical Xbox launch was not performed by CI, so device-specific deployment behavior remains the final operational check.

## Relevant pinned repositories

- Dusklight 2 base: `e9b120544cb75e81b5aa36777f1688fa61f2d9e8`
- Aurora base: `7d4484a7abd6a10d77716977d20ff9da0cb67ce5`
- Aurora UWP patch decoded SHA-256: `66b436d791b14b02dd3f8947ad7afe0176e3fc7280705ff5081fcb830e65fa28`
- Dusklight 2 UWP patch decoded SHA-256: `763de6d4cb08f087c2cda054ee5ae98dc9193b022420ad8cc7f594e91113a1d9`
- Dusklight 2 UWP patch gzip SHA-256: `6109cc87a5cfcab144c59b2d0a9a0ba38b2166161ea424a0303973d9380b727e`
- Dusklight 2 UWP encoded part SHA-256: `a08da638c8a4fbc30e8854b5171edf19794d36d2009f113d3b70a5e4a7d5be53`
- UWP dependency: `6ba4aad18b0f149151784f31fd339225544e24f2`
- Package identity/version target: `TwilightPrincessRandomizer`, `1.4.1.660`

## Restore the Randomizer checkpoint

Concatenate the numbered part(s), Base64-decode, gunzip, and apply the resulting patch to the exact Randomizer base. The current checkpoint has one part. Verify both hashes above before applying.

## Communication requirement

Keep the user updated with a concrete status at least once per minute while tools/builds are running. Do not silently run long builds. Do not describe the build as playable until the runtime hooks and full UWP package checks pass.
