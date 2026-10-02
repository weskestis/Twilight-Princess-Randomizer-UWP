# Twilight Princess Randomizer 1.4.1.699 — Xbox/UWP

.699 keeps the working .698 XAudio2 backend and adds a focused carryable-break safety fix.

Observed .698 runtime issue:
- Throwing and breaking a wooden box can crash the Xbox build.

Fix:
- The Randomizer carry-break hook now exits immediately for every non-pot carryable type.
- Only the six pot variants used by Pots randomization are allowed to inspect or modify the obj_break create-item argument.
- Wooden boxes (TYPE_KIBAKO), barrels, skulls, cannon balls, and other carryables stay entirely on the vanilla break path.
- The generated-game launch and XAudio2 paths are otherwise unchanged.
