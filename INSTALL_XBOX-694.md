# Twilight Princess Randomizer 1.4.1.694 — Xbox/UWP

.694 isolates the message/flow activation failure that .693 exposed after seed parsing was fixed.

Observed Xbox failure:
- Seed generation completes and the seed is selectable.
- Pressing Start Randomizer reaches messages::activate and returns result 5.
- ModResult 5 is MOD_INVALID_ARGUMENT.

Changes:
- Keeps every .693 seed parser/key-width fix.
- Adds exact activation diagnostics for custom messages, native message overrides, custom flow nodes, flow wiring, flow patches, graph commit, and actor-flow resolution.
- The Start Randomizer toast now shows the first specific message/flow record or registration stage that fails instead of only "result 5".
- Full generator certification and runtime seed round-trip validation remain mandatory.

Audio:
- Xbox/UWP audio remains on the proven-safe disabled path while the main01.audio-execute deadlock is investigated separately.
