#include "deathcounter.h"

#include "../mkb/mkb.h"

#include "../systems/goal.h"
#include "../systems/pref.h"
#include "../utils/draw.h"
#include "../utils/libsavest.h"
#include "../utils/mode.h"
#include "../utils/patch.h"
#include "../utils/timerdisp.h"
#include "freecam.h"
#include "validate.h"

namespace deathcounter {

constexpr u16 COUNTER_DISPLAY_Y_POS = 56;
constexpr u16 COUNTER_DISPLAY_X_POS = 18;
constexpr u16 COUNTER_NUMBER_X_POS = COUNTER_DISPLAY_X_POS + 8 * 10;

constexpr u16 WORLD_COUNT = mode::WORLD_COUNT;

static u32 s_world_death_count[WORLD_COUNT] = {};
// Flag to determine when we should/shouldn't increment the death counter
static bool s_can_incr_death_counter = false;

u32 get_total_death_count() {
    u32 total = 0;
    for (u16 k = 0; k < WORLD_COUNT; k++) {
        total += s_world_death_count[k];
    }
    return total;
}

u32 get_world_death_count(u16 world_idx) { return s_world_death_count[world_idx]; }

void increment_world_death_counter() {
    // Check the pref for count first stage deaths and if we're on the first stage
    if (!pref::get(pref::BoolPref::CountFirstStageDeaths) &&
        mode::get_storymode_total_clear_count() == 0) {
        return;
    }
    s_world_death_count[mkb::scen_info.world] += 1;  // death counter for the current world
    s_can_incr_death_counter = false;
}

void reset_flags() { s_can_incr_death_counter = false; }

void reset_death_counters() {
    for (u16 k = 0; k < WORLD_COUNT; k++) {
        s_world_death_count[k] = 0;
    }
    reset_flags();
}

// When we're done holding the savestate button/when gameplay resumes
void update_flags_on_state_release() {
    if (goal::is_gameplay_exact() && !libsavest::state_loaded_this_frame()) {
        // As soon as we're done holding the load state button (or just any time we're controlling
        // the monkey on the stage), we're allowed to die
        s_can_incr_death_counter = true;
    }
}

bool should_increment_normal_death_counter() {
    bool retried_without_clearing =
        (mode::is_spin_in_init(mkb::sub_mode) && s_can_incr_death_counter);
    bool left_stage_without_clearing =
        mode::is_stage_exit_submode(mkb::sub_mode) && s_can_incr_death_counter;
    bool died = mode::is_death_init(mkb::sub_mode);
    return (retried_without_clearing || left_stage_without_clearing || died);
}

bool should_increment_savestate_death_counter() {
    return libsavest::state_loaded_this_frame() && s_can_incr_death_counter;
}

void count_deaths() {
    if (goal::is_postgoal_exact()) {
        s_can_incr_death_counter = false;
    }

    if (should_increment_normal_death_counter()) {
        increment_world_death_counter();
    }

    if (should_increment_savestate_death_counter()) {
        increment_world_death_counter();
    }
}

void tick() {
    // Set the death count to 0 on the file select screen
    if (mode::is_storymode_file_screen_init(mkb::scen_info)) {
        reset_death_counters();
    }

    // Whenever entering a new stage, reset our flags
    if (mode::is_spin_in_first_init(mkb::sub_mode)) {
        reset_flags();
    }

    count_deaths();
    update_flags_on_state_release();
}

bool should_display_death_counter() {
    u8 pref = pref::get(pref::U8Pref::DeathCounterDisplayOptions);
    switch (DeathCounterOptions(pref)) {
        case DeathCounterOptions::AlwaysShow:
            return true;
        case DeathCounterOptions::BetweenWorlds:
            return goal::is_between_worlds();
        case DeathCounterOptions::EndOfRun:
            return goal::is_run_complete();
        case DeathCounterOptions::DontShow:
            return false;
    }
}

void disp() {
    if (!mode::is_main_game_mode_story(mkb::main_game_mode) || freecam::should_hide_hud()) {
        return;
    }

    if (should_display_death_counter()) {
        draw::debug_text(COUNTER_DISPLAY_X_POS, COUNTER_DISPLAY_Y_POS, draw::WHITE, "Deaths:");
        draw::debug_text(COUNTER_NUMBER_X_POS, COUNTER_DISPLAY_Y_POS, draw::WHITE, "%d",
                         get_total_death_count());
    }
    /*
    u8 pos;
    if (should_display_death_counter()) {
        pos = 2;
    } else {
        pos = 0;
    }
    timerdisp::draw_timer(COUNTER_DISPLAY_X_POS, 1 + pos, 44, "Dbg:", 60 * 0, true, draw::WHITE);
    timerdisp::draw_timer(COUNTER_DISPLAY_X_POS, 2 + pos, 44, "Sub:", 60 * mkb::sub_mode, true,
                          draw::WHITE);
    timerdisp::draw_timer(COUNTER_DISPLAY_X_POS, 3 + pos, 44,
                          "Gol:", 60 * goal::is_postgoal_exact(), true, draw::WHITE);
    */
}

}  // namespace deathcounter
