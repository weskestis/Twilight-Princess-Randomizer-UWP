# Twilight Princess Randomizer 1.4.1.709 — Xbox/UWP

.709 is an in-place update over .708.

The .708 Xbox crash journal stopped at:
module.frame-end:dev.twilitrealm.dusklight.overlay-restored

That identifies the restored DVD overlay synchronization path as the failing subsystem.

- DVD overlay synchronization is quarantined again on Xbox.
- Texture replacement remains enabled after its 60-stable-frame gate.
- The official Cosmetics editor remains built in.
- The Cosmetics MenuRing hook shared with Randomizer remains isolated on Xbox.
- Randomizer direct LocalState saves, native XAudio2, transition fixes, and the extended crash journal remain intact.
- The Dusklight 2.0.3 / Randomizer 1.0.6 upstream update remains parked until this Xbox baseline is stable.
