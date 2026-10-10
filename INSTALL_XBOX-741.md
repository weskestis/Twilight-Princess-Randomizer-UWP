# Twilight Princess Randomizer 1.4.1.741 — Xbox/UWP

Install this x64 MSIX over .740. Package identity and publisher are retained so LocalState saves, seeds, mods and settings remain. Install Microsoft.VCLibs.x64.14.00.appx first if the Xbox update removed that dependency.

Max sign hints retain the requested counts and logical access rules. Authored pot, pumpkin and enemy first-defeat checks now have readable hint text; missing text no longer throws and rerolls these seeds. Each primary hint still has a sign reachable before collecting its item. Sign page capacity can grow when needed; Any hints retain their full reachable pool, and optional duplicate pages finish without cycling forever. Five maximum-hint scenarios extend the existing 62 strict generator scenarios to 67. Every scenario still allows at most 50 distinct attempts and requires final logic validation.

B/Escape cancels the generation worker and clears the dialog handles. Generate can be selected again while cancellation finishes; the next worker starts only after the old worker exits. Failed or throwing workers report an error and allow another try. On Xbox, progress updates run on the UI thread. Reset/mod shutdown cancels and drains the worker before unregistering its UI.

Reset Game now requests native opening-scene teardown even if the old scene draw path is black/paused. An already black fader uses a no-fade request. The launcher still waits for the old scene, scene queues, save communication and audio reset to complete. Rejected scene requests keep the existing scene/mod active for retry.

Vanilla play keeps its existing Aurora/GCI save location and gczelda2 save name. A missing Slot A is created even when Slot B already exists. Existing cards, including cards that report errors, are never reformatted by this fix. Launcher save management uses the same GCI backend as Xbox gameplay, and card paths use UTF-8.

Xbox verification: generate with Max sign hints; press B during generation and try again; dismiss a generation error and retry; save/load vanilla; reset from both vanilla and Randomizer and launch again. Console verification remains pending. Keep xbox-startup-stage-741.txt, xbox-runtime-failure.txt, xbox-mod-load.txt and xbox-mod-load-failure.txt when reporting problems; .740–.732 journals remain readable.

The complete green .740 feature set remains: Randomizer, Enemy/Boss Souls at any enabled reachable item check, first defeats, normal Poe isolation, stable pot/pumpkin markers and reward state, shops, tracker, cosmetics, signed mod ports and 22 optional resource packages, immediate known-mod conversion, online storage, controller picker, audio, saves, transitions and crash diagnostics. Compiler caching and fresh executable/PDB, native metadata, import, resource, signing and package checks remain enabled.
