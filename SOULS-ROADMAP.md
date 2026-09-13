# Soul Randomization Roadmap

## Toggle boundaries

Soul families remain independently selectable:

- **Boss Souls** — implemented in 1.4.1.651.
- **Enemy Souls** — future toggle; Off by default.
- **NPC Souls** — future toggle; Off by default.

Enabling one family must never silently enable either of the others.

## Spawn contract

- With Enemy Souls enabled, an enemy archetype cannot spawn until its matching soul is owned.
- With NPC Souls enabled, an NPC identity cannot spawn until its matching soul is owned.
- Disabling either setting preserves completely vanilla spawning for that family.
- Script-critical helpers, cutscene actors, and composite encounters require an authored safety classification before entering a soul pool.

## No-self-lock contract

A generated seed is invalid unless every enabled soul is reachable without already owning that soul.

Validation must cover:

1. Direct locks: an enemy, NPC, or boss soul cannot be placed behind its own actor requirement.
2. Transitive cycles: Soul A behind Soul B and Soul B behind Soul A is invalid.
3. Mixed-family cycles: Boss, Enemy, and NPC Soul requirements are solved together when multiple toggles are enabled.
4. Entrance/item interactions: shuffled entrances, shops, pots, pumpkins, and ordinary checks all participate in the same reachability proof.
5. Runtime parity: the spawn gate must consume the exact serialized setting and owned-soul state proven by generation.
6. Fail closed: generation rejects and rerolls any incomplete, unknown, cyclic, or unreachable soul assignment.

The validation method is target-removal reachability plus a full fixed-point progression search. Each target soul is removed from assumed inventory, its candidate location is searched under the resulting actor-spawn graph, and the complete seed is rejected if any enabled soul cannot be acquired.
