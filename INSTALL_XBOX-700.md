# Twilight Princess Randomizer 1.4.1.700 — Xbox/UWP

.700 is a focused runtime repair over the green .699 build.

Fixes:
- Xbox Slot A now uses a fresh dedicated persistent GCI-folder root under the app's writable LocalState-backed ConfigPath.
- Older card data is not deleted or reformatted; it is simply no longer allowed to poison the active Slot A backend.
- Automatic major-cutscene skips still bypass the pre-skip timer on Xbox.
- Immediately after the automatic skip callback, .700 clears the skip-fade flag and forces the mDoGph black filter back toward visible.
- The working native XAudio2 backend from .698/.699 is preserved.
