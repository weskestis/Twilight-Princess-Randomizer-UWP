# Twilight Princess Randomizer 1.4.1.681 — Xbox/UWP

The .680 crash journal identified `module.lifecycle:dev.twilitrealm.dusklight.overlay`.

In .680, Aurora registration was deferred, but the overlay lifecycle still enumerated active mods and bundle overlay files before it reached that defer. .681 moves the defer ahead of all active-mod/bundle enumeration. The initial lifecycle call returns immediately, leaves synchronization dirty, and frameEnd performs the real overlay work after the runtime is live.

The startup journal is also rolled to `xbox-startup-stage-681.txt`, so the old .680 crash marker will not contaminate the first .681 launch. The launcher version display is fixed to `1.4.1.681` instead of `UNKNOWN-VERSION`.

Install over the existing package; do not uninstall or clear LocalState. If .681 still crashes, relaunch once and photograph the new Previous Xbox stage line.
