# Twilight Princess Randomizer 1.4.1.688 — Xbox/UWP

.688 targets the two runtime failures reported after .687.

Seed launch:
- The selected seed is now fully parsed and activated while the Randomizer gate is still open.
- The heavy seed/message/flow activation no longer waits until the new-save lifecycle callback.
- A failed activation leaves the gate open and shows the exact underlying reason instead of immediately disabling the mod with the generic "seed could not be loaded" message.
- The new-save callback reuses an already validated activation.

Audio:
- The previous Xbox build intentionally disabled all game audio to avoid an early SDL callback deadlock.
- .688 restores the normal game audio resource/JAS initialization path.
- Xbox SDL playback remains paused during launcher/prelaunch startup.
- Playback is resumed only after the first complete frame with prelaunch closed.
- Logo audio waits for actual audio initialization again, and static waves are loaded instead of bypassed.
- The null audio-stream guard remains in place.

The .687 compressed-seed discovery fix and .686 Soul audit remain intact. Full generator certification is forced again.
