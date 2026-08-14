#pragma once

#include "../mkb/mkb.h"

namespace storytimer {

/* enum class TimerOptions {
    DontShow = 0,
    AlwaysShow = 1,
    BetweenWorlds = 2,
    EndOfRun = 3,
}; */

enum class TimerType {
    Fullgame,
    Segment,
};

struct TimerGroup {
    u32 segment;     // the time taken to complete a world up until tape break on the last stage
    u32 full_world;  // the time taken to complete a world until the fade to white on the last stage
};

struct TimerVersion {
    u16 major;
    u16 minor;
    u16 patch;
};

constexpr TimerVersion LOADLESS_TIMER_VERSION = {0, 2, 0};

TimerGroup get_world_timer_info(u16 world_idx);
u32 get_split_timer_for_world(u16 world_idx);
u32 get_loadless_time();

void init_main_loop();
void tick();
void disp();

}  // namespace storytimer