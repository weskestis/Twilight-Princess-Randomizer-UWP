# Twilight Princess Randomizer 1.4.1.740 — Xbox/UWP

Install the x64 MSIX as an update to .739. Package identity and publisher remain unchanged, preserving LocalState saves, seeds, mods and preferences. Install Microsoft.VCLibs.x64.14.00.appx first if the Xbox update removed the dependency.

.740 repairs the asset scan reached at the reported `overlay.sync.mod:dev.twilitrealm.luau` checkpoint. DVD overlay and texture services scan only their own `overlay/` or `textures/` subtree. Disk paths are made relative lexically instead of through filesystem canonicalization. Packages such as the signed Luau runtime, which have no static assets in these folders, return an empty list while retaining runtime overlays.

The UWP overlay dirty flag now clears after the initial gameplay delay. Idle frames no longer rescan every mod or re-notify archives. A replacement overlay batch is prepared outside the DVD callback mutex and published only after all package listings and size queries succeed. A caught scan failure retains the current overlays and their open handles, records the package and error, and waits for the next explicit mod change instead of retrying every frame. Texture listing failures retain the current textures and runtime priority registrations.

The native regressions exercise the compiled production bundle, overlay and texture synchronization functions, including nested Unicode and dollar-sign filenames, assetless Luau, runtime buffers, failed listings and size queries, open handles, disable/retry, idle frames and texture priority changes. A reverse-patch negative control reproduces .739's repeated dirty scans and uncaught listing failures. These are verified code defects; confirming the cause of the console's hard process termination still requires Xbox testing.

The complete .739 source is retained: atomic package preparation, independent signed Randomizer/Cosmetics/controller/Luau ports, Xbox memory diagnostics, Reset Game and transition handling, audio, saves, shop text/idle guards, online installation/conversion, the controller file picker, Enemy/Boss Souls in reachable enabled checks, normal Poe isolation, first-defeat checks, persistent pots/pumpkins, tracker, cosmetics and 22 optional catalog packages.

All 62 generator scenarios still run with strict final logic validation and up to 50 attempts. The Windows cache executable fix from the current branch is retained. A cold cache needs filling before later matching compilations can benefit; final executable/PDB/symbol manifest, imports, native metadata, resource hashes and signature checks remain mandatory.

Keep `xbox-startup-stage-740.txt`, `xbox-mod-load.txt`, `xbox-mod-load-failure.txt` if present, and `xbox-runtime-failure.txt` when reporting a crash. Earlier .739–.732 journals remain available. A hard process termination records the last checkpoint without proving an exception or memory cause.
