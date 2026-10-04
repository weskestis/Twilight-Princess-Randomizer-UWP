# Twilight Princess Randomizer 1.4.1.722 — Xbox/UWP

This build corrects the recovery semantics exposed by the .721 hardware test.

- JUTFader::None is confirmed by engine source to mean fully opaque black.
- JUTFader::Wait is the transparent/non-drawing state.
- Recovery no longer resets to None and hopes a one-frame FadeIn advances.
- After a proven 300-frame transition failure, recovery directly sets JUTFader to Wait, alpha 0, zero duration/timer, while also clearing global fade and restoring scene/window/camera/2D state.
- Recovery is considered internally successful only when overlap is gone, JUTFader is Wait/transparent, global fade is clear, and window/2D state are restored.
- Pressing Retry never auto-dismisses the failure report. The report stays visible until B/Dismiss so a still-black result remains diagnosable.
- .720/.721 gold-pot and green-pumpkin pending-item markers remain, with vanilla restoration after collection.
- Expanded seed-safety hardening moves to .723 so this runtime fix remains isolated.
