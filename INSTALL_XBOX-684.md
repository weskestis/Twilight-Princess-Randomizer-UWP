# Twilight Princess Randomizer 1.4.1.684 — Xbox/UWP

The .683 package boots and Randomizer activation now runs for several more frames before the crash. On relaunch there is no Previous Xbox stage; that means the 12-frame activation journal completed and cleared before the failure.

The strongest remaining correlation is the deferred mod asset synchronization waking immediately after that journal window. Both of those UWP service paths have already independently produced crash markers in earlier builds: overlay, then texture.

Revision .684 therefore quarantines only the mod DVD-overlay synchronization and mod texture-service synchronization on Xbox/UWP instead of trying them again after a delay. The separate user texture_replacements subsystem remains in the build, including the ZIP/loose-file loader restored in .679.

The activation journal is extended from 12 to 60 frames and the per-service frameEnd markers remain enabled, so any different early Randomizer crash should now leave a useful marker.

Install over the current package. Do not uninstall or clear LocalState.
