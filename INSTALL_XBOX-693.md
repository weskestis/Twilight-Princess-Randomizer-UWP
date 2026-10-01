# Twilight Princess Randomizer 1.4.1.693 — Xbox/UWP

.693 fixes the exact runtime seed conversion failure exposed by .691/.692.

Root cause:
- The failing serialized key is 4456784 (0x00440150), the Stage 68 / Room 1 / Item 0x50 shop check.
- mShopCheckData uses a 32-bit shop key.
- The shared runtime check-metadata loader incorrectly hard-coded every metadata key to u16.
- yaml-cpp therefore rejected 4456784 before the generated seed could be accepted.

Fix:
- readCheckData now derives its key type from the destination map instead of assuming u16.
- Treasure/freestanding metadata retain their native key width.
- Shop metadata now accepts the full 32-bit shop key.
- Runtime seed round-trip validation remains mandatory.
- The nearby YAML-line diagnostic remains enabled.
- The workflow statically rejects any regression back to the hard-coded u16 metadata key loader.

Audio:
- Xbox/UWP audio remains on the proven-safe disabled path while the main01.audio-execute deadlock is investigated separately.
