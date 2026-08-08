#include "storytimer.h"

#include "../mkb/mkb.h"

#include "../utils/mode.h"
#include "../utils/timerdisp.h"
#include "mods/freecam.h"
#include "mods/validate.h"
#include "systems/assembly.h"
#include "systems/pad.h"
#include "systems/pref.h"
#include "utils/draw.h"
#include "utils/patch.h"
#include "validate.h"

namespace storytimer {

enum class TimerOptions {
    DontShow = 0,
    AlwaysShow = 1,
    BetweenWorlds = 2,
    EndOfRun = 3,
};

enum class TimerType {
    Fullgame,
    Segment,
};

static constexpr s32 FULLGAME_TIMER_LOCATION_X = 18 + 24;
static constexpr s32 FULLGAME_TIMER_TEXT_OFFSET = 56;
static constexpr s32 SEGMENT_TIMER_LOCATION_X = 30 + 24;
static constexpr s32 SEGMENT_TIMER_TEXT_OFFSET = 44;
static constexpr s32 IW_TIME_LOCATION_X = 42 + 24;
static constexpr s32 WORLD_COUNT = 10;
static constexpr s32 STAGES_PER_WORLD = 10;
static constexpr u32 SECOND_FRAMES = 60;
static constexpr u32 MINUTE_FRAMES = SECOND_FRAMES * 60;
static constexpr u32 HOUR_FRAMES = MINUTE_FRAMES * 60;

struct TimerGroup {
    u32 segment;     // the time taken to complete a world up until tape break on the last stage
    u32 full_world;  // the time taken to complete a world until the fade to white on the last stage
};
static TimerGroup s_timer_group[WORLD_COUNT];  // each world has its own TimerGroup structure
static s32 s_completed_stages;                 // the completed stages for the whole run

u32 get_completed_stagecount() { return s_completed_stages; }

// before starting the run, there are several values we zero
void reset_timer() {
    // s_completed_stages = 0;
    for (s32 k = 0; k < WORLD_COUNT; k++) {
        s_timer_group[k] = {};
    }
}

// Note: mkb::get_world_unbeaten_stage_count() only increments after the game scenario return
// submode finishes
u16 get_total_cleared_stages() {
    u16 sum = 0;
    for (u16 k = 0; k < WORLD_COUNT; k++) {
        sum += mkb::get_world_unbeaten_stage_count(k);
    }
    return sum;
}

/*
u16 get_clear_count_for_world() {
    u16 total_clears = get_total_cleared_stages();
    return total_clears % STAGES_PER_WORLD;
} */

u16 get_clear_count_for_world() {
    return mkb::get_world_unbeaten_stage_count(mkb::scen_info.world);
}

// Importantly, we don't increment the timer on the 10 ball screen after selecting a stage
// We also need to be careful about accessing mkb::g_storymode_stageselect_state when main_game.rel
// is not loaded, so we guard it with a sub_mode + scen_info check
bool is_pre_selection_on_10_ball_screen() {
    if (mode::is_on_10_ball_screen(mkb::sub_mode, mkb::scen_info)) {
        return mode::is_on_10_ball_spin_in(mkb::g_storymode_stageselect_state) ||
               mode::is_idle_on_10_ball_screen(mkb::g_storymode_stageselect_state);
    }
    return false;
}

// On the last stage of a world, we stop the segment timer early (ie on tape break)
bool should_run_segment_timer_on_last_stage_of_world() {
    return (mode::is_on_stage(mkb::sub_mode) || mode::is_story_exit_game(mkb::sub_mode)) &&
           !validate::has_entered_goal();
}

// Places where the segment timer for a world should run
bool should_segment_timer_run(int world_idx) {
    if (mkb::get_world_unbeaten_stage_count(world_idx) < 9) {
        return true;
    } else if (mkb::get_world_unbeaten_stage_count(world_idx) == 9) {
        return is_pre_selection_on_10_ball_screen() ||
               should_run_segment_timer_on_last_stage_of_world();
    } else {
        return false;
    }
}

// TODO: game first init
void update_timers() {
    for (u16 k = 0; k < WORLD_COUNT; k++) {
        if (mkb::scen_info.world == k) {  // World (k+1)'s timer
            if (mode::is_on_stage(mkb::sub_mode) || mode::is_story_exit_game(mkb::sub_mode) ||
                is_pre_selection_on_10_ball_screen()) {  // include extra game scenario return?
                s_timer_group[k].full_world += 1;
            }
            if (get_total_cleared_stages() % STAGES_PER_WORLD != 9 &&
                mode::is_game_scenario_return(mkb::sub_mode)) {
                // is mkb::get_world_unbeaten_stage_count(k) != 9 easier to read?
                s_timer_group[k].full_world += 1;  // +=2 to make timer look nicer?
            }
            if (should_segment_timer_run(k)) {
                s_timer_group[k].segment = s_timer_group[k].full_world;
            }
        }
    }
}

void tick() {
    // reset the timer on the file select screen and the name entry screen
    if (mode::is_storymode_file_screen(mkb::scen_info) ||
        mode::is_storymode_name_entry_screen(mkb::scen_info)) {
        reset_timer();
    }

    update_timers();
    /*
        u32 sum = 0;
        for (s32 k = 0; k < WORLD_COUNT; k++) {
            sum += mkb::get_world_unbeaten_stage_count(k);
        }
        s_completed_stages = sum;

        // for later, it's useful to record what submodes correspond to spin in, gameplay, etc.

        // submodes during spin in
        bool is_on_spin_in =
            (mkb::sub_mode == mkb::SMD_GAME_FIRST_INIT || mkb::sub_mode == mkb::SMD_GAME_READY_INIT
       || mkb::sub_mode == mkb::SMD_GAME_READY_MAIN);

        // submodes during gameplay
        bool is_on_gameplay =
            (mkb::sub_mode == mkb::SMD_GAME_PLAY_INIT || mkb::sub_mode == mkb::SMD_GAME_PLAY_MAIN);

        // submodes entered when exiting game
        bool is_on_exit_game = (mkb::sub_mode == mkb::SMD_GAME_INTR_SEL_INIT ||
                                mkb::sub_mode == mkb::SMD_GAME_INTR_SEL_MAIN ||
                                mkb::sub_mode == mkb::SMD_GAME_SUGG_SAVE_INIT ||
                                mkb::sub_mode == mkb::SMD_GAME_SUGG_SAVE_MAIN);

        // submodes entered when breaking the goal tape
        bool is_postgoal =
            (mkb::sub_mode == mkb::SMD_GAME_GOAL_INIT || mkb::sub_mode == mkb::SMD_GAME_GOAL_MAIN ||
             mkb::sub_mode == mkb::SMD_GAME_GOAL_REPLAY_INIT ||
             mkb::sub_mode == mkb::SMD_GAME_GOAL_REPLAY_MAIN ||
             mkb::sub_mode == mkb::SMD_GAME_SCENARIO_RETURN);

        // submodes for the fallout and y/n screen
        bool is_on_fallout =
            (mkb::sub_mode == mkb::SMD_GAME_RINGOUT_INIT ||
             mkb::sub_mode == mkb::SMD_GAME_RINGOUT_MAIN || mkb::sub_mode ==
       mkb::SMD_GAME_RETRY_INIT || mkb::sub_mode == mkb::SMD_GAME_RETRY_MAIN);

        // submodes during timeover
        bool is_timeover = (mkb::sub_mode == mkb::SMD_GAME_TIMEOVER_INIT ||
                            mkb::sub_mode == mkb::SMD_GAME_TIMEOVER_MAIN);

        // stage select states on the 10 ball screen *not* including the transition after the a
       press
        // input. this transition is not included in the timer because even ignoring completely
       white
        // frames, the time spent on mkb::g_storymode_stageselect_state == mkb::STAGE_SELECTED can
       be
        // highly variable (up to over 40 frames sometimes!), so for the purpose of a loadless
        // timer, it makes sense to cut this out
        bool is_pre_load_in_stage_select =
            (mkb::g_storymode_stageselect_state == mkb::STAGE_SELECT_INTRO_SEQUENCE ||
             mkb::g_storymode_stageselect_state == 3 ||
             mkb::g_storymode_stageselect_state == mkb::STAGE_SELECT_IDLE);
        */

    /*
        for (s32 k = 0; k < WORLD_COUNT; k++) {
            if (mkb::scen_info.world == k) {
                if (is_on_spin_in || is_on_exit_game || is_on_fallout || is_timeover ||
                    is_on_gameplay || is_postgoal || is_pre_load_in_stage_select) {
                    // increment the timer during every frame on spin in, exit game, fallout,
                    // timeover, gameplay, and every frame after breaking the tape until (but not
                    // including) the load back to the 10 ball screen. In addition, increment the
       timer
                    // on every frame on the 10 ball screen until the a press input
                    s_timer_group[k].full_world += 1;
                }
                if (s_completed_stages % STAGES_PER_WORLD != 9 &&
                    mkb::sub_mode == mkb::SMD_GAME_SCENARIO_RETURN) {
                    // need to add 1 frame to the timer when stage selecting to the 10 ball screen
                    // since the first non-completely white frame is not included in
                    // mkb::STAGE_SELECT_INTRO_SEQUENCE; don't include the last stage of a world
                    // since that missing frame is handled by the world start correction
                    s_timer_group[k].full_world += 1;
                }
                if (mkb::get_world_unbeaten_stage_count(k) < 9 ||
                    (mkb::get_world_unbeaten_stage_count(k) == 9 &&
                     (is_pre_load_in_stage_select || is_on_spin_in ||
       !validate::has_entered_goal()))) {
                    // stop incrementing the segment timer once the tape is broken on the last stage
       of
                    // the world
                    s_timer_group[k].segment = s_timer_group[k].full_world;
                }
            }
        } */
}

// cutscene submodes
// 249, 247, 248, 94
/*
 if ((clear_count % STAGES_PER_WORLD == 9) && mode::is_postgoal(mkb::sub_mode)) {
        return true;
    } else if ((clear_count % STAGES_PER_WORLD == 0 &&
                mode::is_on_10_ball_screen(mkb::sub_mode, mkb::scen_info))) {
        // We want this function to return true during the cutscene between worlds, so only return
        // false when we properly enter the next world's 10 ball screen
        return false;
    } else if (clear_count % STAGES_PER_WORLD != 0) {
        return false;
    }
    return false;
*/

// (i will get rid of the magic numbers)

bool is_between_worlds() {
    u16 clear_count = get_total_cleared_stages();
    if ((clear_count % STAGES_PER_WORLD == 9) && mode::is_postgoal(mkb::sub_mode)) {
        return true;
    } else if (mode::is_story_cutscene(mkb::sub_mode)) {  // don't include w1 cutscene?
        return true;
    }
    return false;
}

bool is_run_complete() {
    if (mkb::scen_info.world == 9) {
        return (mkb::get_world_unbeaten_stage_count(9) == 9 && mode::is_postgoal(mkb::sub_mode)) ||
               mkb::get_world_unbeaten_stage_count(9) == 10;
    }
    return false;
}

bool should_display_timer(TimerType type) {
    u8 pref;
    if (type == TimerType::Fullgame) {
        pref = pref::get(pref::U8Pref::FullgameTimerOptions);
    } else {  // Segment timer
        pref = pref::get(pref::U8Pref::SegmentTimerOptions);
    }

    // TODO: different behavior for segment timer bc of breakdown screen
    switch (TimerOptions(pref)) {
        case TimerOptions::AlwaysShow:
            return true;
        case TimerOptions::BetweenWorlds:
            return is_between_worlds();
        case TimerOptions::EndOfRun:
            return is_run_complete();
        case TimerOptions::DontShow:
            return false;
    }
}

u32 get_split_timer_for_world(int world_idx) {
    // TODO: world start correction?
    u32 prev_world_sum = 0;
    for (u16 k = 0; k < world_idx; k++) {
        prev_world_sum += s_timer_group[k].full_world;
    }
    return prev_world_sum + s_timer_group[world_idx].segment;
}

void disp() {
    if ((mkb::main_game_mode != mkb::STORY_MODE && mkb::sub_mode != mkb::SMD_AUTHOR_PLAY_INIT &&
         mkb::sub_mode != mkb::SMD_AUTHOR_PLAY_MAIN) ||
        freecam::should_hide_hud()) {
        return;
    }

    timerdisp::draw_timer(SEGMENT_TIMER_LOCATION_X, 2, SEGMENT_TIMER_TEXT_OFFSET,
                          "Dbg:", 60 * mkb::get_world_unbeaten_stage_count(0), true, draw::WHITE);
    timerdisp::draw_timer(SEGMENT_TIMER_LOCATION_X, 3, SEGMENT_TIMER_TEXT_OFFSET,
                          "Dbg:", 60 * mkb::sub_mode, true, draw::WHITE);

    bool is_postgoal =
        (mkb::sub_mode == mkb::SMD_GAME_GOAL_INIT || mkb::sub_mode == mkb::SMD_GAME_GOAL_MAIN ||
         mkb::sub_mode == mkb::SMD_GAME_GOAL_REPLAY_INIT ||
         mkb::sub_mode == mkb::SMD_GAME_GOAL_REPLAY_MAIN ||
         mkb::sub_mode == mkb::SMD_GAME_SCENARIO_RETURN);

    bool is_between_worlds = false;
    if ((s_completed_stages % STAGES_PER_WORLD == 9) && is_postgoal) {
        is_between_worlds = true;
    } else if ((s_completed_stages % STAGES_PER_WORLD == 0 &&
                mkb::g_storymode_stageselect_state == mkb::STAGE_SELECT_INTRO_SEQUENCE) ||
               s_completed_stages % STAGES_PER_WORLD != 0) {
        // no longer "between worlds" if you enter the next world's 10 ball screen, or if you break
        // the tape on the last stage of the current world, but retry
        is_between_worlds = false;
    }

    bool is_run_complete = (mkb::scen_info.world == 9 &&
                            ((mkb::get_world_unbeaten_stage_count(9) == 9 && is_postgoal) ||
                             mkb::get_world_unbeaten_stage_count(9) == 10));

    // move the position of the fullgame timer if the death counter is on
    u32 fullgame_timer_location_y = 2;
    if (pref::get(pref::BoolPref::ShowDeathCounter)) {
        fullgame_timer_location_y++;
    }

    bool display_story_timer = false;
    switch (TimerOptions(pref::get(pref::U8Pref::FullgameTimerOptions))) {
        case TimerOptions::AlwaysShow:
            display_story_timer = true;
            break;
        case TimerOptions::BetweenWorlds:
            display_story_timer = is_between_worlds;
            break;
        case TimerOptions::EndOfRun:
            display_story_timer = is_run_complete;
            break;
        case TimerOptions::DontShow:
            display_story_timer = false;
            break;
    }

    // at the start of each world, we need to correct the timer by 1 frame
    u32 full_world[10] = {};
    u32 segment[10] = {};

    for (s32 k = 0; k < WORLD_COUNT; k++) {
        if (s_timer_group[k].full_world > 0) {
            // don't apply the correction until the world actually starts
            full_world[k] = s_timer_group[k].full_world + 1;
            segment[k] = s_timer_group[k].segment + 1;
        }
    }

    // split[k] is just the fullgame time at tape break on the last stage of world k;
    // to calculate this, just add full_world[j] (j<k) for the previous worlds to the
    // current world's segment timer
    u32 sum[WORLD_COUNT] = {};
    u32 split[WORLD_COUNT] = {};
    for (s32 k = 1; k < WORLD_COUNT; k++) {
        for (s32 j = 0; j < k; j++) {
            sum[k] += full_world[j];
        }
        split[k] = sum[k] + segment[k];
    }
    split[0] = segment[0];
    u32 loadless_story_timer = split[9];
    /*
        if (display_story_timer) {
            timerdisp::draw_timer(FULLGAME_TIMER_LOCATION_X, fullgame_timer_location_y,
                                  FULLGAME_TIMER_TEXT_OFFSET, "Time:", loadless_story_timer, false,
                                  draw::WHITE);
        }
     */
    // move the position of the segment timer depending on if the death counter and fullgame timer
    // is on
    u32 segment_timer_location_y = 2;
    if (display_story_timer) {
        segment_timer_location_y++;
    }
    if (pref::get(pref::BoolPref::ShowDeathCounter)) {
        segment_timer_location_y++;
    }
    /*
        switch (TimerOptions(pref::get(pref::U8Pref::SegmentTimerOptions))) {
            case TimerOptions::AlwaysShow:
                for (s32 k = 0; k < WORLD_COUNT; k++) {
                    if (mkb::scen_info.world == k && !is_run_complete) {
                        timerdisp::draw_timer(SEGMENT_TIMER_LOCATION_X, segment_timer_location_y,
                                              SEGMENT_TIMER_TEXT_OFFSET, "Seg:", segment[k], false,
                                              draw::WHITE);
                    }
                }
                break;
            case TimerOptions::BetweenWorlds:
                for (s32 k = 0; k < WORLD_COUNT; k++) {
                    if (is_between_worlds && mkb::scen_info.world == k && k != 9) {
                        timerdisp::draw_timer(SEGMENT_TIMER_LOCATION_X, segment_timer_location_y,
                                              SEGMENT_TIMER_TEXT_OFFSET, "Seg:", segment[k], false,
                                              draw::WHITE);
                    }
                }
                break;
            case TimerOptions::EndOfRun:
                break;
            case TimerOptions::DontShow:
                break;
        } */

    u32 split_hours[10] = {};
    u32 split_minutes[10] = {};
    u32 split_seconds[10] = {};
    u32 split_centiseconds[10] = {};

    u32 segment_hours[10] = {};
    u32 segment_minutes[10] = {};
    u32 segment_seconds[10] = {};
    u32 segment_centiseconds[10] = {};

    for (s32 k = 0; k < WORLD_COUNT; k++) {
        split_hours[k] = split[k] / HOUR_FRAMES;
        split_minutes[k] = split[k] % HOUR_FRAMES / MINUTE_FRAMES;
        split_seconds[k] = split[k] % MINUTE_FRAMES / SECOND_FRAMES;
        split_centiseconds[k] = (split[k] % SECOND_FRAMES) * 100 / 60;

        segment_hours[k] = segment[k] / HOUR_FRAMES;
        segment_minutes[k] = segment[k] % HOUR_FRAMES / MINUTE_FRAMES;
        segment_seconds[k] = segment[k] % MINUTE_FRAMES / SECOND_FRAMES;
        segment_centiseconds[k] = (segment[k] % SECOND_FRAMES) * 100 / 60;
    }

    char timer_str[10][32] = {};  // timer_str[k-1] is the formatted text for "Wk: split[k-1]
                                  // (segment[k-1])"

    // if the segment timer is enabled in any capacity, after the tape is broken on the last stage,
    // show all 10 split and iw times in the format "Wk: split[k] (segment[k])"
    // to do: it is very likely that the current timer formatting text below is redundant given that
    // there is similar code in timerdisp.cpp; for later: implement complex's suggestion for this
    /* if (TimerOptions(pref::get(pref::U8Pref::SegmentTimerOptions)) != TimerOptions::DontShow &&
        is_run_complete) {
        for (s32 k = 0; k < WORLD_COUNT; k++) {
            u32 X[10] = {};  // horizontal position for the line of text "Wk: split[k] (segment[k])"
            if (k != 9) {
                X[k] = IW_TIME_LOCATION_X;
            } else {
                X[k] =
                    SEGMENT_TIMER_LOCATION_X;  // change the position for world 10 because W10 takes
                                               // up 3 characters instead of 2 like the rest
            }
            u32 Y[k] = {};  // vertical position for the line of text "Wk: split[k] (segment[k])"
            Y[k] = 24 + (segment_timer_location_y + k) * 16;
            if (split_hours[k] > 0) {
                if (segment_hours[k] > 0) {
                    mkb::sprintf(timer_str[k], "W%d:%d:%02d:%02d.%02d (%d:%02d:%02d.%02d)", k + 1,
                                 split_hours[k], split_minutes[k], split_seconds[k],
                                 split_centiseconds[k], segment_hours[k], segment_minutes[k],
                                 segment_seconds[k], segment_centiseconds[k]);
                    draw::debug_text(X[k], Y[k], draw::WHITE, "%s", timer_str[k]);
                } else {
                    mkb::sprintf(timer_str[k], "W%d:%d:%02d:%02d.%02d (%02d:%02d.%02d)", k + 1,
                                 split_hours[k], split_minutes[k], split_seconds[k],
                                 split_centiseconds[k], segment_minutes[k], segment_seconds[k],
                                 segment_centiseconds[k]);
                    draw::debug_text(X[k], Y[k], draw::WHITE, "%s", timer_str[k]);
                }
            } else {
                mkb::sprintf(timer_str[k], "W%d:%02d:%02d.%02d (%02d:%02d.%02d)", k + 1,
                             split_minutes[k], split_seconds[k], split_centiseconds[k],
                             segment_minutes[k], segment_seconds[k], segment_centiseconds[k]);
                draw::debug_text(X[k], Y[k], draw::WHITE, "%s", timer_str[k]);
            }
        }
    } */
}

}  // namespace storytimer