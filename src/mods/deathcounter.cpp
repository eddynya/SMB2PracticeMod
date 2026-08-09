#include "deathcounter.h"

#include "../mkb/mkb.h"

#include "../systems/pref.h"
#include "../utils/draw.h"
#include "../utils/libsavest.h"
#include "../utils/mode.h"
#include "../utils/patch.h"
#include "../utils/timerdisp.h"
#include "freecam.h"
#include "mods/old_storytimer.h"
#include "validate.h"

namespace deathcounter {

constexpr u16 COUNTER_DISPLAY_Y_POS = 56;
constexpr u16 COUNTER_DISPLAY_X_POS = 18;
constexpr u16 COUNTER_NUMBER_X_POS = COUNTER_DISPLAY_X_POS + 8 * 10;

constexpr u16 WORLD_COUNT = 10;

static u32 s_world_death_count[WORLD_COUNT] = {};
static bool s_can_incr_death_counter = false;  // flag for if we can increment the death counter

static bool s_ignore_retry_flag = false;
static bool s_has_incremented_death_counter = false;
static bool s_is_holding_load_state = false;
static bool s_ignore_state_load_flag = false;

static bool s_can_die;
static u32 s_death_count;

u32 get_total_death_count() {
    u32 total = 0;
    for (u16 k = 0; k < WORLD_COUNT; k++) {
        total += s_world_death_count[k];
    }
    return total;
}

u32 get_world_death_count(u16 world_idx) { return s_world_death_count[world_idx]; }

void increment_world_death_counter() {
    s_world_death_count[mkb::scen_info.world] += 1;  // death counter for the current world
    s_has_incremented_death_counter = true;  // set the flag to be true when calling this function
}

// When we're done holding the savestate button/when gameplay resumes
void update_flags_on_state_release() {
    if (mode::is_gameplay(mkb::sub_mode) && !libsavest::state_loaded_this_frame()) {
        s_has_incremented_death_counter = false;
        s_ignore_retry_flag = false;
        s_is_holding_load_state = false;
        s_ignore_state_load_flag = false;
    }
}

bool should_increment_normal_death_counter() {
    bool retried_without_clearing = (mode::is_spin_in_init(mkb::sub_mode) && !s_ignore_retry_flag);
    bool left_stage_without_clearing =
        mode::is_stage_exit_submode(mkb::sub_mode) && !s_ignore_retry_flag;
    bool died = mode::is_death_init(mkb::sub_mode);
    return (retried_without_clearing || left_stage_without_clearing || died);
}

// "Normal" deaths refer to non-savestate related deaths
void update_normal_deaths() {
    if (validate::has_entered_goal()) {
        s_ignore_retry_flag = true;
    }

    if (should_increment_normal_death_counter() && !s_has_incremented_death_counter) {
        increment_world_death_counter();
        // s_has_incremented_death_counter = true;
    }

    // if we load state, we want to be able to still detect normal deaths (e.g. touching a fallout
    // volume), so unset the flag once we enter normal gameplay again
    if (mode::is_gameplay(mkb::sub_mode) && !libsavest::state_loaded_this_frame()) {
        // s_has_incremented_death_counter = false;
        // s_ignore_retry_flag = false;
    }
}

bool should_increment_state_death_counter() {
    return libsavest::state_loaded_this_frame() && !s_ignore_state_load_flag &&
           !s_is_holding_load_state;
}

void update_savestate_deaths() {
    if (validate::has_entered_goal()) {
        s_ignore_state_load_flag = true;
    }

    if (libsavest::state_loaded_this_frame() && !s_ignore_state_load_flag &&
        !s_is_holding_load_state && !s_has_incremented_death_counter) {
        increment_world_death_counter();
        s_is_holding_load_state = true;
        // s_has_incremented_death_counter = true;
    }

    if (mode::is_gameplay(mkb::sub_mode) && !libsavest::state_loaded_this_frame()) {
        // s_is_holding_load_state = false;
        // s_ignore_state_load_flag = false;
    }
}

void tick() {
    // set the death count to 0 on the file select screen
    if (mode::is_storymode_file_screen_init(mkb::scen_info)) {
        s_death_count = 0;
        s_can_die = false;
        for (u16 k = 0; k < WORLD_COUNT; k++) {
            s_world_death_count[k] = 0;
        }
    }

    update_normal_deaths();
    update_savestate_deaths();
    update_flags_on_state_release();

    for (u16 k = 0; k < WORLD_COUNT; k++) {
        // update_world_death_counter(k);
    }

    // Don't increment the death counter on stage 1 if the setting is ticked
    /*
    if (mkb::sub_mode == mkb::SMD_GAME_PLAY_MAIN && !validate::has_entered_goal()) {
        if (pref::get(pref::BoolPref::CountFirstStageDeaths)) {
            s_can_die = true;
        } else if (!pref::get(pref::BoolPref::CountFirstStageDeaths) &&
                   old_storytimer::get_completed_stagecount() != 0) {
            s_can_die = true;
        }
    } else if (validate::has_entered_goal()) {
        s_can_die = false;
    } */
    /* if (s_can_die &&
        (mkb::sub_mode == mkb::SMD_GAME_READY_INIT || mkb::sub_mode == mkb::SMD_GAME_RINGOUT_INIT ||
         mkb::sub_mode == mkb::SMD_GAME_TIMEOVER_INIT ||
         mkb::sub_mode == mkb::SMD_GAME_SCENARIO_RETURN ||
         mkb::sub_mode == mkb::SMD_GAME_INTR_SEL_INIT)) {
        // you can die either by retrying after dropping in, falling out, timing over, stage
        // selecting after dropping in (but before breaking the tape), or exiting game after
        // dropping in (but before breaking the tape)
        s_death_count += 1;
        s_can_die = false;  // once the death counter is incremented, set this to false so we only
                            // increment it by 1
    } */
}

void disp() {
    if (!mode::is_main_game_mode_story(mkb::main_game_mode) || freecam::should_hide_hud() ||
        !pref::get(pref::BoolPref::ShowDeathCounter)) {
        return;
    }
    draw::debug_text(COUNTER_DISPLAY_X_POS, COUNTER_DISPLAY_Y_POS, draw::WHITE, "Deaths: ");
    draw::debug_text(COUNTER_NUMBER_X_POS, COUNTER_DISPLAY_Y_POS, draw::WHITE, "%d",
                     get_total_death_count());
}

void update_world_death_counter(u16 world_idx) {
    if (mode::is_gameplay_init(mkb::sub_mode)) {
        s_can_incr_death_counter = true;  // allowed to increment the counter after dropping in
    }
    //
    if (libsavest::state_loaded_this_frame() && !mode::is_postgoal(mkb::sub_mode)) {
        s_world_death_count[world_idx] += 1;
    }

    if (mode::is_death_init(mkb::sub_mode) || mode::is_stage_exit_init(mkb::sub_mode)) {
    }
}

}  // namespace deathcounter
