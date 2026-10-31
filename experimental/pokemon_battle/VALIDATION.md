# CP1 validation

Base: Xbox .749 control commit `8b1a9bc011032b32b5e123af8d76ba644b28a971`.
Date: 2026-10-31.

Reproduced locally:

- C++20 compilation of the combat core, session contract, console demo, and
  native UI adapter with warnings treated as errors.
- All **24 battle/session checks** passed, including a **3,000-seed simulation**.
- Native panel tests passed against Dusklight SDK commit
  `422d7bb1b6c8d973cccf8b3d0b226a57ac3cc8c7`: menu lifecycle, available actions,
  victory, escape, cancellation, panel rebuild handles, no idle text churn,
  and a UI builder failure.
- The console autoplay produced a complete victory with the expected sequence:
  introduction, Link's action, enemy action, subsequent actions, and completion.
- The same suite passed AddressSanitizer and UndefinedBehaviorSanitizer.
  LeakSanitizer was disabled because this traced runtime cannot inspect the
  required `/proc` process information. No leak-test pass is claimed.

Commands:

```sh
python scripts/test-pokemon-battle-cp1.py --sdk /path/to/dusklight/sdk
ASAN_OPTIONS=detect_leaks=0 python scripts/test-pokemon-battle-cp1.py --sdk /path/to/dusklight/sdk --sanitize
git diff --check
```

A dedicated workflow runs the suite on Linux and Windows/MSVC. Its actual run
status is authoritative for CI; the local results above do not claim a Windows
or Xbox pass. The unchanged .749 production release workflow is not dispatched
by this experimental branch.

Not validated or implemented: live game/actor hooks, a rendered Pokemon model,
controller behavior on Xbox, capture/companions, save changes, real randomizer
rewards, static Xbox registration, signing, MSIX packaging, or console performance.
The host-session tests use an instrumented fake host, not Twilight Princess.

The two uploaded Pokemon source archives were inspected for files and relevant
source interfaces. Their retail matching builds were not reproduced: the retail
game inputs are absent. Source archive inspection is not a model-conversion test.
