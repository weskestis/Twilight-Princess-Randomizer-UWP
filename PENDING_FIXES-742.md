This batch is included in the 1.4.1.742 Xbox update. The release advances the
complete green .741 source and retains every earlier source layer and all
67 strict generator scenarios.

The reports from .741 are:

- A generated game runs, but the Pure Item tracker can scale and cannot move
  with the controller.
- Selecting Mods crashes both in the launcher and during gameplay. The visible
  mod list also looks cluttered.
- Vanilla still reports no memory card in Slot A inside the game, immediately
  before selecting a save file. The .741 Slot A creation change did not resolve
  this on the console.

The tracker has horizontal and vertical percentage controls. Unlocking its
layout enables both controls. Rendering clamps the actual window to the current
viewport, preserves mouse dragging, saves placement on release, and restores it
after hiding or recreating the UI. Reset Layout restores both positions too.

The Mods image callback now contains decode, allocation, cache and logging
exceptions. Menu icons are bounded to 512 pixels and other previews to 1024,
with aspect ratio and premultiplied alpha preserved. Bundle and gameplay texture
bytes stay intact. The installed list shows names, versions and status; author
and description remain in the detail pane. Active mods appear first and display
names sort within each group, without changing loader order or selection IDs.
Missing custom style properties no longer get dereferenced. Both menu entry
points arm the existing activity journal before constructing the window.

Slot A provisioning now has a native filesystem fallback when SDL cannot create
the GCI directory. Existing cards, saves and obstructing files are retained. A
failed fallback reports an I/O error instead of marking the card ready. The
game's actual initial CARDProbeEx result, card check, size and sector size are
recorded through the existing journal and OS report after CARDInit.

Local native checks passed for the production tracker against pinned ImGui at
720p, 1080p and 4K, edge positions, scaling, reset, invalid settings, context
recreation, mouse release persistence and locked movement. The production Mods
image provider passes thumbnail, cache eviction and injected read, decode,
allocation, unknown-exception and logging faults. Reversing the new provider
patch reproduces an uncaught image allocation failure in .741.

The production CARDInit, GCI format, CARDProbe, CARDProbeEx and CARDCheck bodies
pass injected directory failure recovery, Unicode paths, existing Slot B,
existing file preservation and failed fallback checks. Reversing the GCI change
reproduces NOCARD from the actual probe after a directory provisioning failure.
Filesystem/SDL adapters are fault-injection fixtures; this is not an Xbox
runtime result. These regressions are wired into the next build workflow.

The exact console crash and remaining Slot A cause have not been established
from console logs. Release packaging requires full Windows compilation, all
67 generator scenarios and signed-package verification. Visual menu verification
and Xbox retesting remain separate checks. Do not describe either console issue
as confirmed resolved from native fixtures or build results alone.
