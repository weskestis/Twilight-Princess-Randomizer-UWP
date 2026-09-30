# Twilight Princess Randomizer 1.4.1.692 — Xbox/UWP

.692 fixes the runtime seed conversion failure exposed by .691.

Seed/runtime:
- .691 correctly stopped a generated seed that the exact runtime parser could not reload.
- The failing record key 4456784 decodes to Stage 68 / Room 1 / Item 0x50 (Barnes Bomb Bag shop metadata).
- The shop override loader no longer hard-codes an 8-bit item ID. It reads the actual mapped type of mShopOverrides, keeping the loader aligned with the customized shop/item table.
- Runtime seed round-trip validation remains mandatory, so an unloadable seed cannot be advertised as valid or selected.
- The nearby YAML-line diagnostic remains enabled.

Certification:
- The full generator settings suite is forced for .692 instead of reusing the older .688 certification.
- The package build verifies the width-safe shop loader marker before producing the MSIX.

Audio:
- Xbox/UWP audio remains on the proven-safe disabled path while the main01.audio-execute deadlock is investigated separately.
