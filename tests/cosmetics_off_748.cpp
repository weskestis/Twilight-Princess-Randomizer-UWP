#include "appearance_runtime.hpp"
#include "dusk/config_var.hpp"
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <limits>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Link the real ConfigVar layer methods without file IO or UI subscribers.
namespace dusk::config {
ConfigVarBase::ConfigVarBase(std::string key, const ConfigImplBase* implementation)
    : name(std::move(key)), registered(true), layer(ConfigVarLayer::Default), impl(implementation) {}
ConfigVarBase::~ConfigVarBase() = default;
bool ConfigVarBase::has_subscribers() const { return false; }
void ConfigVarBase::notify_changed(const void*) {}
const char* ConfigVarBase::getName() const noexcept { return name.c_str(); }
template <ConfigValue T> const ConfigImplBase* GetConfigImpl() { return nullptr; }
}
using dusk::config::ConfigVar;
namespace dusk {
struct Settings {
    struct {
        ConfigVar<std::string> menuAccentColor{"cosmetics.menuAccentColor", ""};
        ConfigVar<std::string> menuBackgroundColor{"cosmetics.menuBackgroundColor", ""};
        ConfigVar<std::string> menuTextColor{"cosmetics.menuTextColor", ""};
        ConfigVar<std::string> menuBorderColor{"cosmetics.menuBorderColor", ""};
        ConfigVar<bool> vanillaAppearance{"cosmetics.vanillaAppearance", false};
        ConfigVar<bool> rainbowAllColors{"cosmetics.rainbowAllColors", false};
        ConfigVar<bool> randomizeAllColorsPerSeed{"cosmetics.randomizeAllColorsPerSeed", false};
        ConfigVar<bool> randomizeAllColorsEveryLoad{"cosmetics.randomizeAllColorsEveryLoad", false};
        ConfigVar<std::string> perSeedRandomizedColors{"cosmetics.perSeedRandomizedColors", ""};
    } cosmetics;
};
Settings settings;
Settings& getSettings() { return settings; }
namespace config {
unsigned saves = 0;
std::array<bool, 4> persisted{};
void save() {
    const auto& colors = settings.cosmetics;
    persisted = {colors.vanillaAppearance.getValueForSave(), colors.rainbowAllColors.getValueForSave(),
        colors.randomizeAllColorsPerSeed.getValueForSave(), colors.randomizeAllColorsEveryLoad.getValueForSave()};
    ++saves;
}
}
namespace ui {
/* ACTUAL_MENU_TYPES */
RuntimeState state;
unsigned themes = 0, rolls = 0;
RuntimeState& runtime_state() { return state; }
bool per_seed_enabled(const ConfigVar<std::string>& option) {
    return settings.cosmetics.perSeedRandomizedColors.getValueForSave().find(option.getName()) != std::string::npos;
}
bool saved_menu_rainbow();
void apply_menu_theme() noexcept { ++themes; }
MenuPalette rainbow_palette(std::uint64_t) {
    return MenuPalette{{"00ff00", "001000", "ffffff", "77ff77"}};
}
/* ACTUAL_MENU_FUNCTIONS */
void refresh_menu_colors() noexcept { update_menu_colors_impl(true); }
void randomize_menu_colors_for_load_screen() noexcept { ++rolls; }
}
}
/* ACTUAL_HOST_MODES */

using ConfigVarHandle = uint64_t;
using ModResult = int;
struct ModContext {};
struct ModError {};
using UiWindowHandle = uint64_t;
using UiElementHandle = uint64_t;
struct UiControlValue { int64_t int_value = 0; const char* string_value = nullptr; };
struct UiControlDesc {
    int kind = 0;
    const char* label = nullptr;
    const char* help_rml = nullptr;
    const char* const* options = nullptr;
    size_t option_count = 0;
    void (*get)(ModContext*, void*, UiControlValue*) = nullptr;
    void (*set)(ModContext*, void*, const UiControlValue*) = nullptr;
    int binding = 0;
    ConfigVarHandle config_var = 0;
    bool (*is_modified)(ModContext*, void*) = nullptr;
    void* user_data = nullptr;
    const char* const* color_presets = nullptr;
    size_t color_preset_count = 0;
};
#define UI_CONTROL_DESC_INIT {}
#define UI_CONTROL_SELECT 1
#define UI_CONTROL_COLOR 2
#define UI_BINDING_CONFIG_VAR 1
#define UI_BINDING_CALLBACKS 0
#define MOD_OK 0
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
UiControlDesc control;
void add_control(UiElementHandle, const UiControlDesc& value) { control = value; }
/* ACTUAL_UI_CALLBACKS */

void* mod_ctx = nullptr;
unsigned reads = 0;
struct ConfigServiceFixture {
    std::array<std::string, 6> colors{{"", "135724", "rainbow", "abcdef", "", "234567"}};
    int64_t hair = 6;
    bool failWrites = false;
    ModResult get_string(void*, ConfigVarHandle handle, char* output, size_t capacity, size_t* length) {
        ++reads;
        if (handle >= colors.size()) return 1;
        const auto& value = colors[static_cast<size_t>(handle)];
        *length = value.size();
        if (output) { assert(capacity > value.size()); std::memcpy(output, value.c_str(), value.size() + 1); }
        return MOD_OK;
    }
    ModResult get_int(void*, ConfigVarHandle, int64_t* value) { ++reads; *value = hair; return MOD_OK; }
    ModResult set_string(void*, ConfigVarHandle handle, const char* value) {
        if (failWrites) return 1;
        colors.at(static_cast<size_t>(handle)) = value; return MOD_OK;
    }
    ModResult set_int(void*, ConfigVarHandle, int64_t value) {
        if (failWrites) return 1;
        hair = value; return MOD_OK;
    }
} service;
auto* svc_config = &service;
dusk_xbox_appearance::Runtime gAppearanceRuntime;
/* ACTUAL_MOD_GETTERS */
/* ACTUAL_MANUAL_COLOR_CALLBACKS */

int main() {
    using namespace dusk_xbox_appearance;
    using namespace dusk::ui;
    const std::vector<Target> targets{{1, "herosTunicCapColor"}, {2, "heartColor"},
        {3, "aButtonColor"}, {4, "midnaHairBaseColor", true}, {5, "eponaColor"}};
    const auto saved = service.colors;
    auto& s = dusk::settings.cosmetics;
    s.menuAccentColor.setValue("4da3ff");
    s.menuTextColor.setValue("rainbow");
    s.perSeedRandomizedColors.setValue("|cosmetics.menuBackgroundColor|");
    state.seedHash = "seed-a"; state.seedMask = kAllRoles;
    state.seedRainbow = true; state.loadActive = true; state.loadRainbow = true;
    gAppearanceRuntime.sync(targets, Mode::Load, 1, "seed-a", 0, 1);
    tpr_appearance_set_mode(2u);
    assert(tpr_appearance_mode() == 2u);

    // Selecting vanilla must work even before the next mod update, despite stale randomized state.
    tpr_appearance_set_mode(4u);
    assert(tpr_appearance_mode() == 4u);
    assert(!s.rainbowAllColors.getValue() && !s.randomizeAllColorsPerSeed.getValue() && !s.randomizeAllColorsEveryLoad.getValue());
    const auto readsBefore = reads;
    assert(get_str_option(1, "bad") == "" && get_str_option(2, "bad") == "");
    assert(get_int_option(4, 9) == 0 && reads == readsBefore);
    assert(selected_seed_mask() == 0);
    for (unsigned scene = 0; scene < 1000; ++scene) {
        gAppearanceRuntime.sync(targets, static_cast<Mode>(4u), scene, "seed-b", scene * 1000ULL, 2);
        assert(*gAppearanceRuntime.string(1) == "" && *gAppearanceRuntime.integer(4) == 0);
        assert(!gAppearanceRuntime.is_rainbow(2));
        const auto previousThemes = themes;
        update_menu_colors_impl(false);
        assert(themes == previousThemes);
        const auto defaults = default_palette();
        const auto vars = color_vars();
        for (size_t i = 0; i < vars.size(); ++i) assert(vars[i]->getValue() == defaults.colors[i]);
    }
    assert(s.menuAccentColor.getValueForSave() == "4da3ff" && s.menuTextColor.getValueForSave() == "rainbow");
    assert(s.perSeedRandomizedColors.getValueForSave() == "|cosmetics.menuBackgroundColor|");
    assert(service.colors == saved);

    // Simulate app restart by loading the values emitted by the production setter's save call.
    const auto persisted = dusk::config::persisted;
    s.vanillaAppearance.setValue(false); s.randomizeAllColorsEveryLoad.setValue(true);
    s.vanillaAppearance.setValue(persisted[0]); s.rainbowAllColors.setValue(persisted[1]);
    s.randomizeAllColorsPerSeed.setValue(persisted[2]); s.randomizeAllColorsEveryLoad.setValue(persisted[3]);
    gAppearanceRuntime.clear();
    assert(tpr_appearance_mode() == 4u && get_str_option(2, "bad") == "");
    tpr_appearance_set_mode(0u);
    assert(tpr_appearance_mode() == 0u);
    assert(get_str_option(1, "") == saved[1] && get_str_option(2, "") == saved[2]);
    assert(get_int_option(4, 0) == 6 && !s.vanillaAppearance.getValue());

    // The actual five-choice controller control must map both ways without shifting old modes.
    build_appearance_tab(nullptr, 0, 0, 0, nullptr, nullptr);
    assert(control.option_count == 5);
    assert(std::string_view(control.options[0]) == "Off - Keep My Colors");
    assert(std::string_view(control.options[1]) == "Vanilla Colors");
    const std::array<unsigned, 5> modes{{0, 4, 1, 2, 3}};
    for (size_t i = 0; i < modes.size(); ++i) {
        UiControlValue selected{static_cast<int64_t>(i)};
        control.set(nullptr, nullptr, &selected);
        assert(tpr_appearance_mode() == modes[i]);
        UiControlValue echoed{-1}; control.get(nullptr, nullptr, &echoed);
        assert(echoed.int_value == selected.int_value);
    }
    const auto savesBefore = dusk::config::saves;
    UiControlValue invalid{-1}; control.set(nullptr, nullptr, &invalid);
    invalid.int_value = 5; control.set(nullptr, nullptr, &invalid);
    tpr_appearance_set_mode(5u);
    assert(dusk::config::saves == savesBefore && tpr_appearance_mode() == 3u);

    for (const unsigned mode : {1u, 2u, 3u}) {
        tpr_appearance_set_mode(mode);
        gAppearanceRuntime.sync(targets, static_cast<Mode>(mode), 123, "seed-a", 0, 1);
        assert(!get_str_option(1, "").empty() && !s.vanillaAppearance.getValue());
        tpr_appearance_set_mode(0u);
        // Stale generated values must not delay Off's saved-color restoration.
        assert(get_str_option(1, "") == saved[1]);
    }
    assert(service.colors == saved);
    // Selecting a manual red cap must exit each automatic mode, including an unchanged saved value.
    constexpr const char* presets[]{"ff0000", "rainbow"};
    ConfigVarHandle cap = 1, midna = 4;
    for (const unsigned mode : {1u, 2u, 3u, 4u}) {
        tpr_appearance_set_mode(mode);
        gAppearanceRuntime.sync(targets, static_cast<Mode>(mode), 1, "seed-a", 1000, 1);
        add_cosmetic_option(0, cap, "Hero's Cap", presets);
        assert(control.binding == UI_BINDING_CALLBACKS && control.user_data == &cap);
        UiControlValue red{}; red.string_value = "ff0000";
        control.set(nullptr, control.user_data, &red);
        assert(tpr_appearance_mode() == 0u && get_str_option(cap, "") == "ff0000");
        UiControlValue echoed{}; control.get(nullptr, control.user_data, &echoed);
        assert(std::string_view(echoed.string_value) == "ff0000");
        assert(control.is_modified(nullptr, control.user_data));
        for (unsigned scene = 0; scene < 1000; ++scene) {
            gAppearanceRuntime.sync(targets, static_cast<Mode>(tpr_appearance_mode()), scene, "seed-b", scene * 1000ULL, 2);
            assert(get_str_option(cap, "") == "ff0000");
        }
        tpr_appearance_set_mode(mode);
        add_midna_hair_option(0, midna, "Midna Hair");
        UiControlValue hair{2}; control.set(nullptr, control.user_data, &hair);
        assert(tpr_appearance_mode() == 0u && get_int_option(midna, 0) == 2);
    }
    tpr_appearance_set_mode(2u); service.failWrites = true;
    add_cosmetic_option(0, cap, "Hero's Cap", presets);
    UiControlValue failed{}; failed.string_value = "00ff00";
    control.set(nullptr, control.user_data, &failed);
    assert(tpr_appearance_mode() == 2u && service.colors[1] == "ff0000");
    service.failWrites = false;
    puts("PASS: production UI/mode/getters, 1000 vanilla scenes, stale override bypass, real ConfigVar saved-color preservation, restart and random-mode return");
    puts("PASS: manual red cap holds across 4000 scene loads, repeated same-color edits exit automatic modes, Midna edits and failed writes");
}
