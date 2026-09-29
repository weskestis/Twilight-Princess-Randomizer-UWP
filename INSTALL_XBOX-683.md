# Twilight Princess Randomizer 1.4.1.683 — Xbox/UWP

The .682 package now boots successfully. The remaining crash happens when Randomizer is selected, and its relaunch journal stops at `mods.frame-end`.

Revision .683 keeps the .681 overlay defer and .682 texture defer, but neither deferred asset synchronization is allowed to run during the Randomizer activation guard's first 12 frames. Both remain pending until that activation window completes.

The frame-end dispatcher is also instrumented per service. If another service is responsible, the next relaunch marker will name that exact service instead of only showing `mods.frame-end`.

The startup journal is rolled to `xbox-startup-stage-683.txt`. Install over the existing package; do not uninstall or clear LocalState.
