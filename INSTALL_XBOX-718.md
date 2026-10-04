# Twilight Princess Randomizer 1.4.1.718 — Xbox/UWP

This build keeps the .717 dual-fade recovery/failure report and makes that report fully usable on Xbox.

- **A** retries fade recovery directly from Twilight Princess controller input.
- **B** dismisses the failure report directly from Twilight Princess controller input.
- Retry Fade Recovery is the default-focused ImGui control.
- While the failure report is open, ImGui's software cursor is forced on top of the popup so pointer/cursor mode is not visually buried behind it.
- The previous cursor-render setting is restored when the report closes.
- Keeps the dual-fade black-screen recovery and LocalState failure-report files from .717.
- Keeps the .716 tracker performance fixes and all existing Randomizer features.
