# Twilight Princess Randomizer 1.4.1.727 — Xbox/UWP

This build follows the .726 hardware report and the confirmed test settings:
Full Enemy Randomizer + Enemy First-Defeat Checks + Enemy Souls + Boss Souls.

The .726 report identified one concrete child blocker:
- PLAY_SCENE create request phase 3 (wait-layer-children)
- dScnPly phase 7 (phase_4-world-init)
- ROOM_SCENE child active
- E_HP (Poe) child create request at run-process-create

The older Soul/First-Defeat + Enemy Randomizer design intentionally preserves one native source
species/variant per ACTR/TGSC group while shuffling the rest, so the blocked E_HP can be the
preserved Poe used by Soul/first-defeat logic.

.727 adds Poe-specific creation diagnostics:
- E_HP mPhaseReq.id (resource load sub-phase)
- actor heap presence
- mpMorfSO presence
- secondary model presence
- mpMorf presence
- per-create-request process create_phase

Interpretation:
- resource_phase 0: blocked before E_HP archive loading starts; Soul/enemy-create gate becomes primary suspect.
- resource_phase 1: E_HP archive/resource synchronization itself is stuck.
- resource_phase 2: resources completed; heap/model initialization is the blocker.

All .726 scene-transaction safeguards remain. Gold randomized pots / green randomized pumpkins
remain and revert to vanilla after collection.

Expanded seed-safety hardening is deferred to .728 while this actor-creation blocker is isolated.
