# Twilight Princess Randomizer 1.4.1.724 — Xbox/UWP

This build targets the two render gates not measured by .723.

- Major overlap handling can leave the PLAY_SCENE process tree paused even when global/play pause and camera pause flags are clear.
- Recovery now finds fpcNm_PLAY_SCENE_e and clears pause bits 1/2 recursively from the scene node.
- The 3D renderer also requires an active global drawlist view. Recovery now rebinds window 0, its viewport, and the player camera's view into the global drawlist.
- Recovery success now also requires the play scene to be unpaused and the global render view to be the player camera view with a valid viewport.
- Failure reports add play_scene_present, play_scene_pause_1/2, render_view_present, render_view_is_player_camera, and render_viewport_present.
- .723 camera QuickStart/pause cleanup, .722 direct transparent JUTFader state, .721 overlap cancellation, and .720 breakable color lifecycle remain.
- Expanded seed-safety hardening moves to .725 so this remaining Xbox rendering fault stays isolated.
