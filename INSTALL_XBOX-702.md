# Twilight Princess Randomizer 1.4.1.702 — Xbox/UWP

.702 replaces the .701 game-mode memory-card reattach path.

Observed .701 behavior:
- Pressing Start at the title screen reached the save/file screen with severe lag, then crashed.

Cause addressed:
- Vanilla and Randomizer share the same physical GCI-folder card but use different logical save filenames.
- .701 re-mounted/re-probed the entire card asynchronously when only the logical filename changed.
- That could race the file-select screen.

Fix:
- Game-mode changes wait for any previous CARD command to finish.
- They switch the logical filename and queue a lightweight COMM_RESCAN_FILE operation on the existing mounted card.
- The file-select screen sees CHECKING/BUSY until that rescan completes.
- A missing first-time Randomizer save resolves cleanly to NO_FILE so the game can create it.
- Vanilla `gczelda2` remains untouched and Randomizer stays `randomizer-xbox`.
- Native XAudio2 and the post-auto-skip fade recovery are retained.
