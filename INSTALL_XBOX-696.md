# Twilight Princess Randomizer 1.4.1.696 — Xbox/UWP

.696 restores core game audio using a different Xbox-safe playback model.

Why .688 froze:
- .688 successfully restored normal JAS/audio-resource initialization.
- After SDL callback playback was resumed, the Xbox runtime journal stopped at main01.audio-execute.
- Dusklight's SDL audio callback renders JAS/DSP data on a second thread while the game thread also updates JAS state behind the same critical-section system.

.696 change:
- Core game audio is enabled again.
- SDL's audio callback is not registered on Xbox.
- JAS/DSP audio is rendered on the game thread after mDoAud_Execute and pushed into SDL's stream queue.
- The queue targets about 80 ms of buffered audio to tolerate Xbox scheduling jitter without excessive latency.
- Audio initialization failure is non-fatal; the game can continue instead of crashing on a null stream.
- Logo audio initialization and static-wave loading are restored.
- New startup markers distinguish audio-engine execution from queue pumping if Xbox still stalls.
- The optional mod audio-replacement lifecycle remains disabled for now; this build targets normal game music/SFX first.

Randomizer:
- All .695 seed-launch/message-group fixes remain intact.
- .694 detailed activation diagnostics remain intact.
- .693 runtime seed parser/key-width fixes remain intact.
