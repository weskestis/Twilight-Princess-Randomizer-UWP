# Twilight Princess Randomizer 1.4.1.708 — Xbox/UWP

.708 is an in-place update over .707 and preserves the direct LocalState Randomizer save architecture.

- Keeps the official Cosmetics editor built into the signed executable.
- Isolates only the Cosmetics MenuRing hook on Xbox because it is the sole hook target shared with the built-in Randomizer.
- Keeps all other Cosmetics hooks enabled.
- Extends the Xbox activation crash journal through the delayed overlay/texture restoration window.
- Adds an explicit overlay-restoration stage so a later crash identifies whether the overlay or texture service was being restored.
- Keeps the 60-stable-frame asset-service gate, native XAudio2, save fixes, and transition fixes already in .707.
- Does not include the parked Dusklight 2.0.3 / Randomizer 1.0.6 upstream update.
