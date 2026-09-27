# Twilight Princess Randomizer 1.4.1.680 — Xbox/UWP

Revision 680 fixes the first-launch crash at the overlay lifecycle boundary. Overlay registration is now deferred until the first valid runtime frame, and an empty overlay set is skipped safely.

This build restores all seven custom seed settings—Enemy Randomization, Boss Souls, Enemy Souls, Enemy First-Defeat Checks, Pots, Pumpkins, and Seed-Aware Junk—and removes duplicate choices. It also restores the transparent Item & Check Tracker, Pure Item Tracker, death counter, game timer, controller input viewer, L3+R3 cursor mode, custom menu-color randomization, and automatic loose-file/ZIP texture-pack loading.

Online update checks and WebSocket features use Xbox-compatible Windows Runtime networking (`Windows.Web.Http` and `MessageWebSocket`), not desktop WinHTTP. The launcher reports version `2.0.1+xbox.680` instead of `UNKNOWN-VERSION`.

1. Do not uninstall the existing app; updating in place preserves LocalState saves, settings, and texture packs.
2. In Xbox Device Portal, choose **Add** and select `TwilightPrincessRandomizer_1.4.1.680_x64.msix`.
3. If Device Portal requests them, add the included `.cer` certificate and x64 Microsoft VCLibs package from this folder.
4. Let Device Portal update the existing package, then launch it normally.
5. On the Dusklight launcher, choose **Randomizer** on the Play row and start it.
6. Open the Randomizer seed settings and confirm the seven restored choices appear once each.
7. In the Randomizer **Tracker** tab, enable **Item & Check Tracker**, **Pure Item Tracker**, **Death Counter**, or **Game Timer**. Their windows are transparent and movable; position/size locks are in Dusklight Settings.
8. In Dusklight Settings, enable **Show Input Viewer** for the transparent controller display.
9. In a Dusklight menu, hold L3+R3 for about 0.35 seconds to toggle cursor mode. Use the right stick to move and A to click or drag.
10. Open **Menu Colors** for manual colors, coordinated Rainbow, per-seed palettes, or a new palette on every area load.

Texture packs in the existing `texture_replacements` LocalState folder load automatically at startup. ZIP archives may contain `.dds` or `.png` replacements and do not need to be unpacked.

Game audio remains intentionally disabled on this UWP build because the earlier Xbox audio path deadlocked during the first game frame. The audio replacement lifecycle remains isolated for the same reason.

Do not uninstall, clear LocalState, or format unrelated storage while testing this revision. The signing certificate is generated solely for this CI build and is not a Microsoft Store certificate.
