#include "mods/svc/ui.h"

#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

extern const UiService* svc_ui;
extern "C" ModResult mod_initialize(ModError*);
extern "C" ModResult mod_update(ModError*);
extern "C" ModResult mod_shutdown(ModError*);

#define CHECK(c) do { if (!(c)) throw std::runtime_error(#c); } while (false)
namespace {
UiModsPanelDesc panel{};
std::map<UiElementHandle, std::string> texts;
std::map<std::string, UiControlDesc> controls;
UiElementHandle next_handle = 1;
int writes = 0;
int fail_at = 0;
int adds = 0;
ModResult registered(ModContext*, const UiModsPanelDesc* value) { panel = *value; return MOD_OK; }
ModResult section(ModContext*, UiElementHandle, const char*) { return MOD_OK; }
ModResult text(ModContext*, UiElementHandle, const char* value, UiElementHandle* output) {
    if (++adds == fail_at) return MOD_ERROR;
    const auto id = next_handle++; texts[id] = value;
    if (output) *output = id;
    return MOD_OK;
}
ModResult control(ModContext*, UiElementHandle, const UiControlDesc* value, UiElementHandle*) {
    controls[value->label] = *value; return MOD_OK;
}
ModResult set_text(ModContext*, UiElementHandle handle, const char* value) {
    if (!texts.contains(handle)) return MOD_INVALID_ARGUMENT;
    texts[handle] = value; ++writes; return MOD_OK;
}
bool contains(const std::string& needle) {
    for (const auto& [id, value] : texts) { (void)id; if (value.find(needle) != std::string::npos) return true; }
    return false;
}
bool enabled(const char* label) {
    const auto& c = controls.at(label);
    return !c.is_disabled || !c.is_disabled(nullptr, c.user_data);
}
void press(const char* label) {
    CHECK(enabled(label)); const auto c = controls.at(label);
    c.on_pressed(nullptr, c.user_data); CHECK(panel.update(nullptr, nullptr, nullptr) == MOD_OK);
}
void rebuild() {
    texts.clear(); controls.clear(); adds = 0;
    CHECK(panel.build(nullptr, 42, nullptr, nullptr) == MOD_OK);
}
} // namespace

int main() {
    try {
        CHECK(mod_initialize(nullptr) == MOD_UNAVAILABLE);
        UiService service{};
        service.register_mods_panel = registered;
        service.pane_add_section = section;
        service.pane_add_text = text;
        service.pane_add_control = control;
        service.elem_set_text = set_text;
        svc_ui = &service;
        CHECK(mod_initialize(nullptr) == MOD_OK); CHECK(panel.build && panel.update);
        rebuild(); CHECK(contains("simulation only"));
        CHECK(enabled("Start encounter")); CHECK(!enabled("Sword")); CHECK(!enabled("Continue"));
        const int old_writes = writes;
        for (int i = 0; i < 120; ++i) CHECK(panel.update(nullptr, nullptr, nullptr) == MOD_OK);
        CHECK(writes == old_writes); // No per-frame text churn while nothing changes.
        press("Start encounter"); CHECK(contains("wild Pikachu"));
        CHECK(!enabled("Start encounter")); CHECK(!enabled("Sword"));
        press("Continue"); CHECK(enabled("Sword")); CHECK(!enabled("Continue"));
        press("Bomb"); CHECK(contains("Bombs 1")); CHECK(!enabled("Bomb"));
        rebuild(); CHECK(contains("Bombs 1")); CHECK(enabled("Continue"));
        press("Continue"); CHECK(contains("Pikachu used")); CHECK(!enabled("Sword"));
        press("Continue");
        int budget = 40;
        while (!enabled("Start encounter") && --budget) {
            if (enabled("Continue")) press("Continue");
            else if (enabled("Bomb")) press("Bomb");
            else press("Sword");
        }
        CHECK(budget > 0); CHECK(contains("Victory!")); CHECK(enabled("Start encounter"));
        press("Start encounter"); press("Continue"); press("Flee");
        press("Continue"); press("Continue"); CHECK(contains("Returned from"));
        press("Start encounter"); press("End test encounter"); CHECK(contains("cancelled"));
        CHECK(enabled("Start encounter"));
        fail_at = 2; adds = 0;
        CHECK(panel.build(nullptr, 42, nullptr, nullptr) == MOD_ERROR);
        fail_at = 0; rebuild();
        CHECK(mod_update(nullptr) == MOD_OK); CHECK(mod_shutdown(nullptr) == MOD_OK);
        CHECK(mod_initialize(nullptr) == MOD_OK); rebuild(); CHECK(enabled("Start encounter"));
        CHECK(mod_shutdown(nullptr) == MOD_OK);
        std::cout << "PASS native SDK panel: lifecycle, controls, victory, escape, cancellation, stale handles, idle refresh, build failure\n";
    } catch (const std::exception& error) { std::cerr << "FAIL UI " << error.what() << '\n'; return 1; }
}
