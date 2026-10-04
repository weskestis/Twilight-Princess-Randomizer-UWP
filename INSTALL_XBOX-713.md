# Twilight Princess Randomizer 1.4.1.713 — Xbox/UWP

This build keeps the working .711/.712 mod and asset path intact.

- **Start Randomizer** uses the already selected/generated seed and no longer runs generation synchronously, removing the minute-long UI lockup.
- A selected seed is consumed by one new save. The next new slot must select or generate another seed instead of silently inheriting the previous hash.
- **Colors / Cosmetics** is now visible directly on the launcher and opens the Cosmetics mod panel; the in-game Cosmetics tab remains available.
- A Randomizer-only Xbox watchdog forces a fade-in if a major scene transition has completed but the fader remains fully black for 180 frames.
- During Reset Game, Xbox audio gframe/pump work is skipped while reset teardown is active.
- Granular audio markers now identify whether a future crash is in time/speech/SE/BGM, Z2 frame work, sound-manager framework, or the final audio framework.
- DVD overlays and texture replacement services remain enabled.
