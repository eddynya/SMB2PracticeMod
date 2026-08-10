#include "deathcounter.h"

#include "../mkb/mkb.h"

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

// Flags to determine when we should/shouldn't increment the death counter

// "Normal" deaths refer to non-savestate related deaths
// Default state for the normal deaths flag should be true when entering a stage since retrying
// before first drop in shouldn't count as a death
static bool s_ignore_normal_deaths_flag = true;
static bool s_ignore_state_load_flag = false;
static bool s_has_incremented_death_counter = false;

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
    s_has_incremented_death_counter =
        true;  // set the flag to be true when calling this function (if no early return)
}

void reset_flags() {
    s_ignore_normal_deaths_flag = true;
    s_ignore_state_load_flag = false;
    s_has_incremented_death_counter = false;
}

void reset_death_counters() {
    for (u16 k = 0; k < WORLD_COUNT; k++) {
        s_world_death_count[k] = 0;
    }
    reset_flags();
}

// When we're done holding the savestate button/when gameplay resumes
void update_flags_on_state_release() {
    if (mode::is_gameplay(mkb::sub_mode) && !libsavest::state_loaded_this_frame()) {
        // As soon as we're done holding the load state button (or just any time we're controlling
        // the monkey on the stage), we're allowed to die
        s_has_incremented_death_counter = false;
        s_ignore_normal_deaths_flag = false;
        s_ignore_state_load_flag = false;
    }
}

bool should_increment_normal_death_counter() {
    bool retried_without_clearing =
        (mode::is_spin_in_init(mkb::sub_mode) && !s_ignore_normal_deaths_flag);
    bool left_stage_without_clearing =
        mode::is_stage_exit_submode(mkb::sub_mode) && !s_ignore_normal_deaths_flag;
    bool died = mode::is_death_init(mkb::sub_mode);
    return (retried_without_clearing || left_stage_without_clearing || died);
}

void update_normal_deaths() {
    if (validate::has_entered_goal()) {
        s_ignore_normal_deaths_flag = true;
    }

    if (should_increment_normal_death_counter() && !s_has_incremented_death_counter) {
        increment_world_death_counter();
    }
}

bool should_increment_savestate_death_counter() {
    return libsavest::state_loaded_this_frame() && !s_ignore_state_load_flag;
}

void update_savestate_deaths() {
    if (validate::has_entered_goal()) {
        s_ignore_state_load_flag = true;
    }

    if (should_increment_savestate_death_counter() && !s_has_incremented_death_counter) {
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

    update_normal_deaths();
    update_savestate_deaths();
    update_flags_on_state_release();
}

bool should_display_death_counter() {
    u8 pref = pref::get(pref::U8Pref::DeathCounterDisplayOptions);
    switch (DeathCounterOptions(pref)) {
        case DeathCounterOptions::AlwaysShow:
            return true;
        case DeathCounterOptions::BetweenWorlds:
            return validate::is_between_worlds();
        case DeathCounterOptions::EndOfRun:
            return validate::is_run_complete();
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
}

}  // namespace deathcounter
