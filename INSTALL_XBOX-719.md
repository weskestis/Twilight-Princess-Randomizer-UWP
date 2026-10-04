# Twilight Princess Randomizer 1.4.1.719 — Xbox/UWP

This build uses the .717 hardware failure report to target the actual stuck-transition state.

- The captured failure showed a live player in stage 61 (Upper Zora's River), no running event, no pending next stage, but the overlap manager remained in peek state for 300 frames while JUTFader stayed fully black.
- Recovery now calls the engine's normal `fopOvlpM_ClearOfReq()` path when a stale overlap remains active after the destination gameplay scene is already live.
- Both screen-fade systems are still cleared as a fail-safe so the scene becomes visible immediately.
- The failure report now also records `overlap_doing_req` and the human-readable stage name.
- Keeps .718 controls: A retries recovery, B dismisses, Retry is default-focused, and the software cursor is rendered above the modal.
