# Twilight Princess Randomizer 1.4.1.716 — Xbox/UWP

This build targets the hardware behavior seen in the .715 opening-scene video.

- Keeps the .715 new-save crash fix and Pure Item Tracker V3.
- Defers tracker initialization until a real player actor exists, no event/cutscene is running, room loading is finished, and the gameplay stage has stayed stable for 300 frames.
- Reuses the tracker logic world for item updates instead of running GenerateTrackerWorld again after pickups.
- Uploads at most one new tracker icon texture per rendered frame to avoid the all-at-once startup hitch.
- Black-transition recovery now uses player/stage readiness and can recover even when the overlap manager remains stuck.
- The recovery resets the JUTFader with the engine's own canonical fade-in handshake before revealing the scene.
- Keeps colors/cosmetics, mods, Random Start, seed flow, reset-audio guards, saves, and asset services.
