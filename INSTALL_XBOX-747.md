Xbox UWP 1.4.1.747

Install `TwilightPrincessRandomizer_1.4.1.747_x64.msix` through Xbox Device Portal in Developer Mode. Use an update installation to retain saves, data location and mod settings.

This build adds the launcher side of a Dusklight companion connection. Open **Companion Tracker → Pair Android** on the launcher or in-game menu. Use the displayed Xbox IP and code, or the QR deep link in the companion app. Compare the matching code and approve on Xbox. The connection is read-only and uses the same local IPv4 network. Closing the screen keeps it on; **Turn connection off** stops it. Pair again after closing/restarting Dusklight.

The app can read the active seed/settings, collected and seen checks, owned items/Souls, discovered entrances, dungeon state, game time and deaths. It receives full snapshots after reconnecting. The Android app itself is a separate deliverable; the integration contract is in `COMPANION_API-747.md`.

All .744/.745/.746 fixes, signed source-built mods, restored per-scene appearance and adjustable game-time/red-death overlays remain. The .746 mod-menu navigation optimization remains unchanged. This build does not claim additional Xbox FPS improvements.

Physical Xbox/phone pairing validation remains pending installation. Source ports of Crests of Courage, A Link Between Twilight and MFB Multiplayer remain; Xbox microphone capture and the previously source-blocked standalone mods have the same limits as .746.
