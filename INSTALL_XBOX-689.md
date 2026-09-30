# Twilight Princess Randomizer 1.4.1.689 — Xbox/UWP

.689 targets the two runtime failures reported after .687.

Seed launch:
- The selected seed is now fully parsed and activated while the Randomizer gate is still open.
- The heavy seed/message/flow activation no longer waits until the new-save lifecycle callback.
- A failed activation leaves the gate open and shows the exact underlying reason instead of immediately disabling the mod with the generic "seed could not be loaded" message.
- The new-save callback reuses an already validated activation.

Audio:
- The previous Xbox build intentionally disabled all game audio to avoid an early SDL callback deadlock.
- .689 keeps the Xbox/UWP audio path disabled at the proven-safe startup guard.
- This isolates the black-screen freeze observed immediately after choosing Randomizer in .688.
- The .688 seed preactivation fix remains enabled and unchanged.
- Audio restoration will be reintroduced only after the Xbox deadlock point is narrowed further.

The .687 compressed-seed discovery fix and .686 Soul audit remain intact. Full generator certification is forced again.
