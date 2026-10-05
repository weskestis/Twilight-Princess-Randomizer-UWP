# Twilight Princess Randomizer 1.4.1.731 — Xbox/UWP

Known-good base:
- 1.4.1.730 carry-object hardening remains intact.
- The .729 generator, breakable pickup, dim marker, and logic hardening remain intact.
- The .728 Enemy Souls transition fix remains preserved.

.731 Randomizer save handoff fix:
- Xbox Randomizer mod-save blobs now use a direct LocalState sidecar directory alongside the direct `randomizer-xbox.sav` path instead of requiring GameCube CARD/GCI backing.
- The selected seed remains available until the save-loaded boundary completes successfully.
- If the first seed-hash blob staging call is temporarily unavailable, the immediate save-loaded callback recovers from the selected seed instead of disabling the Randomizer.
- Save-loaded failures now report a specific seed-association error instead of only result 1.
- Runtime journal advances to `xbox-startup-stage-731.txt`.

Preserved:
- .730 wooden barrel/keg/box carry hardening and crash checkpoints.
- Final post-fill logic validation and Beatable Only proof.
- 50-attempt runtime seed reroll and deterministic CI certification.
- Bottle-capacity logic fix for filled bottles.
- Pot/pumpkin pickup completion fix.
- Gold pot RGB 102,74,17 and green pumpkin RGB 10,102,19.
- Xbox runtime failure reporting and hard-crash next-launch diagnostics.
