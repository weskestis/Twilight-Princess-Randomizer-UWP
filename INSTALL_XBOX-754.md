# Twilight Princess Randomizer Xbox/UWP 1.4.1.754

Install TwilightPrincessRandomizer_1.4.1.754_x64.msix over the standard .749 app using Xbox Device Portal. The app identity and save locations are unchanged. Keep the existing app installed so its LocalState saves and settings remain available. Microsoft.VCLibs.x64.14.00.appx is the dependency if Device Portal requests it.

This release integrates the Dusklight 2.0.3 host and matching Aurora/Borealis runtime changes. The existing custom Randomizer revision is retained, including Souls, shops, entrance logic, hints, cosmetics, native mods and Companion Tracker. It does not replace that custom logic with the separate upstream Randomizer 1.0.6.

Save requests now wait for the card lock instead of silently disappearing under contention. A mode change during an active card operation is deferred until that operation finishes, preserving its original save filename. Both direct save formats use checked atomic replacement and retain the previous complete save. A missing Randomizer primary file can recover its validated backup; an existing invalid file is preserved and reported as an error.

The previous-session startup warning and its special launch path are removed. Choose Diagnostics on the launcher to read the last session report or open Logs. A leftover marker is described as an unclean exit, without assuming a startup crash. Clean shutdown clears the marker, and opening the launcher repeatedly does not append duplicate reports.

The Mod Manager incorporates the 2.0.3 cache improvements while retaining prepared thumbnails, resident list icons and a 16 MiB image-cache hard limit. Resolution and frame-rate preferences are unchanged.

Console checks for this test build:

- Cold launch Play, create or load a save, save, close the app and reload it.
- Reset to the launcher, switch to Randomizer, load a seed and save.
- Reset and switch back to Play; confirm the original save and all three quest slots remain intact.
- Repeat both directions, including an existing Randomizer seed.
- Check cosmetics, pot/pumpkin and Soul pickups, shops, Mods navigation and audio.

Automated checks and the MSIX signature are verified before delivery. Physical Xbox validation remains pending.
