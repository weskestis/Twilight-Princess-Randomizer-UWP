# Twilight Princess Randomizer 1.4.1.733 — Xbox/UWP

Built forward from .732 commit `04ccd371721d5387a4d647073ec39833ccaf11ef` on `codex/xbox-680-from-679`.

Install the x64 MSIX over .732. Keep the current app and LocalState data; uninstalling removes app-local saves and mods.

Changes:
- Reset Game latches once, keeps Randomizer hooks and seed data while the old scene shuts down, waits for audio, scene queues and save activity, then returns to the launcher after frame completion.
- Audio reset drains sounds without waiting for DSP work on the same thread. Reset DSP subframes advance even if the output voice is paused or its queue is full. Normal XAudio2 playback is unchanged.
- Transition recovery refuses to cancel a scene transaction, actor creation/deletion, scripted event or reset. The watchdog tracks the actual room, layer and scene process before treating the destination as stable.
- A recovery is successful only after the existing camera, render view, pause, fade and overlap checks pass.
- Scene-change requests start persistent tracing; tracing stays active throughout scene creation/deletion and reset. The previous .732 startup checkpoint remains readable after an upgrade.

Preserved:
- .732 Ordon delayed-text FIFO and Online Mods CRT-backed LocalState downloader.
- Normal Poe Souls and the distinct Poe Enemy Soul catalog entry.
- Randomizer, Enemy Souls, Boss Souls, first-defeat checks, pots/pumpkins, transparent tracker, controller input/cursor, cosmetics and audio.
- Direct checksummed Xbox Randomizer saves and sidecars, vanilla saves and all .725–.732 diagnostics.
- All 58 generator scenarios, up to 50 independent seed attempts per scenario, final logic validation and 50-attempt runtime seed generation.

Xbox checks still required:
1. Load a saved seed, Reset Game, return to the launcher and load it again; repeat twice. Test with game audio enabled and muted.
2. Revisit the scene transitions that previously went black or crashed.
3. Confirm Ordon shelf text matches the selected item, normal Poe Souls increment and persist after saving/reloading, and Poe Enemy Soul is labeled separately.
4. Download/install an Online Mod and confirm it appears after installation.

After a crash, relaunch and record the main-menu last runtime checkpoint. Full diagnostics remain in LocalState as `xbox-runtime-failure.txt`, `xbox-runtime-failures.log` and `xbox-startup-stage-733.txt`.
