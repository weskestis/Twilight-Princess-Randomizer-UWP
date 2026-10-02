# Twilight Princess Randomizer 1.4.1.701 — Xbox/UWP

.701 fixes the crash reproduced while Randomizer was checking Slot A.

Root cause:
- Vanilla and Randomizer intentionally use different CARD filenames.
- The memory-card worker could already be READY for vanilla `gczelda2`.
- Switching to the Randomizer filename changed only the string and left the old READY state live.
- The file-select/save path could then start a read against the new file before CARD had reattached/re-probed it.

Fix:
- Switching save filenames now atomically changes the card state to CHECKING/BUSY.
- The worker queues a fresh ATTACH/probe for the new filename before file-select can treat it as ready.
- Randomizer uses the stable Xbox save name `randomizer-xbox` for first-time creation.
- Vanilla saves remain on their existing working card.
- The .700 automatic-cutscene post-skip fade recovery and the working XAudio2 backend are retained.
