#pragma once

#include "../mkb/mkb.h"

namespace storyreset {

u8 get_active_file_index();
u8 get_active_world();
void reset_active_run_info();
bool should_reset_run();

void tick();
void disp();

}  // namespace storyreset
