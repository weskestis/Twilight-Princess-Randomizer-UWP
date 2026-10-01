# Twilight Princess Randomizer 1.4.1.698 — Xbox/UWP

.698 directly fixes the audio blocker isolated by .697.

Observed .697:
- Xbox reports Previous Xbox stage: audio.sdl-open.
- That means SDL_OpenAudioDeviceStream itself is hanging on Xbox/UWP.

.698:
- Xbox/UWP no longer opens an SDL playback device.
- Native XAudio2 creates the mastering and source voices.
- Twilight Princess JAS/DSP synthesis remains on the game thread.
- Completed stereo float-PCM frames are submitted to XAudio2 using persistent queued buffers.
- SDL remains in the application for non-audio systems, but not for Xbox game-audio output.
- The working Randomizer launch path from .695 is unchanged.
