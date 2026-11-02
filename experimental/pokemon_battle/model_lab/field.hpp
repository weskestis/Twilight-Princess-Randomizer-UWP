#pragma once
#include "gx_renderer.hpp"
#include "mods/svc/ui.h"
#include <memory>

namespace pokemon_battle::field {
ModResult initialize(ModError* error);
ModResult shutdown();
bool has_actors();
ModResult build_panel(UiElementHandle pane);
ModResult update_panel();
void update();
}
