# Twilight Princess Randomizer Xbox/UWP 1.4.1.748

Install `TwilightPrincessRandomizer_1.4.1.748_x64.msix` over .747 through Xbox Device Portal. Keep the same app identity and existing saves/settings. Use the bundled Microsoft VCLibs dependency if Device Portal requests it.

In **Cosmetics → Randomize Appearance**, choose:

- **Off - Keep My Colors**: use your saved equipment, heart, HUD and character colors. Individual Rainbow choices remain active.
- **Vanilla Colors**: bypass saved color/Rainbow/seed choices and use the original colors. Your choices remain saved and return when you select Off.
- **Randomize All Per Seed**, **Randomize All Colors on Every Load Screen**, or **Rainbow All**: use the corresponding automatic appearance mode.

Choosing a color under Equipment, Interface or Characters now switches appearance randomization off automatically. A manually chosen red Hero's Cap stays red across scene loads. Selecting the same color again also turns global randomization off. These controls do not disable model or gameplay mods.

Equipment textures restore immediately; some existing HUD recolors refresh on the next area load. Vanilla mode persists across app restarts. The default for new settings remains Off.

Also retains .744 pickup/shop/grotto fixes and native ports, .745 run overlays, .746 Mods-menu performance changes, and .747 Companion Tracker pairing and live sync. The Android companion APK remains a separate deliverable.

Pot and pumpkin hints now describe their container and general hint region, without internal check numbers, room numbers or stage codes. For example: "They say that the reward for a pot in the City in the Sky is a Clawshot." Generate a new seed for this wording; text already stored in older seed files is unchanged.

Enemy and Boss Soul rewards now register real item-get textboxes in the native common message group. This fixes the rejected message-group path used by rewards such as Huge Poe Soul, including Soul purchases in shops. Existing seeds get these messages too. Huge Poe Soul is an Enemy Souls permission item that unlocks Huge Poe enemies; it is separate from Jovani's regular Poe Soul count.

The missing-reward-text recovery now runs directly inside the native pickup routine and also handles destroyed message actors. It covers pot/pumpkin rewards and bound Enemy Soul pickups, including shop purchases, as well as Boss Souls. It keeps normal item-animation cleanup and the existing exact-once reward/save binding.

This is a signed Xbox test build. Full compile, production cosmetics regressions, all 67 strict generator scenarios and package checks run before delivery. Physical Xbox validation remains pending.
