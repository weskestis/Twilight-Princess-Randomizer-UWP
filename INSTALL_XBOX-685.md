# Twilight Princess Randomizer 1.4.1.685 — Xbox/UWP

The .684 build boots and reaches Randomizer seed generation. The remaining failure is now a generator/data-integrity error, not the Xbox startup path:

`Unresolved flow reference in group 2: Kakariko Malo Mart Red Potion Sold Out`

The Kakariko Malo Mart purchase patches conditionally link to custom sold-out flow nodes. .685 keeps those purchase links conditional on `Shop_Items == On`, but makes both sold-out target nodes always exist in the flow-name table. This removes the dangling symbolic reference without changing whether the shop patch is actually used.

The same correction is applied to the Wooden Shield sold-out target proactively.

Because this changes embedded seed data, .685 forces the complete Randomizer generator scenario suite instead of reusing the .679 certification.

Install over the existing package; do not uninstall or clear LocalState.
