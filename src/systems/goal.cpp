#include "goal.h"

#include "../mkb/mkb.h"
// #include "../utils/draw.h"
#include "../utils/libsavest.h"
#include "../utils/mode.h"
#include "../utils/patch.h"
// #include "../utils/timerdisp.h"

namespace goal {

// Utility file that provides some functions that deal with and are based off of giving precise goal
// checks (ie the moment we break the tape and not delayed like the goal submodes are)
// This grew out of validate.cpp and was separated into its own file to avoid conflicting interests
// Namely, this file should be very low in the include chain to allow as many other files to use it

constexpr u16 WORLD_COUNT = mode::WORLD_COUNT;
constexpr u16 STAGES_PER_WORLD = mode::STAGES_PER_WORLD;
constexpr u8 TIME_BETWEEN_TAPE_BREAK_AND_GOAL_SUBMODE = 3;
static u8 s_frames_until_goal_submode = 0;

// Various goal flags that get set to true on goal tape break (and stay true during postgoal), but
// get unset at different times
static bool s_goal_flag_game_return;  // Unset after game scenario return
static bool s_goal_flag_exit_game;    // Unset outside of exit game or game scenario return
static bool s_goal_flag_retry;        // Unset on spin in (or gameplay pre tape break)
static bool s_goal_flag_last_stage;  // Unset on spin in, gameplay pre tape break, and game scenario
                                     // main

static patch::Tramp<decltype(&mkb::did_ball_enter_goal)> s_goal_tramp;
static patch::Tramp<decltype(&mkb::mode_tick)> s_mode_tick_tramp;

u8 get_frames_until_goal_submode() { return s_frames_until_goal_submode; }
bool get_goal_flag_game_return() { return s_goal_flag_game_return; }
bool get_goal_flag_exit_game() { return s_goal_flag_exit_game; }
bool get_goal_flag_retry() { return s_goal_flag_retry; }
bool get_goal_flag_last_stage() { return s_goal_flag_last_stage; }

// Important note on run order
// The run order of various relevant functions for us here is as follows:
// (1) mkb::process_inputs() (what prac mod tick() functions hook)
// (2) mkb::mode_tick() (changes the submode)
// (3) mkb::did_ball_enter_goal()
// (4) mkb::get_world_unbeaten_stage_count() (used to determine clear count, and if we're between
//     worlds/at the end of a story mode run)
// (5) mkb::draw_debugtext (what prac mod disp() functions hook)

void set_goal_flags() {
    s_goal_flag_game_return = true;
    s_goal_flag_exit_game = true;
    s_goal_flag_retry = true;
    s_goal_flag_last_stage = true;
}

// Remedy for goal submode checks being delayed from tape break
bool is_postgoal_exact() {
    return (s_frames_until_goal_submode != 0 && mode::is_gameplay_main(mkb::sub_mode)) ||
           mode::is_postgoal(mkb::sub_mode);
}

bool is_gameplay_exact() { return mode::is_gameplay(mkb::sub_mode) && !is_postgoal_exact(); }

void unset_goal_flags() {
    if (!is_postgoal_exact()) {
        if (!mode::is_game_scenario_return(mkb::sub_mode)) {
            s_goal_flag_game_return = false;
        }
        if (!(mode::is_game_scenario_return(mkb::sub_mode) ||
              mode::is_story_exit_game(mkb::sub_mode))) {
            s_goal_flag_exit_game = false;
        }
        if (mode::is_spin_in_init(mkb::sub_mode) || is_gameplay_exact()) {
            s_goal_flag_retry = false;
        }
        if (mode::is_spin_in_init(mkb::sub_mode) || is_gameplay_exact() ||
            mode::is_game_scenario_main(mkb::sub_mode)) {
            s_goal_flag_last_stage = false;
        }
    }
}

void reset_tape_break_counter() {
    // Handle cases where we load state or pause (and either leave the stage or retry) immediately
    // after breaking the tape
    // This gets run after mkb::mode_tick ie after the game uppdates the submode
    // Because of the run order notes mentioned earlier, this always runs after savest_ui's tick
    if (mode::is_stage_exit_init(mkb::sub_mode) || libsavest::state_loaded_this_frame() ||
        mode::is_spin_in_init(mkb::sub_mode)) {
        s_frames_until_goal_submode = 0;
    }
}

bool did_ball_enter_goal_hook(mkb::Ball* ball, int* out_stage_goal_idx, int* out_itemgroup_id,
                              mkb::byte* out_goal_flags) {
    bool result = s_goal_tramp.dest(ball, out_stage_goal_idx, out_itemgroup_id, out_goal_flags);

    if (result) {
        set_goal_flags();
        s_frames_until_goal_submode = TIME_BETWEEN_TAPE_BREAK_AND_GOAL_SUBMODE;
    }
    // We don't need to check if we're paused because this function doesn't
    // get run while paused
    if (s_frames_until_goal_submode != 0) {
        s_frames_until_goal_submode -= 1;
    }
    return result;
}

void mode_tick_hook() {
    s_mode_tick_tramp.dest();
    // Run this after the game updates the submode
    // This ensurs that our flags that should get unset during game scenario main
    // get unset after the submode switch (and so there isn't a frame where they
    // are true when they shouldn't be)
    reset_tape_break_counter();
    unset_goal_flags();
}

void init() {
    patch::hook_function(s_goal_tramp, &mkb::did_ball_enter_goal, did_ball_enter_goal_hook);
    patch::hook_function(s_mode_tick_tramp, mkb::mode_tick, mode_tick_hook);
}

// Notes about loading state right after goal and s_frames_until_goal_submode being 0, not 1

// --- story mode status functions based on goal checks (is_between_worlds(), is_run_complete()) ---

// Important note: mode::get_clear_count_for_world() is based off
// mkb::get_world_unbeaten_stage_count(), which only increments after game scenario return ends
// This is why it's very important to be careful about the behavior of our flag on the game scenario
// return frame (this ensures our timer/deathcounter won't flash/disappear for a frame when the
// "Between Worlds" pref is enabled)
// Due to the run order notes earlier, when the submode switches to game scenario main, first our
// flag gets updated, then the clear count increments, and then disp() functions run

bool is_between_worlds_main(bool goal_flag) {
    u16 world_clear_count = mode::get_clear_count_for_world();
    if ((world_clear_count == STAGES_PER_WORLD - 1) && goal_flag) {
        return true;
    } else if (world_clear_count == STAGES_PER_WORLD) {
        return true;
    }
    return false;
}

// The reason we use different flags depending on if we're on the last world or not is for the
// following behavior:
// (1) If we exit game after completing the last stage and then return to the menu, we want our run
// to still be flagged as complete (so that we know we should reset the run instead of running the
// timer, due to how we handle resetting/not resetting the run if we fully exit game)
// (2) We can't use s_goal_flag_retry for every world, however, because of the possibility of first
// framing the last stage of a world and returning to the 10 ball screen

bool is_between_worlds() {
    u8 curr_world = mkb::scen_info.world;
    if (curr_world == WORLD_COUNT - 1) {
        return is_between_worlds_main(s_goal_flag_last_stage);
    } else {
        return is_between_worlds_main(s_goal_flag_exit_game);
    }
}

bool is_run_complete() { return mkb::scen_info.world == WORLD_COUNT - 1 && is_between_worlds(); }

}  // namespace goal