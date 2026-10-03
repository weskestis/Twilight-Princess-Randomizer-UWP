# Twilight Princess Randomizer 1.4.1.711 — Xbox/UWP

.711 keeps both Xbox asset systems enabled.

The .710 journal stopped while the overlay service was enumerating the built-in Cosmetics bundle:
overlay.sync.mod:dev.twilitrealm.cosmetics

The official Cosmetics and Randomizer packages contain no overlay/ or textures/ payloads, so .711 skips only those two built-in bundles during asset enumeration.

- DVD overlay synchronization remains enabled for user mods.
- Texture replacement synchronization remains enabled for user mods and texture packs.
- Cosmetics remains active.
- Randomizer remains active.
- Granular overlay/texture crash diagnostics remain enabled.
- Direct LocalState Randomizer saves, native XAudio2, and transition fixes remain intact.
- Dusklight 2.0.3 / Randomizer 1.0.6 remains parked until this Xbox baseline is stable.
