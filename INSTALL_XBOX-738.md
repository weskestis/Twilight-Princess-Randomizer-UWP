# Twilight Princess Randomizer 1.4.1.738 — Xbox/UWP

Install `TwilightPrincessRandomizer_1.4.1.738_x64.msix` over .737. Package identity and publisher are unchanged, preserving LocalState saves, seeds, mods and preferences. If the Xbox system update removed Microsoft.VCLibs.140.00, install `Microsoft.VCLibs.x64.14.00.appx` first.

The disc/folder picker now fills the available window area. The folder list has its own scroll viewport, larger text and exactly two visible footer buttons: All Locations and Cancel. The file type, folder choice and applicable page actions appear in the vertical list.

1. Open Select Disc Image.
2. Choose Development Files to open `D:\DevelopmentFiles` directly. This shortcut remains available even when Xbox prevents listing the drive root.
3. App Folder opens Dusklight's actual installation folder, including game files uploaded into its folder through Device Portal. App Data opens the configured saves/seeds/mods folder; App Cache opens the app's cache location.
4. Use D-pad Up/Down to move and A to open a folder or select a file. B returns to the parent folder; at a named starting location it returns to All Locations, and at All Locations it cancels. LB/RB change pages in large folders. Returning restores the folder you had selected.
5. In folder selection, Choose This Folder selects the displayed folder. Save exports retain the checked staged writer and ask before replacing an existing file.

Folder access remains subject to Xbox's storage permissions. An unreadable location displays its real error and allows Back or All Locations. The Development Files and app-folder paths still require console verification; the native UI tests use controlled local mounts.

The production Browser, Modal, Pane, List, Button, Component and event handlers are tested with the same pinned RmlUi layout engine and Fira Sans fonts at 720p, 1080p and 4K. These checks exercise folder/file selection, controller focus, Back and selection restoration, paging, filters, missing folders, cancellation, export and overwrite confirmation. The .737 native backend/worker-callback tests and all 62 generator scenarios are retained, with up to 50 distinct attempts and strict final logic validation.

All existing .733–.737 reset, scene/audio/save lifecycle, shop shelf bounds and message lifetime, online download writer, optional signed mod ports, Enemy/Boss Souls, normal Poe isolation, first-defeat checks, persistent pots/pumpkins, tracker, cosmetics and diagnostics are retained. The 22 optional catalog mods keep existing defaults and preferences. This picker repair does not establish a cause for the remaining intermittent scene/Barnes crashes.

Crash records remain in `xbox-runtime-failure.txt`, `xbox-runtime-failures.log` and `xbox-startup-stage-738.txt`. Earlier .737/.736/.735/.734/.733/.732 journals are retained for upgrades.
