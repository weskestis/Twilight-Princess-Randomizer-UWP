# Twilight Princess Randomizer 1.4.1.679 — Xbox/UWP

Revision 679 restores the custom Randomizer settings that disappeared from the seed screen: Enemy Randomization, Boss Souls, Enemy Souls, Enemy First-Defeat Checks, Pots, Pumpkins, and Seed-Aware Junk. It also removes repeated option rows and adds validation so duplicate settings cannot silently return.

This package restores the L3+R3 controller cursor, automatic loose-file and ZIP texture-pack loading, custom menu colors with Rainbow/per-seed/every-load modes, and online HTTP/WebSocket support through the Xbox-compatible WinHTTP backend. The overlay and texture module lifecycles are enabled again; the audio replacement lifecycle remains isolated because UWP audio is still intentionally disabled.

1. Do not uninstall the existing app; updating in place preserves LocalState saves, settings, and texture packs.
2. In Xbox Device Portal, choose **Add** and select `TwilightPrincessRandomizer_1.4.1.679_x64.msix`.
3. Add the included x64 Microsoft VCLibs package only if Device Portal reports that dependency as missing.
4. Let Device Portal update the existing package, then launch it normally.
5. On the Dusklight launcher, choose **Randomizer** on the Play row and start it.
6. Open the Randomizer seed settings and confirm the restored choices appear once each.
7. Open a Dusklight menu and hold L3+R3 for about 0.35 seconds to toggle cursor mode. Use the right stick to move and A to click or drag.
8. Open **Menu Colors** for manual colors, coordinated Rainbow, per-seed palettes, or a new palette on every area load.

Texture packs in the existing `texture_replacements` LocalState folder are loaded automatically. ZIP archives may contain `.dds` or `.png` replacements and do not need to be unpacked.

Do not uninstall, clear LocalState, or format unrelated storage while testing this revision. The signing certificate is generated solely for this CI build and is not a Microsoft Store certificate.
