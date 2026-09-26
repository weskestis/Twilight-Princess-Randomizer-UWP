# Twilight Princess Randomizer 1.4.1.671 — Xbox/UWP

This is the Xbox first-frame isolation build. It keeps the existing `TwilightPrincessRandomizer` package identity, Dusklight v2.0.1, the built-in Randomizer, all 58 generator scenarios, and Enemy Souls.

1. Do not uninstall the existing app; updating in place preserves LocalState saves and settings.
2. In Xbox Device Portal, choose **Add** and select `TwilightPrincessRandomizer_1.4.1.671_x64.msix`.
3. The supplied x64 Microsoft VCLibs package is only needed if Device Portal explicitly reports that dependency as missing.
4. Let Device Portal update the existing `TwilightPrincessRandomizer` package, then launch it normally.

Revision 671 leaves the revision 670 asset-lifecycle quarantine in place, then delays restoring the saved Randomizer game mode until the vanilla engine has presented one complete frame. This prevents pre-frame hook activation from blocking the first visible Xbox frame while retaining the full Randomizer, runtime hooks, menus, generator, and Enemy Souls.

The startup journal now records each main-loop boundary. If the first launch still closes or remains black, close it and launch revision 671 once more. The safe standard prelaunch screen will stay on Vanilla and display `Previous Xbox stage: ...`; photograph or report that exact line. It does not use the modal or toast paths that were unsafe in revisions 668 and 669.

The signing certificate is generated solely for this CI build and is not a Microsoft Store certificate.
