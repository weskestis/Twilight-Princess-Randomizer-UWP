# Twilight Princess Randomizer 1.4.1.717 — Xbox/UWP

This build targets the persistent black-but-not-frozen transition captured on Xbox hardware.

- Fixes the second Twilight Princess fade system used by cutscene skipping (mDoGph_gInf_c), not just JUTFader.
- The transition watchdog now detects both full-screen fade paths and clears both when gameplay is already live but hidden.
- Adds an on-screen **Xbox Transition Failure** report whenever the watchdog has to intervene.
- Saves the latest report to `xbox-transition-failure.txt` and appends history to `xbox-transition-failures.log` in Dusklight's writable user-data/LocalState folder.
- Captures seed hash, stage/room, event state, next-stage state, overlap state, black/stable frame counts, global fade state/rate/speed/alpha, and JUTFader status/timers.
- Includes a **Retry Fade Recovery** button directly in the popup.
- Keeps the .716 tracker performance fixes: player-controlled startup gate, reused tracker world, and one new icon upload per frame.
