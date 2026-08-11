#pragma once

#include "../mkb/mkb.h"

namespace validate {

void validate_run();
bool was_run_valid(bool mods_allowed);
void disable_invalidating_settings();
u32 get_framesave();
bool has_entered_goal();

void init();
void tick();

}  // namespace validate
