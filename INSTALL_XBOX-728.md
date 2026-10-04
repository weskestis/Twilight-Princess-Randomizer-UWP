# Twilight Princess Randomizer 1.4.1.728 — Xbox/UWP

This is a real runtime fix attempt for the .726 PLAY_SCENE creation hang.

Evidence:
- Full Enemy Randomizer + Enemy First-Defeat Checks + Enemy Souls + Boss Souls are enabled.
- PLAY_SCENE is stuck at wait-layer-children / phase_4-world-init.
- The concrete child blocker is E_HP (Poe) at run-process-create.
- The Enemy Souls layer installs a pre-create hook on fpcMtd_Create, a post-create hook on fopAc_Create,
  and a pre-execute gate on fopAc_Execute.

Fix:
- Disable only the Enemy Souls fpcMtd_Create pre-hook registration.
- Enemy actors are now allowed to finish archive/resource/heap/model creation so ROOM_SCENE and PLAY_SCENE
  cannot be held open by Soul permission logic.
- Keep the fopAc_Execute Enemy Soul gate, so an enemy can still remain inert until its matching Soul is owned.
- Keep fopAc_Create post-hook tracking and Enemy First-Defeat handling.
- Keep Full Enemy Randomizer species-preservation behavior.
- Keep .727 Poe creation diagnostics so any remaining stall reports the resource/heap/model sub-phase.

This preserves the intended four-feature combination while moving permission enforcement out of the scene-construction critical path.
