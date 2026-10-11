Install TwilightPrincessRandomizer_1.4.1.743_x64.msix through Xbox Device Portal. Keep the existing TwilightPrincessRandomizer package identity so LocalState saves and preferences remain available. Use the x64 VCLibs dependency and certificate included with the build if Device Portal requests them.

The Mods menu includes all 35 supplied custom asset mods, including SSB4 Fierce Deity Link and the Fierce Deity tunic-pack variant, the Zora armor colors, wolf models, shields, Epona replacements and Hour of Twilight. New packaged optional mods start disabled. Enable the outfit or model you want; choosing another member of the same asset group disables the previous choice. Restart after changing model overlays.

In TP Classic Modern Controller UI, choose Xbox, PlayStation or Switch and restart Dusklight. The buttons now save a next-launch choice even when the current texture pack is already in use.

Twilit Essentials is source-built from the recovered public 2.5.0 fork with its matching assets and collection-menu library. The unavailable upstream 2.7.1 source is not included. The signed native versions and their source pins are listed in NATIVE_MODS-743.json. Online downloads of these exact versions bind their assets to implementations in the signed executable; other native versions still need their own source port.

The package includes 28 additional signed native ports, including Deferred Fog 2.0.0, VBAO 1.1.0, Nozzlys random QoL 1.1.2 and Dusklight Item Tracker 1.1.0. The complete inventory has 88 entries, including the existing built-in and optional mods and the 35 restored custom asset mods. The inventory also records native mods that still require source recovery or further Xbox integration.

Vanilla saves use the ordinary regional Card A/<maker>-<game>-gczelda2.gci files, with game checksums, redundant quest data and a last-completed .bak copy. Randomizer saves retain randomizer-xbox.sav and the existing mod sidecars. The direct vanilla backend does not depend on a working virtual CARD mount.

This build also removes inaccessible-ancestor canonicalization from app-owned mod-install paths, rejects over-budget PNGs before decoding, reduces the Mods image cache, and prevents repeated accepted title scene requests. Hardware validation remains pending: test vanilla file selection/save/reload, the title movie transition, online downloads and repeated Mods navigation. The current crash journal is xbox-startup-stage-743.txt; previous journals remain readable.

The base stays pinned to Dusklight 2.0.1. All previous runtime fixes and all 67 strict generator scenarios remain required, along with the new production fault regressions, every native metadata range, every restored resource byte, final symbols, UWP imports and package signature.
