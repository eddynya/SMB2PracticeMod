#pragma once

#include "../mkb/mkb.h"

namespace goal {

u8 get_frames_until_goal_submode();
bool is_postgoal_exact();
bool is_gameplay_exact();
bool is_postgoal_extended();
bool is_between_worlds();
bool is_run_complete();

void init();
void tick();

}  // namespace goal
