# Twilight Princess Randomizer UWP 1.4.1.662 — Dusklight v2.0.1

Checkpoint date: 2026-09-20

## Goal and upstream release

Rebase the complete Xbox/UWP Randomizer build from Dusklight v2.0.0 to the official fixes release tagged `v2.0.1`, while preserving the Xbox loader fix, runtime hooks, and Enemy Souls.

- Official Dusklight tag: `v2.0.1`
- Dusklight source commit: `422d7bb1b6c8d973cccf8b3d0b226a57ac3cc8c7`
- Aurora source commit: `d0933b745abe0eb9815bedcea8047575da18698d`
- Official Randomizer v1.0.4 source commit: `d0ceae8f18bbef4a40287a72a2791a3c06c9aca5`
- Control branch: `codex/dusklight2-xbox-660-build`
- Build-control source revision: `0ccb9f298660cbfe54eab1a4c2f494a8eab8f42c`

The upstream release contributes its save/import safety fixes, dialogue- and audio-mod crash fixes, launch-menu shutdown fix, UI-scale and shader-progress fixes, prelaunch Mods-button fix, Aurora updates, Luau execution-budget change, and official Randomizer v1.0.4 fixes.

## Rebase result

The Dusklight UWP/Xbox patch, Aurora UWP patch, and full custom Randomizer checkpoint all applied cleanly to their new exact upstream revisions with no conflicts. Fresh checkouts passed `git apply --check` and `git diff --check` for all three layers.

Preserved patch hashes:

| Layer | Encoded SHA-256 | Gzip SHA-256 | Decoded SHA-256 |
| --- | --- | --- | --- |
| `dusklight201-uwp-662` | `82302f5d940161c1458950a3352bc71048f379a2a39b884d2eddbce227c415bc` | `095f123e3469f636f7491f6e6b092e8a0bf097a926a9b34ae736382e159fc123` | `7ea1bb82f45f54c333f682639250986930628cab71d2353d58cf9688b243f393` |
| `aurora-uwp-662` | `99424654b2a37f8f3ec5f1d6b878864d5a20f09786e440beaecf187a623626c2` | `2edc6756bde2af85832970d9b5223b4918eb1d2f9387d653a86e9ba43292fc31` | `24173d1d5328aed26ec73756d644e670420d271269dc0553ca57b90a643e22dc` |
| `randomizer-662-checkpoint` | `1d89f69d1cc7cb04e2f1b0efed2e06749b4704d82d790920cafdb69992e22198` | `6d269e542e5472021eb413131cb5e150739a856a4843b18c9fade4a7ddbd06cf` | `a7d7986b7c33cbf025e3290fd75074dc5d59ab1cf014aea952d3ecf439b6c4cc` |

## Verified final build

- GitHub Actions run `35544374374` completed successfully with all 19 job steps green.
- All 58 Randomizer generator scenarios passed.
- Source-level and packaged-module Enemy Souls gates passed.
- The packaged native `mod.dll` imports `TwilightPrincessRandomizer.exe` and contains the Enemy Souls, Darknut Soul, Torch Slug Soul, and fail-closed gate markers.
- All eight Boss Soul model resources are present.
- Bundled Randomizer metadata reports version `1.0.4`.
- The final signed MSIX is `TwilightPrincessRandomizer_1.4.1.662_x64.msix`, 43,526,759 bytes, SHA-256 `303693c28cbbb59f726b3d17f73fb10a55dcaa0364639778346962f17a9dcd67`.
- The final handoff ZIP is `Twilight-Princess-Randomizer-UWP-1.4.1.662-Xbox-final.zip`, 88,599,426 bytes, SHA-256 `93925a8ce69443969848a8576b5c5b15928ee0c6be32e0dfc048e6b164ef5db1`.
- The downloaded artifact hash exactly matches GitHub's recorded artifact digest.
- All six entries in the included `SHA256SUMS.txt` passed independent verification; the outer ZIP and MSIX both passed archive integrity checks.
- The manifest identity is `TwilightPrincessRandomizer`, version `1.4.1.662`, architecture `x64`, target family `Windows.Universal`.
- The included `Microsoft.VCLibs.x64.14.00.appx` exactly satisfies the declared `Microsoft.VCLibs.140.00` version `14.0.33519.0` dependency.
- The packaged executable import set is identical to 1.4.1.661 and contains neither `PSAPI.DLL` nor `GetMappedFileNameA`.
- The packaged executable has a valid embedded hook symbol database: RVA `0x1c33000`, 4,634,956 bytes, 228,150 entries, symgen manifest version 2 with compression enabled.
- The certificate publisher is `CN=TwilightPrincessRandomizer`; SHA-256 fingerprint `F4:4B:15:02:74:22:E3:43:C6:1C:CE:FC:D5:B7:18:86:55:86:43:DF:ED:97:3C:CF:C6:EB:AB:8F:D7:61:D0:F4`.

## Feature scope and device proof

This remains the full Dusklight 2 line plus the complete custom Randomizer runtime-hook work. Enemy Souls, first-defeat rewards, Boss Souls, compressed seed loading, shop/breakable data, entrance randomization, messages, models, tracker logic, and the prior Xbox activation fix are preserved.

CI and independent inspection prove the package composition and remove the concrete loader blocker found in 1.4.1.660. A physical Xbox launch remains the final device-level confirmation.

## Installation

Deploy `TwilightPrincessRandomizer_1.4.1.662_x64.msix` as an in-place update with `Microsoft.VCLibs.x64.14.00.appx` supplied as its dependency. Use the included `.662` certificate if Device Portal requests one. Do not uninstall first if LocalState preservation matters, and restart the Xbox after deployment.

## Communication requirement

Keep the user updated with a concrete status at least once per minute while tools or builds are running.
