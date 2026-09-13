# Twilight Princess Randomizer UWP 1.4.1.650 (Pending)

This branch stages the next reviewed source patch without dispatching an Xbox
or UWP build.

## Included pending changes

- Adds **Tools -> Export Link + Wolf Modding Assets** after the owned game is
  launched.
- Produces one non-overwriting `Link-Wolf-Modding-Assets.zip` containing only
  `Kmdl.arc`, `Wmdl.arc`, a versioned manifest, and a README.
- Locks the final character mod to one `Hero-Wolf-Link.dusk` package and one
  mod-manager toggle.
- Restricts Human Link changes to the approved Hero's Clothes models.
- Restricts Wolf Link changes to `wl.bmd` and preserves the native chain
  resource byte-for-byte on joint 17, `FLEGR2`.
- Keeps every Midna resource in `Wmdl.arc` byte-for-byte vanilla.
- Includes the completed breakable lifecycle correction: a pot or pumpkin
  marker turns off once its reward has spawned, stays off after collection,
  and can return after a reload only when an unclaimed reward disappeared.

## Reviewed patch

- Encoded patch: `patches/link-wolf-exporter-650.patch.gz.b64.part00`
- Decoded patch SHA-256:
  `5c439525898aab43107e516e7aae2aaa097956835a5f1f3286918a3f28a21c3e`
- Gzip SHA-256:
  `a0188ff1c1225976596ca38455642f266372d46a3e59f0758e7b6965fd74cb41`
- Applies after `ordon-shop-runtime-649.patch`.

## Verification completed before staging

- Patch applies cleanly to the exact reconstructed `.649` source.
- 134/134 Python regression and feature-contract tests pass.
- `src/dusk/link_mod_export.cpp` passes a strict C++23 syntax check with
  warnings enabled.
- `git diff --check` passes.

No installable package has been compiled, signed, uploaded, or dispatched.
