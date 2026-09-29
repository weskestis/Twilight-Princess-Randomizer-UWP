# Twilight Princess Randomizer 1.4.1.681 — Xbox/UWP

Revision 681 is a narrow startup fix on top of the green .680 package.

The .680 crash journal reported `module.lifecycle:dev.twilitrealm.dusklight.overlay`. In .680 the overlay function deferred Aurora registration, but it still enumerated active mods and their bundle overlay files before returning. .681 moves the defer to the very top of the overlay lifecycle so the initial startup call returns before touching ModLoader/bundles. The real overlay synchronization is left dirty and is performed from frameEnd after the runtime is live.

This build also rolls the startup journal to `xbox-startup-stage-681.txt` so the previous .680 crash marker cannot affect the first .681 launch, and the launcher now displays version `1.4.1.681` instead of `UNKNOWN-VERSION`.

All Randomizer settings and .679/.680 feature work remain inherited unchanged.

1. Do not uninstall the existing app; update in place to preserve LocalState.
2. Install `TwilightPrincessRandomizer_1.4.1.681_x64.msix` in Xbox Device Portal.
3. Launch once normally. The first .681 launch should not inherit the .680 overlay marker.
4. If it crashes, relaunch once and photograph the new `Previous Xbox stage` line. That marker will now belong specifically to .681.

The separate online update-check failure is not hidden by this build; it remains available for the next targeted fix after startup is stable.
