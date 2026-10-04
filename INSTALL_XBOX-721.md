# Twilight Princess Randomizer 1.4.1.721 — Xbox/UWP

This build follows the .720 hardware report.

- .720 proved the forced outbound overlap command was accepted (recovery_force_clear_result=1) but the game remained hidden.
- .721 no longer waits for the stalled overlap actor to run its final FadeOut/Done state.
- After the same strict 300-frame stale proof, recovery uses Twilight Princess's own fopOvlpM_Cancel()/delete path.
- If deletion cannot be queued on the first frame, cleanup retries automatically every update until the overlap manager is actually clear.
- Recovery explicitly restores scene execution, window 1, 2D rendering, camera execution, global fade, and JUTFader.
- The failure report now records window_num and show_2d as well as overlap/fader state.
- The .720 ImGui controller pointer and A/B fallbacks remain.
- Gold randomized pots and green randomized pumpkins remain, with vanilla restoration after collection.
- Expanded seed-hardening is deferred to .722 so this runtime transition fix can be isolated on Xbox hardware.
