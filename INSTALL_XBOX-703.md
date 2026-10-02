# Twilight Princess Randomizer 1.4.1.703 — Xbox/UWP

.703 targets the remaining first-time Randomizer save failure seen in .702.

What the latest behavior tells us:
- The Randomizer file is found as a CARD container, but it still resolves to the game's fatal/damaged state.
- A failed first-time creation can leave a real .gci container on disk before any valid Twilight Princess save payload has been written.
- Aurora's CARDCreate starts that payload as zeroes. If execution dies before the first store completes, the next boot sees a file but Twilight Princess rejects its contents.

Fix:
- .703 detects only the pristine all-zero primary + backup save sectors created by an interrupted first-time CARDCreate.
- For the Xbox Randomizer save name only, that empty stub is closed and deleted.
- The card state becomes NO_FILE, allowing the normal "create a save file" flow to run again.
- Existing nonzero/valid Randomizer data is never deleted by this repair.
- Vanilla `gczelda2` is untouched.
- The .702 serialized logical CARD rescan, native XAudio2, and auto-skip fade recovery remain.
