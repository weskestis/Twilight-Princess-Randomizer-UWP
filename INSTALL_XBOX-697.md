# Twilight Princess Randomizer 1.4.1.697 — Xbox/UWP

.697 is the focused audio-boundary build.

Observed .696:
- Randomizer launch remains working.
- On relaunch Xbox reports: Previous Xbox stage: main01.audio-execute.
- Therefore the new SDL queue pump was never reached; the block is inside mDoAud_Execute or the audio initialization it invokes.

.697 adds fine-grained runtime journal checkpoints around audio resource loading, Z2 initialization, SDL init/open, DSP initialization, stream resume, and normal gframe processing.

The .696 callback-free queue pump remains unchanged so this test isolates the exact blocker without changing another variable.
