#pragma once

#include "../mkb/mkb.h"

namespace storyreset {

/* enum class TimerOptions {
    DontShow = 0,
    AlwaysShow = 1,
    BetweenWorlds = 2,
    EndOfRun = 3,
}; */

// Does it make more sense to move this enum class to pref.h?
// We need this enum class in storyreset.cpp, but we also need it in both deathcounter.cpp and
// storytimer.cpp
enum class StoryDisplayOptions {
    DontShow = 0,
    AlwaysShow = 1,
    BetweenWorlds = 2,
    EndOfRun = 3,
};

// Loadless timer and deathcounter share the same display options! type alias them:
// using TimerOptions = StoryDisplayOptions;
// using DeathCounterOptions = StoryDisplayOptions;

bool is_run_active();
void set_run_active_status(bool is_active);

u8 get_active_file_index();
u8 get_active_world();
void reset_active_run_info();
bool should_reset_run();
void display_reset_run_message();
bool all_loadless_timer_prefs_off();

void tick();

}  // namespace storyreset
