# Twilight Princess Randomizer 1.4.1.686 — Xbox/UWP

The .685 Xbox build successfully generated and verified a real seed. The generation popup, however, did not show whether Boss Souls or Enemy Souls were actually present.

.686 advances the seed audit to format v4 and adds independent soul counts. When enabled, seed generation now refuses to advertise PASS unless all eight Boss Souls and all 85 Enemy Souls are accounted for across shuffled locations and starting inventory.

The verification popup now includes:
- Boss Souls: actual/expected
- Enemy Souls: actual/expected
- Starting Enemy Souls: count

Older audit-format seeds remain load-compatible; the new soul rows are shown only for v4 audits.

The complete Randomizer generator suite is forced again for this build.


Starting Enemy Souls are now conditional instead of always four:
- Faron Soul is pre-granted only if Faron Twilight is not already cleared.
- Eldin Soul is pre-granted only if Eldin Twilight is not already cleared.
- Lanayru Insect + Twilit Bloat Souls are pre-granted only if Lanayru Twilight is not already cleared.
- With all three Twilight sections skipped, Starting Enemy Souls is 0 and those four remain part of the normal 85-Soul shuffle.
