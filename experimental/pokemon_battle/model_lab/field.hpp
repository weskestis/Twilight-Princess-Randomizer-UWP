#pragma once
#include "gx_renderer.hpp"
#include "mods/svc/ui.h"
#include <memory>

namespace pokemon_battle::field {
ModResult initialize(ModError* error);
ModResult shutdown();
void set_asset(std::shared_ptr<runtime::GxAsset> asset);
bool has_actors();
ModResult build_panel(UiElementHandle pane);
ModResult update_panel();
void update();
}
