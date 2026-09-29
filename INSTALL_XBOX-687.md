# Twilight Princess Randomizer 1.4.1.687 — Xbox/UWP

.687 fixes the generated-seed selection failure seen on Xbox after .685/.686.

Root cause: seed.dat is now written in the verified compressed format, while the seed browser still attempted to open seed.dat directly as plain YAML. Generated seeds therefore existed and passed write verification but were filtered out of the Play list.

Fixes:
- Seed discovery now uses the same compression-aware decoder as runtime seed loading.
- The Play seed control is refreshable.
- A successfully generated seed is added to the selector immediately and selected automatically.
- Reopening the Randomizer window also re-enumerates compressed and legacy uncompressed seeds correctly.
- The .686 Boss Souls / Enemy Souls audit and dynamic starting-Soul logic remain intact.

The full Randomizer generator certification suite is forced again for this build.
