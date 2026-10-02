# Twilight Princess Randomizer 1.4.1.705 — Xbox/UWP

.705 changes the Randomizer save architecture on Xbox.

After .700-.704 all reproduced the same Randomizer CARD failure, .705 stops trying to make the Randomizer save behave like a second emulated GameCube memory-card file.

- Vanilla remains unchanged on the working `gczelda2` Aurora/GCI save.
- Randomizer uses `LocalState/randomizer-xbox.sav` directly.
- The file stores the same three Twilight Princess quest-log buffers with a format header and FNV-1a checksum.
- First launch with no Randomizer save reports NO_FILE immediately, allowing normal first-save creation.
- Randomizer load/save no longer calls CARDOpen, CARDCreate, CARDRead, CARDWrite, CARDMount, or the CARD worker.
- The Randomizer mod sidecar/seed data stays separate under its existing save namespace.
- XAudio2 and the current cutscene-skip recovery remain intact.
