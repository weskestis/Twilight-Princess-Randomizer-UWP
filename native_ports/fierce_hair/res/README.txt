Fierce Deity Hair Physics 0.1.0 — Xbox test port

Uses damped spring motion on the five native head-hair joints. Roots remain
anchored. Uses the game's wind targets, movement acceleration and head turns.
The original skirt mod inspired the spring-damper approach; this port targets
head joints and does not reuse the pelvis/skirt bones.

Enable this optional mod on the signed .746 hair test build. The appearance
packages are separate .dusk downloads. They can use the existing game's hair
motion when this enhancement is off. Fully quit and restart after changing a
Link appearance. Do not combine two Link appearance packages.

Numerical stability and bounded angle checks pass. Xbox gameplay, collision,
clipping and performance still require console testing. This simulates five
bone groups, not each rendered strand independently.
