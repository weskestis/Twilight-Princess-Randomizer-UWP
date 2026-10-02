# Twilight Princess Randomizer 1.4.1.700 — Xbox/UWP

.700 is a focused runtime repair over the green .699 build.

Fixes:
- Vanilla Xbox saves stay on the existing working GCI-folder card root.
- Randomizer now uses a fresh Xbox-only save name, `randomizer-xbox-700`, so a malformed older Randomizer save cannot make the Randomizer file screen report Slot A as damaged.
- Automatic major-cutscene skips still bypass the pre-skip timer on Xbox.
- Immediately after the automatic skip callback, .700 clears the skip-fade flag and forces the mDoGph black filter back toward visible.
- The working native XAudio2 backend from .698/.699 is preserved.
