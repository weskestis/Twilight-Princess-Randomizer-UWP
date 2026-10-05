# Twilight Princess Randomizer 1.4.1.732 — Xbox/UWP

Known-good base:
- 1.4.1.731 Randomizer direct-save/seed handoff remains intact.
- 1.4.1.730 carry-object hardening remains intact.
- .729 generator/breakable/logic hardening and .728 Enemy Souls transition recovery remain intact.

.732 runtime polish:
- Ordon shop delayed text now keeps an ordered FIFO of pending shelf/item identities instead of one overwriteable pending key. Delayed text actors consume the matching selection in order.
- Xbox Reset Game rejects reset re-entry before invoking game-mode callbacks or unregistering Randomizer hooks, and forces a single clean return to the Dusklight launcher.
- Online mod downloads keep using LocalState/mods/.downloads, but writable UWP filesystem paths bypass SDL's datastream writer and use an app-local CRT FILE-backed SDL_IOStream instead.
- CI verifies that the Enemy Soul catalog keeps Poe Enemy Soul distinct from the vanilla 60-count Poe Soul.
- CI rejects an 8-bit shop override item-id map so custom Enemy Soul IDs cannot silently truncate in shop data.
- Runtime journal advances to xbox-startup-stage-732.txt.

Scene crash note:
- Existing .725-.730 scene/create/transition/hard-crash diagnostics remain enabled. No speculative scene-transition rewrite is included in .732; a recurring crash will preserve the last checkpoint for the next-launch report.

Preserved:
- Direct checksummed Xbox Randomizer save plus LocalState mod sidecars.
- Full Enemy Randomizer, Enemy Souls, Enemy First-Defeat Checks, Boss Souls and Poe diagnostics.
- Final post-fill logic validation and Beatable Only proof.
- 50-attempt runtime seed reroll and deterministic CI certification.
- Pot/pumpkin pickup completion and dimmed gold/green markers.
- Xbox runtime failure and next-launch hard-crash reporting.
