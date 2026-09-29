# Twilight Princess Randomizer 1.4.1.682 — Xbox/UWP

The .681 relaunch journal moved past the overlay service and stopped at:

`module.lifecycle:dev.twilitrealm.dusklight.texture`

That confirms the .681 overlay change did its job and exposed the next startup failure.

Revision .682 leaves the .681 overlay fix intact and defers the texture service's initial active-mod/bundle scan. The first texture lifecycle callback now returns without enumerating or registering mod textures. A pending flag is consumed from the first game ModLoader frameEnd, where texture replacements are synchronized after the game runtime is active.

The startup journal is rolled to `xbox-startup-stage-682.txt`, so the .681 texture crash marker cannot contaminate the first .682 launch. The launcher reports version `1.4.1.682`.

Install over the existing package. Do not uninstall or clear LocalState. If it still crashes, relaunch once and photograph the new Previous Xbox stage line.
