# Twilight Princess Randomizer 1.4.1.723 — Xbox/UWP

This build targets the render-path state exposed after .722 successfully cleared overlap/fader/window state but the game image remained black.

- The camera framework will not execute/draw while the global gameplay pause flag or play-scene pause timer is active.
- Recovery now clears those pause gates only after the same proven 300-frame stale major-transition condition.
- The player camera process has pause bits 1/2 cleared.
- Recovery uses dCamera_c::QuickStart(), which immediately enters active camera state 0, instead of Start(), which only enters transitional state 2 and depends on later event/order updates.
- Camera trim is reset and the view is refreshed.
- Recovery success now requires: overlap gone, transparent JUTFader, global fade clear, window/2D restored, both pause gates clear, camera process unpaused, and camera Active().
- The failure report now records global_pause, play_pause, camera_present, camera_active, camera_pause_1, and camera_pause_2.
- Gold randomized pots and green randomized pumpkins remain, reverting to vanilla after reward collection.
- Expanded seed-hardening moves to .724 so this Xbox runtime repair stays isolated.
