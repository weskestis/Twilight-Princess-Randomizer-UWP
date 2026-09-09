# Primary layered Input Viewer patch

This private development branch overlays one isolated next-update feature onto the exact working
`1.4.1.634` source kit. It does not change the pinned `v1.4.1-627` runtime line and does not
contain any `.628`-`.630` launch-path changes.

## Behavior

- Replaces the existing Input Viewer at the same runtime entry point; two viewers never compete.
- Original layered GameCube/Twilight Princess controller presentation; no Ship of Harkinian artwork is copied.
- Simultaneous button and D-pad highlights.
- Live main-stick and C-stick travel.
- Analog L/R travel plus digital-click indication.
- Optional gyro and numeric analog values.
- Freely movable and truly resizable by default with uniform aspect-preserving art scaling.
- Persistent background, gold-outline, stick, analog-value, and position/size-lock controls.
- Controller-accessible size reset in Settings, plus a right-click quick menu.
- Compact compatibility view used automatically only if drawing bounds are invalid.

## Safety and verification

- Exact source archive SHA-256: `b7d66ba41863a1207cbdd8266f3df8657f837cae9b814e59cbbf716d8b42a1c2`.
- Source-kit and assembled-tree static checks pass.
- Standalone C++ syntax check passes against ImGui `v1.91.9b-docking`.
- Windows/MSVC/UWP compile-only run passed: https://github.com/weskestis/Twilight-Princess-Randomizer-UWP/actions/runs/34296239427
- The verification workflow has no package or artifact-upload step.

No MSIX or release has been published from this branch.
