#pragma once

#include "../mkb/mkb.h"

namespace storyreset {

bool is_run_active();
void set_run_active_status(bool is_active);

u8 get_active_file_index();
u8 get_active_world();
void reset_active_run_info();
bool should_reset_run();
void display_reset_run_message();

void tick();
void disp();

}  // namespace storyreset
