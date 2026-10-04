# Twilight Princess Randomizer 1.4.1.720 — Xbox/UWP

This build responds directly to the .719 Xbox hardware report.

- A stale major-scene overlap that remains in peek/doing state for 300 stable gameplay frames is force-advanced through the engine's normal outbound overlap command.
- The failure reporter records whether that force-clear command was accepted.
- The failure popup has its own right-stick ImGui pointer and A-button mouse bridge; B remains an unconditional dismiss fallback.
- Direct A Retry reports whether the recovery command was accepted and closes the popup when the command is successfully issued.
- Randomized, unclaimed pots are gold (255,185,42).
- Randomized, unclaimed pumpkins are bright green (24,255,48).
- Both actor markers are applied only after environment lighting and restored immediately after each draw, preventing shared-material color leaks.
- Once the shuffled breakable reward is no longer pending/uncollected, the marker condition is false and the actor renders vanilla again, including after reload.
