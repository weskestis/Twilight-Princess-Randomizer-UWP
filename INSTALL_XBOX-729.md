# Twilight Princess Randomizer 1.4.1.729 — Xbox/UWP

Known-good base:
- 1.4.1.728 transition fix remains intact.
- Enemy Souls no longer pre-gates fpcMtd_Create.
- Enemy Souls execution gating, Enemy First-Defeat tracking, Boss Souls, and Full Enemy Randomizer remain enabled.

.729 fixes:
- Junk pot/pumpkin checks are persisted complete as soon as their shuffled reward actor successfully spawns. This prevents a junk reward that vanilla inventory-capacity rules refuse to collect from causing a regrowing breakable to stay gold/green forever.
- Major, minor, and trap breakable rewards still require real pickup before the check is persisted complete.
- Gold pot marker brightness reduced about 60%: RGB 102,74,17.
- Green pumpkin marker brightness reduced about 60%: RGB 10,102,19.

Logic hardening:
- Final validation runs after post-fill/custom transformations, including custom randomized checks represented in the finished world graph.
- All Locations Reachable still requires every logical location to be reachable.
- Beatable Only now independently requires the finished seed to be provably beatable.
- No Logic intentionally remains no-guarantee.
- Runtime generation automatically deletes and re-rolls failed generated worlds up to 50 attempts (Plandomizer failures are not automatically re-rolled).
- Generator scenario retry ceiling raised from 3 to 50.

Runtime diagnostics:
- Existing .728 in-game runtime failure reporting is preserved.
- Hard process termination remains visible through the next-launch Xbox startup diagnostic path.
