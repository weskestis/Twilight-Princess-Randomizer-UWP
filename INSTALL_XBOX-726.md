# Twilight Princess Randomizer 1.4.1.726 — Xbox/UWP

This build follows the .725 hardware video, which identified a precise scene transaction stall:

- scene_change_phase=4 (wait-destination-created)
- scene_change_target=11 (PLAY_SCENE)
- scene_change_creating_id=144
- scene_change_creating=1
- scene_change_created_exists=0
- delete_queue_size=0

Changes:
- Stops appending the same deferred-recovery line every frame; failure reports remain bounded/readable.
- Keeps recovery non-destructive while destination scene creation is still active.
- Instruments the standard process-creation request for the destination PLAY_SCENE.
- Reports create-request phase, process allocation/state, layer child-creation state, exact dScnPly creation phase, and up to 8 create-queue entries.
- This distinguishes profile load, process allocation, PLAY_SCENE create logic, layer-child completion, and post-create handoff stalls.
- Gold randomized pots / green randomized pumpkins remain and revert to vanilla after collection.
- Expanded seed-safety hardening is deferred to .727 while the blocking PLAY_SCENE creation fault is isolated.
