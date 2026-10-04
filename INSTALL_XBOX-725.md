# Twilight Princess Randomizer 1.4.1.725 — Xbox/UWP

This build stops treating every long black major transition as a finished scene with a stuck fade.

The .724 hardware report showed:
- overlap_peek=1 / overlap_doing_req=1
- PLAY_SCENE absent
- player/camera/view already partially constructed

That state is consistent with the engine's node scene-change request being between old-scene deletion and destination-scene creation.

Changes:
- Automatic recovery is deferred while a scene-change transaction is active and neither PLAY_SCENE nor OPENING_SCENE exists.
- The watchdog no longer cancels the overlap underneath an in-progress destination-scene creation.
- Recovery supports whichever dScnPly gameplay scene is actually active (PLAY_SCENE or OPENING_SCENE).
- Failure reports now include:
  - opening_scene_present / pause bits
  - scene_change_present
  - scene_change_phase and readable phase name
  - scene_change target, old ID, creating ID
  - whether the destination process is still in the creation queue or already exists
  - deletion queue size
  - up to 8 deletion-queue blocker process names/profile IDs/process IDs/states/timers
- Gold randomized pots / green randomized pumpkins remain and revert to vanilla after collection.
- Expanded seed-safety hardening is deferred to .726 while this blocking scene-creation fault is isolated.
