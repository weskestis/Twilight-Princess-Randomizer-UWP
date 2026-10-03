# Twilight Princess Randomizer 1.4.1.712 — Xbox/UWP

.712 keeps the working .711 mod/asset path and addresses the remaining user-facing regressions.

- Adds a top-level **Cosmetics** in-game menu tab that opens the official color editor directly.
- Generates/selects a fresh Randomizer seed every time **Start Randomizer** is used for a new save.
- Restores the upstream major-cutscene skip fade/timer handshake instead of clearing the skip-fade flag on Xbox.
- Keeps the unsafe forced post-skip global fade reset removed.
- Deactivates the active Randomizer game mode before Xbox Reset Game tears down the scene/runtime.
- Keeps DVD overlays and texture replacements enabled, with the .711 assetless-built-in skip and diagnostics intact.
- Keeps direct LocalState Randomizer saves and native XAudio2.
- Dusklight 2.0.3 / Randomizer 1.0.6 remains parked until this Xbox baseline is stable.
