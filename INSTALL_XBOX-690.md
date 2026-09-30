# Twilight Princess Randomizer 1.4.1.690 — Xbox/UWP

.690 targets the two runtime failures reported after .687.

Seed launch:
- The selected seed is now fully parsed and activated while the Randomizer gate is still open.
- The heavy seed/message/flow activation no longer waits until the new-save lifecycle callback.
- A failed activation leaves the gate open and shows the exact underlying reason instead of immediately disabling the mod with the generic "seed could not be loaded" message.
- The new-save callback reuses an already validated activation.

Audio:
- The previous Xbox build intentionally disabled all game audio to avoid an early SDL callback deadlock.
- .690 keeps the Xbox/UWP audio path disabled at the proven-safe startup guard.
- This isolates the black-screen freeze observed immediately after choosing Randomizer in .688.
- The .688 seed preactivation fix remains enabled and unchanged.
- Audio restoration will be reintroduced only after the Xbox deadlock point is narrowed further.

The .687 compressed-seed discovery fix and .686 Soul audit remain intact. Full generator certification is forced again.


.690 seed/runtime validation:
- Generated seed.dat is now round-trip loaded by the same runtime parser before generation can report success.
- A seed that cannot be loaded is deleted instead of being advertised as verified/selectable.
- 64-bit entrance mappings use portable decimal scalar parsing to avoid yaml-cpp numeric conversion differences.
- Entrance audit wording reports how many mappings changed from vanilla instead of the ambiguous "Vanilla matches" label.
- Xbox audio remains on the .689 safe-disabled path while the main01.audio-execute deadlock is fixed separately.
