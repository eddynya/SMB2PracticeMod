#include "storytimer.h"

#include "../mkb/mkb.h"

#include "../mods/freecam.h"
#include "../mods/validate.h"
#include "../systems/assembly.h"
#include "../systems/pad.h"
#include "../systems/pref.h"
#include "../utils/draw.h"
#include "../utils/mode.h"
#include "../utils/patch.h"
#include "../utils/timerdisp.h"
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

struct TimerGroup {
    u32 segment;     // the time taken to complete a world up until tape break on the last stage
    u32 full_world;  // the time taken to complete a world until the fade to white on the last stage
};

constexpr u16 FULLGAME_TIMER_LOCATION_X = 18 + 24;
constexpr u16 FULLGAME_TIMER_TEXT_OFFSET = 56;
constexpr u16 SEGMENT_TIMER_LOCATION_X = 30 + 24;
constexpr u16 SEGMENT_TIMER_TEXT_OFFSET = 44;
constexpr u16 IW_TIME_LOCATION_X = 42 + 24;  // why is it named this bro what was i doing
constexpr u16 STARTING_ROW = 2;

static constexpr s32 WORLD_COUNT = 10;
static constexpr s32 STAGES_PER_WORLD = 10;

static TimerGroup s_timer_group[WORLD_COUNT];  // each world has its own TimerGroup structure

// static s32 s_completed_stages;  // the completed stages for the whole run

// u32 get_completed_stagecount() { return s_completed_stages; }

void reset_timer() {
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

// Note that after completing a world, this function
// returns 10 until the 10 ball screen of the next world (at which
// point it reverts to 0)
u16 get_clear_count_for_world() {
    return mkb::get_world_unbeaten_stage_count(mkb::scen_info.world);
}

// Importantly, we don't increment the timer on the 10 ball screen after selecting a stage
// We also need to be careful about accessing mkb::g_storymode_stageselect_state when main_game.rel
// is not loaded, so we guard it with a sub_mode + scen_info check
bool is_pre_selection_on_10_ball_screen() {
    if (mode::is_on_10_ball_screen(mkb::sub_mode, mkb::scen_info)) {
        // the 10 ball spin in check doesn't catch the first frame of a world
        return mode::is_on_10_ball_spin_in(mkb::g_storymode_stageselect_state) ||
               mode::is_idle_on_10_ball_screen(mkb::g_storymode_stageselect_state) ||
               mode::is_first_frame_of_world(mkb::scen_info);
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
    if (mkb::get_world_unbeaten_stage_count(world_idx) < STAGES_PER_WORLD - 1) {
        return true;
    } else if (mkb::get_world_unbeaten_stage_count(world_idx) == STAGES_PER_WORLD - 1) {
        return is_pre_selection_on_10_ball_screen() ||
               should_run_segment_timer_on_last_stage_of_world();
    } else {
        return false;
    }
}

// TODO: game first init
/*
mode::is_on_stage_with_endpoints(mkb::sub_mode) ||
                mode::is_story_exit_game(mkb::sub_mode) ||
*/
// load outs currently 2f (not 1f) behind old loadless timer?
void update_timers() {
    for (u16 k = 0; k < WORLD_COUNT; k++) {
        if (mkb::scen_info.world == k) {  // World (k+1)'s timer
            if (mode::is_on_stage_with_endpoints(mkb::sub_mode) ||
                mode::is_story_exit_game(mkb::sub_mode) ||
                is_pre_selection_on_10_ball_screen()) {  // include extra game scenario return?
                s_timer_group[k].full_world += 1;
            }

            if (should_segment_timer_run(k)) {
                s_timer_group[k].segment = s_timer_group[k].full_world;
            }
        }
    }
}

/* if (get_total_cleared_stages() % STAGES_PER_WORLD != 9 &&
                mode::is_game_scenario_return(mkb::sub_mode)) {
                // is mkb::get_world_unbeaten_stage_count(k) != 9 easier to read?
                s_timer_group[k].full_world += 1;  // +=2 to make timer look nicer?
            } */

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

// (world_clear_count == 9) && mode::is_postgoal(mkb::sub_mode)

// maybe move the next 2 functions to mode.cpp so deathcounter can also use them?
bool is_between_worlds() {
    // u16 clear_count = get_total_cleared_stages();
    u16 world_clear_count = get_clear_count_for_world();
    if ((world_clear_count == STAGES_PER_WORLD - 1) && validate::has_entered_goal()) {
        return true;
    } else if (world_clear_count == STAGES_PER_WORLD) {
        // mode::is_story_cutscene(mkb::sub_mode)
        return true;
    }
    return false;
}

// && mode::is_postgoal(mkb::sub_mode))
bool is_run_complete() {
    if (mkb::scen_info.world == WORLD_COUNT - 1) {
        return (get_clear_count_for_world() == STAGES_PER_WORLD - 1 &&
                validate::has_entered_goal()) ||
               get_clear_count_for_world() == STAGES_PER_WORLD;
    }
    return false;
}

// The run breakdown screen replaces the segment timer at the end of the run
// if the pref for it is on
bool should_display_timer(TimerType type) {
    u8 pref;
    if (type == TimerType::Fullgame) {
        pref = pref::get(pref::U8Pref::FullgameTimerOptions);
    } else {  // Segment timer
        pref = pref::get(pref::U8Pref::SegmentTimerOptions);
    }

    switch (TimerOptions(pref)) {
        case TimerOptions::AlwaysShow:
            if (type == TimerType::Fullgame) {
                return true;
            } else {
                if (pref::get(pref::BoolPref::ShowRunBreakdown)) {
                    return !is_run_complete();
                } else {
                    return true;
                }
            }
        case TimerOptions::BetweenWorlds:
            // return is_between_worlds();
            if (type == TimerType::Fullgame) {
                return is_between_worlds();
            } else {
                if (pref::get(pref::BoolPref::ShowRunBreakdown)) {
                    return is_between_worlds() && !is_run_complete();
                } else {
                    return is_between_worlds();
                }
            }
        case TimerOptions::EndOfRun:
            // return is_run_complete();
            if (type == TimerType::Fullgame) {
                return is_run_complete();
            } else {
                if (pref::get(pref::BoolPref::ShowRunBreakdown)) {
                    return false;
                } else {
                    return is_run_complete();
                }
            }
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

// Doing this for now until a better display setup is figured out
u16 get_timer_y_pos(TimerType type) {  // maybe rename to get_timer_row()?
    u16 y_pos = STARTING_ROW;
    bool show_death_counter = pref::get(pref::BoolPref::ShowDeathCounter);

    if (type == TimerType::Fullgame) {
        if (show_death_counter) {
            y_pos++;
        }
    } else {  // Segment timer
        if (should_display_timer(TimerType::Fullgame)) {
            y_pos++;
        }
        if (show_death_counter) {
            y_pos++;
        }
    }

    return y_pos;
}

// Bundle up the info timerdisp::draw_timer uses into a struct for convenience
struct TimerDisplayInfo {
    u16 pos_x;
    u16 pos_y;
    u16 text_offset;
};

TimerDisplayInfo get_timer_display_info(TimerType type) {
    if (type == TimerType::Fullgame) {
        return {FULLGAME_TIMER_LOCATION_X, get_timer_y_pos(type), FULLGAME_TIMER_TEXT_OFFSET};
    } else {
        return {SEGMENT_TIMER_LOCATION_X, get_timer_y_pos(type), SEGMENT_TIMER_TEXT_OFFSET};
    }
}

void draw_timers() {
    TimerDisplayInfo fullgame_info = get_timer_display_info(TimerType::Fullgame);
    u32 loadless_time = get_split_timer_for_world(WORLD_COUNT - 1);

    if (should_display_timer(TimerType::Fullgame)) {
        timerdisp::draw_timer(fullgame_info.pos_x, fullgame_info.pos_y, fullgame_info.text_offset,
                              "Time:", loadless_time, false, draw::WHITE);
    }

    u16 world_idx = mkb::scen_info.world;  // index of the current world (between 0 and 9 inclusive)
    TimerDisplayInfo seg_info = get_timer_display_info(TimerType::Segment);

    if (should_display_timer(TimerType::Segment)) {
        timerdisp::draw_timer(seg_info.pos_x, seg_info.pos_y, seg_info.text_offset,
                              "Seg:", s_timer_group[world_idx].segment, false, draw::WHITE);
    }
}

// We only use this function for 0 <= row <= 9
Vec2d get_breakdown_row_position(u16 row) {
    u16 starting_row = get_timer_y_pos(TimerType::Segment);
    u16 pos_y = timerdisp::row_number_to_vertical_pos(starting_row + row);
    if (row < WORLD_COUNT - 1) {
        return {IW_TIME_LOCATION_X, pos_y};
    } else {  // "W10" takes up more space than "Wk" for "1 <= k <= 9"
        return {SEGMENT_TIMER_LOCATION_X, pos_y};
    }
}

void draw_breakdown_screen() {  // TODO: death count per world
    char split_buf[WORLD_COUNT][32] = {};
    char seg_buf[WORLD_COUNT][32] = {};
    char row_info_buf[WORLD_COUNT][32] = {};  // Format: "Wk: split k-1 time (segment k-1 time)"

    for (u16 idx = 0; idx < WORLD_COUNT; idx++) {
        Vec2d pos = get_breakdown_row_position(idx);

        timerdisp::format_time_to_buffer(split_buf[idx], get_split_timer_for_world(idx),
                                         timerdisp::TimeFormatType::MINUTES_ALWAYS_LEADING_ZERO);
        timerdisp::format_time_to_buffer(seg_buf[idx], s_timer_group[idx].segment,
                                         timerdisp::TimeFormatType::MINUTES_ALWAYS_LEADING_ZERO);
        mkb::sprintf(row_info_buf[idx], "W%d: %s (%s)", idx + 1, split_buf[idx], seg_buf[idx]);

        draw::debug_text(pos.x, pos.y, draw::WHITE, "%s", row_info_buf[idx]);
    }
}

void disp() {
    if ((mkb::main_game_mode != mkb::STORY_MODE && mkb::sub_mode != mkb::SMD_AUTHOR_PLAY_INIT &&
         mkb::sub_mode != mkb::SMD_AUTHOR_PLAY_MAIN) ||
        freecam::should_hide_hud()) {
        return;
    }

    draw_timers();

    if (pref::get(pref::BoolPref::ShowRunBreakdown) && is_run_complete()) {
        draw_breakdown_screen();
    }
    /*
        // mkb::scen_info.mode
        timerdisp::draw_timer(SEGMENT_TIMER_LOCATION_X, 2, SEGMENT_TIMER_TEXT_OFFSET,
                              "Dbg:", 60 * s_timer_group[0].full_world, true, draw::WHITE);
        timerdisp::draw_timer(SEGMENT_TIMER_LOCATION_X, 3, SEGMENT_TIMER_TEXT_OFFSET,
                              "Sub:", 60 * mkb::sub_mode, true, draw::WHITE);
        timerdisp::draw_timer(SEGMENT_TIMER_LOCATION_X, 4, SEGMENT_TIMER_TEXT_OFFSET,
                              "Sta:", 60 * mkb::g_storymode_stageselect_state, true, draw::WHITE);
        timerdisp::draw_timer(SEGMENT_TIMER_LOCATION_X, 5, SEGMENT_TIMER_TEXT_OFFSET,
                              "Scn:", 60 * mkb::scen_info.mode, true, draw::WHITE); */

    //
    // --- old stuff (will comment out/remove) ---
    //

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
}

//  void dmd_scen_sel_world_init(void);
//  void dmd_scen_sel_world_next(void);
static patch::Tramp<decltype(&mkb::dmd_scen_sel_world_init)> s_dmd_scen_sel_world_init_tramp;
static patch::Tramp<decltype(&mkb::dmd_scen_sel_world_next)> s_dmd_scen_sel_world_next_tramp;

void init() {
    patch::hook_function(s_dmd_scen_sel_world_init_tramp, mkb::dmd_scen_sel_world_init, []() {
        // s_timer_group[0].full_world += 1;
        // while (true);
        s_dmd_scen_sel_world_init_tramp.dest();
    });

    patch::hook_function(s_dmd_scen_sel_world_next_tramp, mkb::dmd_scen_sel_world_next, []() {
        // s_timer_group[0].full_world += 1;
        s_dmd_scen_sel_world_next_tramp.dest();
    });
}

}  // namespace storytimer