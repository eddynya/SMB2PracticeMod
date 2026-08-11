#include "goal.h"

#include "../mkb/mkb.h"
#include "../utils/libsavest.h"
#include "../utils/mode.h"
#include "../utils/patch.h"

namespace goal {

// Utility file that provides some functions that deal with and are based off of giving precise goal
// checks (ie the moment we break the tape and not delayed like the goal submodes are)
// This grew out of validate.cpp and was separated into its own file to avoid conflicting interests
// Namely, this file should be very low in the include chain to allow as many other files to use it

constexpr u8 TIME_BETWEEN_TAPE_BREAK_AND_GOAL_SUBMODE = 3;
static u8 s_frames_until_goal_submode = 0;

// true during postgoal (starting at tape break), and true during game scenario return *if* we enter
// it after successfully entering a goal
static bool s_is_postgoal_extended = false;

static patch::Tramp<decltype(&mkb::did_ball_enter_goal)> s_goal_tramp;
static patch::Tramp<decltype(&mkb::mode_tick)> s_mode_tick_tramp;

u8 get_frames_until_goal_submode() { return s_frames_until_goal_submode; }

// Remedy for goal submode checks being delayed from tape break
bool is_postgoal_exact() {
    return (s_frames_until_goal_submode != 0 && mode::is_gameplay_main(mkb::sub_mode)) ||
           mode::is_postgoal(mkb::sub_mode);
}

// Include the 1 game scenario return frame with the above function
bool is_postgoal_exact_with_game_return() {
    return is_postgoal_exact() || mode::is_game_scenario_return(mkb::sub_mode);
}

// Includes the game scenario return frame *if* we entered the goal before entering it
// For example, if we stage select mid gameplay, this will return false on the game
// scenario return frame
// This distinction has utility for our functions dealing with story mode clear counts
// (eg is_between_worlds() and is_run_complete())
bool is_postgoal_extended() { return is_postgoal_exact() || s_is_postgoal_extended; }

bool is_gameplay_exact() { return mode::is_gameplay(mkb::sub_mode) && !is_postgoal_exact(); }

bool did_ball_enter_goal_hook(mkb::Ball* ball, int* out_stage_goal_idx, int* out_itemgroup_id,
                              mkb::byte* out_goal_flags) {
    bool result = s_goal_tramp.dest(ball, out_stage_goal_idx, out_itemgroup_id, out_goal_flags);

    if (result) {
        s_is_postgoal_extended = result;
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
    // We need to do this here and not in our usual tick function (which hooks into
    // mkb::process_inputs, which I believe runs before mkb::mode_tick), otherwise
    // s_is_postgoal_extended will remain true during the first frame of the
    // game scenario main submode
    if (!is_postgoal_exact_with_game_return()) {
        s_is_postgoal_extended = false;
    }
}

void init() {
    patch::hook_function(s_goal_tramp, &mkb::did_ball_enter_goal, did_ball_enter_goal_hook);
    patch::hook_function(s_mode_tick_tramp, mkb::mode_tick, mode_tick_hook);
}

void tick() {
    // Handle cases where we pause (and either leave the stage or retry) or load state immediately
    // after breaking the tape
    // Note: savest_ui's tick gets run before goal's tick, so if we load state right after
    // breaking the tape, there won't be a frame where we're in gameplay and
    // s_frames_until_goal_submode is nonzero
    if (mode::is_stage_exit_init(mkb::sub_mode) || libsavest::state_loaded_this_frame() ||
        mode::is_spin_in_init(mkb::sub_mode)) {
        s_frames_until_goal_submode = 0;
    }
}

// We can use is_postgoal_extended() to get the following useful status functions during a story
// mode run (these are used in storytimer.cpp and deathcounter.cpp)
// It's important that we pass in is_postgoal_extended() so we get the correct behavior on the game
// scenario return frame (because mkb::get_world_unbeaten_stage_count() only increments after
// this submode)

bool is_between_worlds() { return mode::is_between_worlds(is_postgoal_extended()); }
bool is_run_complete() { return mode::is_run_complete(is_postgoal_extended()); }

}  // namespace goal