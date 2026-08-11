#pragma once

#include "../mkb/mkb.h"

namespace validate {

void validate_run();
bool was_run_valid(bool mods_allowed);
void disable_invalidating_settings();
u32 get_framesave();

bool has_entered_goal();
bool is_postgoal_exact();
bool is_postgoal_extended();
bool is_gameplay_exact();
bool is_between_worlds();
bool is_run_complete();

void init();
void tick();

}  // namespace validate
