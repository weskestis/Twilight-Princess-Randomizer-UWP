# Twilight Princess Randomizer 1.4.1.730 — Xbox/UWP

Known-good base:
- 1.4.1.729 generator, bottle-capacity, breakable pickup, dim marker, and transition hardening remains intact.
- The .728 Enemy Souls transition fix remains preserved.

.730 carry-object hardening:
- Randomizer pot-marker code now runs only for actual pot carry subtypes. Wooden barrels/kegs, boxes, skulls, cannon balls, Deku nuts, and light balls no longer enter the Randomizer pot-marker lookup from their draw path.
- Wooden barrel/box carry execution now guards null player/model pointers.
- Persistent Xbox checkpoints bracket carry pickup initialization, grab collision offset, drop initialization, and object break/effect boundaries.
- A hard process termination after a barrel/box interaction should now report the last carry checkpoint on the next launcher start instead of `Last runtime checkpoint: unavailable`.
- Runtime journal advanced to `xbox-startup-stage-730.txt`.

Preserved:
- Final post-fill logic validation and Beatable Only proof.
- 50-attempt runtime seed reroll and deterministic CI certification.
- Bottle-capacity logic fix for filled bottles.
- Pot/pumpkin pickup completion fix.
- Gold pot RGB 102,74,17 and green pumpkin RGB 10,102,19.
- Xbox runtime failure reporting and .728 transition recovery.
