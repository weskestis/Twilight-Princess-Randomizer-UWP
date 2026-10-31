#include "pokemon_battle/battle.hpp"
#include "mods/service.hpp"
#include "mods/svc/ui.h"

#include <array>
#include <string>

DEFINE_MOD();
IMPORT_SERVICE(UiService, svc_ui);

namespace {
using namespace pokemon_battle;
Battle battle;
std::uint32_t next_seed = 749;
UiElementHandle status_handle = 0;
UiElementHandle event_handle = 0;
bool dirty = true;

struct Button { const char* label; Command command; };
std::array<Button, 5> buttons{{
    {"Sword", Command::Sword}, {"Bow", Command::Bow}, {"Bomb", Command::Bomb},
    {"Guard", Command::Guard}, {"Flee", Command::Flee}
}};

bool cannot_start(ModContext*, void*) { return battle.active(); }
bool cannot_advance(ModContext*, void*) { return battle.event() == nullptr; }
bool cannot_cancel(ModContext*, void*) { return !battle.active(); }
bool cannot_act(ModContext*, void* data) {
    return !battle.can_submit(static_cast<Button*>(data)->command);
}
void start(ModContext*, void*) { if (battle.start(pikachu_demo(next_seed))) { ++next_seed; dirty = true; } }
void advance(ModContext*, void*) {
    if (const auto* event = battle.event()) { battle.acknowledge(event->serial); dirty = true; }
}
void act(ModContext*, void* data) {
    if (battle.submit(static_cast<Button*>(data)->command)) dirty = true;
}
void cancel(ModContext*, void*) { battle.cancel(); dirty = true; }

ModResult refresh(ModContext*, void*, ModError*) {
    if (!dirty || status_handle == 0 || event_handle == 0) return MOD_OK;
    const auto& state = battle.state();
    std::string status = "Start a simulated encounter.";
    if (battle.phase() != Phase::Idle) {
        status = state.player.name + " HP " + std::to_string(state.player.hp) + "/" +
            std::to_string(state.player.max_hp) + "  |  " + state.enemy.name + " HP " +
            std::to_string(state.enemy.hp) + "/" + std::to_string(state.enemy.max_hp) +
            "  |  Arrows " + std::to_string(state.arrows) + "  Bombs " + std::to_string(state.bombs);
    }
    const std::string message = battle.event() ? battle.event()->text :
        battle.phase() == Phase::PlayerInput ? "Choose Link's action." :
        battle.phase() == Phase::Finished ? outcome_name(battle.outcome()) : "Ready.";
    auto result = svc_ui->elem_set_text(mod_ctx, status_handle, status.c_str());
    if (result != MOD_OK) return result;
    result = svc_ui->elem_set_text(mod_ctx, event_handle, message.c_str());
    if (result == MOD_OK) dirty = false;
    return result;
}

ModResult add_button(UiElementHandle pane, const char* label, UiPressedFn pressed,
                     UiPredicateFn disabled, void* data = nullptr) {
    UiControlDesc control = UI_CONTROL_DESC_INIT;
    control.label = label;
    control.on_pressed = pressed;
    control.is_disabled = disabled;
    control.user_data = data;
    return svc_ui->pane_add_control(mod_ctx, pane, &control, nullptr);
}

ModResult build(ModContext*, UiElementHandle pane, void*, ModError*) {
    // Panels are rebuilt on tab changes. Never retain handles from the old pane.
    status_handle = event_handle = 0;
    dirty = true;
    auto result = svc_ui->pane_add_section(mod_ctx, pane, "Pokemon Battle Lab - CP1");
    if (result != MOD_OK) return result;
    result = svc_ui->pane_add_text(mod_ctx, pane,
        "Combat simulation only. Uses test HP and items; no save changes, overworld enemies, or 3D model yet.", nullptr);
    if (result != MOD_OK) return result;
    result = svc_ui->pane_add_text(mod_ctx, pane, "", &status_handle);
    if (result != MOD_OK) return result;
    result = svc_ui->pane_add_text(mod_ctx, pane, "", &event_handle);
    if (result != MOD_OK) return result;
    result = add_button(pane, "Start encounter", start, cannot_start);
    if (result != MOD_OK) return result;
    result = add_button(pane, "Continue", advance, cannot_advance);
    if (result != MOD_OK) return result;
    for (auto& button : buttons) {
        result = add_button(pane, button.label, act, cannot_act, &button);
        if (result != MOD_OK) return result;
    }
    result = add_button(pane, "End test encounter", cancel, cannot_cancel);
    if (result != MOD_OK) return result;
    return refresh(nullptr, nullptr, nullptr);
}
} // namespace

extern "C" {
MOD_EXPORT ModResult mod_initialize(ModError* error) {
    if (!svc_ui) return mods::set_error(error, MOD_UNAVAILABLE, "UI service is required");
    UiModsPanelDesc panel = UI_MODS_PANEL_DESC_INIT;
    panel.build = build;
    panel.update = refresh;
    return svc_ui->register_mods_panel(mod_ctx, &panel);
}
MOD_EXPORT ModResult mod_update(ModError*) { return MOD_OK; }
MOD_EXPORT ModResult mod_shutdown(ModError*) {
    battle.cancel();
    status_handle = event_handle = 0;
    dirty = true;
    return MOD_OK;
}
}
