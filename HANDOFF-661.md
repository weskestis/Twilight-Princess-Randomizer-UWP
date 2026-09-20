# Twilight Princess Randomizer UWP 1.4.1.661 Xbox activation hotfix

Checkpoint date: 2026-09-20

## User goal

Deliver the complete Dusklight 2 Xbox/UWP build with the Randomizer runtime hooks and Enemy Souls preserved, then fix the Xbox activation failure observed in 1.4.1.660.

## 1.4.1.660 device failure and diagnosis

- Xbox Device Portal reported `-2144927141 (0x8027025B): Failed to launch the application.` for 1.4.1.660.
- Enabling Crash Data and reproducing the failure produced no user-mode dump. This is consistent with package activation failing before the application process reached a dumpable state.
- The 1.4.1.660 executable directly imported `PSAPI.DLL!GetMappedFileNameA`; the known-launching 1.4.1.658 reference executable did not import `PSAPI.DLL`.
- The import came from Funchook's optional mapped-module filename diagnostic. Hook resolution itself does not require it.

## 1.4.1.661 fix

- Control branch: `codex/dusklight2-xbox-660-build`
- Source revision: `8ab1047682260745168717ffdd4a4e2bb40213ff`
- Dusklight 2 base: `e9b120544cb75e81b5aa36777f1688fa61f2d9e8`
- Funchook's UWP build now uses a local no-op mapped-filename diagnostic instead of importing desktop `PSAPI.DLL`.
- Desktop builds retain the normal `psapi.h` diagnostic path.
- The package version is `1.4.1.661`, allowing an in-place upgrade over 1.4.1.660 while preserving package identity and LocalState.
- The workflow fails closed if the final packaged executable imports either `PSAPI.DLL` or `GetMappedFileNameA`.
- The 1.4.1.661 import set is identical to 1.4.1.660 except that `PSAPI.DLL` is absent.

Dusklight patch preservation hashes:

- Encoded part: `patches/dusklight2-uwp-661.patch.gz.b64.part00`
- Encoded part SHA-256: `4dd7708034a2886a90f7b41e32f1b76a528b6a285487fc92e14c65daac27b917`
- Gzip SHA-256: `c2bcaca4b124024bed5511e55e4fc8a6e664722a565bd0ea5a84bfeac9423692`
- Decoded patch SHA-256: `1ac89283c426850b4c6999bbda9036042e66b92870cecaf3a641778bc2881f51`

The patch was verified with `git apply --check` against the exact Dusklight 2 base, applied cleanly, and passed `git diff --check`.

## Verified final build

- GitHub Actions run `35540959867` completed successfully from source revision `8ab1047682260745168717ffdd4a4e2bb40213ff`.
- All 58 Randomizer generator scenarios passed.
- Pre-build and packaged-module checks both preserved Enemy Souls.
- The bundled x86-64 `mod.dll` imports `TwilightPrincessRandomizer.exe` and contains `Enemy Souls`, `Darknut Soul`, `Torch Slug Soul`, and the fail-closed Enemy Soul gate marker.
- All eight Boss Soul model resources are present in `mods/randomizer.dusk`.
- The final signed MSIX is `TwilightPrincessRandomizer_1.4.1.661_x64.msix`, 43,525,783 bytes, SHA-256 `f02290b49a18a812ca764aeb40cd1b87544a5ddf5954f5e627859c03a79e37a0`.
- The final handoff ZIP is `Twilight-Princess-Randomizer-UWP-1.4.1.661-Xbox-final.zip`, 88,598,219 bytes, SHA-256 `aed4e62f103f294c2e7176669620e73069fe0d041bf2e2a52033fc1c781ce06c`.
- The downloaded artifact SHA-256 exactly matches GitHub's recorded artifact digest.
- All six entries in the included `SHA256SUMS.txt` passed independent verification, and both the outer ZIP and MSIX passed archive integrity checks.
- The manifest identity is `TwilightPrincessRandomizer`, version `1.4.1.661`, architecture `x64`, target family `Windows.Universal`.
- The included `Microsoft.VCLibs.x64.14.00.appx` identity/version (`Microsoft.VCLibs.140.00`, `14.0.33519.0`) exactly satisfies the manifest dependency.
- The packaged executable has no `PSAPI.DLL` or `GetMappedFileNameA` import.
- The packaged executable is PE32+ x86-64 and has a valid embedded `.symdb`: RVA `0x1c32000`, 4,634,429 bytes, 228,122 symbols, symgen manifest version 2 with compression enabled.
- The included certificate subject/publisher is `CN=TwilightPrincessRandomizer`; its SHA-256 fingerprint is `78:47:20:96:F6:B1:99:F3:03:F4:03:A3:13:20:C1:8D:CF:18:B1:E0:63:90:26:C6:0E:7E:24:54:CA:CF:33:7C`.

## Feature scope

This remains the full Dusklight 2.0 build with the Randomizer runtime-hook integration. Enemy Souls, first-defeat rewards, Boss Souls, compressed seed loading, shop/breakable data, entrance randomization, messages, models, tracker logic, and the prior 1.4.1.660 runtime work remain included. The 1.4.1.661 source change is limited to the Xbox activation hotfix, versioning, documentation, and a fail-closed binary import check.

## Xbox installation and remaining proof

Install `TwilightPrincessRandomizer_1.4.1.661_x64.msix` as an update and include `Microsoft.VCLibs.x64.14.00.appx` in the dependency field. Do not uninstall first if preserving LocalState matters. Restart the Xbox after deployment.

CI and independent package inspection prove that the concrete pre-activation loader blocker found in 1.4.1.660 is absent. A launch on the user's physical Xbox remains the final device-level confirmation; do not claim physical launch success until that test passes.

## Communication requirement

Keep the user updated with a concrete status at least once per minute while tools or builds are running.
