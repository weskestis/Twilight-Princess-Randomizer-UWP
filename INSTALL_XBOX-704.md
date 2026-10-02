# Twilight Princess Randomizer 1.4.1.704 — Xbox/UWP

.704 moves the Randomizer save repair down into Aurora's GCI-folder layer.

Why:
- A malformed/truncated Randomizer .gci can make CardGciFolder enter IOERROR while the card folder is first enumerated.
- Once that happens, Dusklight only sees CARD status 8 ("damaged"), before its normal Randomizer save recovery code can run.

Fix:
- On Xbox only, Aurora now identifies malformed files in our Randomizer GCI namespace during folder enumeration.
- Only invalid/truncated/mismatched Randomizer GCI files are removed and skipped.
- Valid recognized GCI files are left alone.
- Vanilla gczelda2 is never targeted.
- If the Randomizer path is occupied by an unrecognized stale file, Aurora removes that stale path and reports NOFILE rather than poisoning Card A.
- The existing all-zero first-time stub repair remains.
- Randomizer GCI deletion uses a direct file removal on Xbox rather than moving the broken stub through the _deleted folder.
