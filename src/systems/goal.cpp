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

constexpr u16 WORLD_COUNT = mode::WORLD_COUNT;
constexpr u16 STAGES_PER_WORLD = mode::STAGES_PER_WORLD;

constexpr u8 TIME_BETWEEN_TAPE_BREAK_AND_GOAL_SUBMODE = 3;
static u8 s_frames_until_goal_submode = 0;

// true during postgoal (starting at tape break), and true during game scenario return *if* we enter
// it after successfully entering a goal
static bool s_is_postgoal_extended = false;  // should we rename this to s_has_entered_goal?
static bool s_has_entered_goal = false;

// Various goal flags that get set to true on goal tape break (and stay true during postgoal), but
// get unset at different times
static bool s_goal_flag_game_return;  // Unset after game scenario return
static bool s_goal_flag_exit_game;    // Unset outside of exit game or game scenario return
static bool s_goal_flag_retry;        // Unset on spin in (or gameplay pre tape break)

static patch::Tramp<decltype(&mkb::did_ball_enter_goal)> s_goal_tramp;
static patch::Tramp<decltype(&mkb::mode_tick)> s_mode_tick_tramp;

u8 get_frames_until_goal_submode() { return s_frames_until_goal_submode; }

void set_goal_flags() {
    s_goal_flag_game_return = true;
    s_goal_flag_exit_game = true;
    s_goal_flag_retry = true;
}

// Remedy for goal submode checks being delayed from tape break
bool is_postgoal_exact() {
    return (s_frames_until_goal_submode != 0 && mode::is_gameplay_main(mkb::sub_mode)) ||
           mode::is_postgoal(mkb::sub_mode);
}

// Include the 1 game scenario return frame with the above function
bool is_postgoal_exact_with_game_return() {
    return is_postgoal_exact() || mode::is_game_scenario_return(mkb::sub_mode);
}

bool is_gameplay_exact() { return mode::is_gameplay(mkb::sub_mode) && !is_postgoal_exact(); }

// Includes the game scenario return frame *if* we entered the goal before entering it
// For example, if we stage select mid gameplay, this will return false on the game
// scenario return frame
// This distinction has utility for our functions dealing with story mode clear counts
// (eg is_between_worlds() and is_run_complete())
bool is_postgoal_extended() { return is_postgoal_exact() || s_is_postgoal_extended; }

// Includes postgoal starting from tape break, includes the game scenario return frame if
// we enter it from a successful run, and includes story exit game submodes if we enter them
// from a successful run
bool has_entered_goal() { return s_has_entered_goal; }

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
    }
}

void reset_tape_break_counter() {
    if (mode::is_stage_exit_init(mkb::sub_mode) || libsavest::state_loaded_this_frame() ||
        mode::is_spin_in_init(mkb::sub_mode)) {
        s_frames_until_goal_submode = 0;
    }
}

bool did_ball_enter_goal_hook(mkb::Ball* ball, int* out_stage_goal_idx, int* out_itemgroup_id,
                              mkb::byte* out_goal_flags) {
    bool result = s_goal_tramp.dest(ball, out_stage_goal_idx, out_itemgroup_id, out_goal_flags);

    if (result) {
        s_is_postgoal_extended = result;
        s_has_entered_goal = result;
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
    // We need to do this here and not in our usual tick function (which hooks into
    // mkb::process_inputs, which I believe runs before mkb::mode_tick), otherwise
    // s_is_postgoal_extended will remain true during the first frame of the
    // game scenario main submode
    if (!is_postgoal_exact_with_game_return()) {
        s_is_postgoal_extended = false;
    }
    if (!(is_postgoal_exact_with_game_return() || mode::is_story_exit_game(mkb::sub_mode))) {
        s_has_entered_goal = false;
    }

    unset_goal_flags();
    // reset_tape_break_counter();
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
        // I think I could move this to the mode tick hook
    }
}

// We can use is_postgoal_extended() to get the following useful status functions during a story
// mode run (these are used in storytimer.cpp and deathcounter.cpp)
// It's important that we pass in is_postgoal_extended() so we get the correct behavior on the game
// scenario return frame (because mkb::get_world_unbeaten_stage_count() only increments after
// this submode)

// bool is_between_worlds() { return mode::is_between_worlds(has_entered_goal()); }
// bool is_run_complete() { return mode::is_run_complete(has_entered_goal()); }

// TODO: exit game after tape break on last stage should still count as run being complete
// Use different flag for is_run_complete(), one that only gets reset by spin in eg

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
// If we exit game after completing the last stage and then return to the menu, we want our run
// to still be flagged as complete (so that we know we should reset the run instead of running the
// timer, due to how we handle resetting/not resetting the run if we fully exit game)

bool is_between_worlds() {
    u8 curr_world = mkb::scen_info.world;
    if (curr_world == WORLD_COUNT - 1) {
        return is_between_worlds_main(s_goal_flag_retry);
    } else {
        return is_between_worlds_main(s_goal_flag_exit_game);
    }
}

bool is_run_complete() { return mkb::scen_info.world == WORLD_COUNT - 1 && is_between_worlds(); }

}  // namespace goal