#include "storytimer.h"

#include "../mkb/mkb.h"

#include "../mods/freecam.h"
#include "../mods/validate.h"
#include "../systems/pref.h"
#include "../utils/draw.h"
#include "../utils/macro_utils.h"
#include "../utils/mode.h"
#include "../utils/patch.h"
#include "../utils/timerdisp.h"
#include "deathcounter.h"
#include "validate.h"

namespace storytimer {

constexpr u16 FULLGAME_TIMER_LOCATION_X = 18 + 24;
constexpr u16 FULLGAME_TIMER_TEXT_OFFSET = 56;
constexpr u16 SEGMENT_TIMER_LOCATION_X = 30 + 24;
constexpr u16 SEGMENT_TIMER_TEXT_OFFSET = 44;
constexpr u16 BREAKDOWN_ROW_LOCATION_X = 42 + 24;
constexpr u16 STARTING_ROW = 2;

constexpr u16 WORLD_COUNT = mode::WORLD_COUNT;
constexpr u16 STAGES_PER_WORLD = mode::STAGES_PER_WORLD;

static TimerGroup s_timer_group[WORLD_COUNT];  // each world has its own TimerGroup structure

// static u32 s_counter = 0;  // testing variable

// Some getters that other files can use (if needed)

TimerGroup get_world_timer_info(u16 world_idx) {
    // clamp for safety so we don't access outside the bounds of the array
    u16 clamped_idx = MIN(world_idx, WORLD_COUNT - 1);
    return s_timer_group[clamped_idx];
}

// Used to calculate our split times for the breakdown screen; also gives us a
// convenient way to calculate fullgame loadless time
u32 get_split_timer_for_world(u16 world_idx) {
    u16 clamped_idx = MIN(world_idx, WORLD_COUNT - 1);
    u32 prev_world_sum = 0;
    for (u16 k = 0; k < clamped_idx; k++) {
        prev_world_sum += s_timer_group[k].full_world;
    }
    return prev_world_sum + s_timer_group[clamped_idx].segment;
}

u32 get_loadless_time() { return get_split_timer_for_world(WORLD_COUNT - 1); }

// --- main timer logic ---

void reset_timer() {
    // s_counter = 0;  // debugging
    for (u16 k = 0; k < WORLD_COUNT; k++) {
        s_timer_group[k] = {};
    }
    // mkb::OSReport("Reset timer \n");
}

// Places where the segment timer should run while we're on a stage
bool should_segment_timer_run_on_stage(int world_idx) {
    if (mkb::get_world_unbeaten_stage_count(world_idx) < STAGES_PER_WORLD - 1) {
        return true;
    } else if (mkb::get_world_unbeaten_stage_count(world_idx) == STAGES_PER_WORLD - 1) {
        // Stop the world segment timer on tape break of the last stage of that world
        return !validate::has_entered_goal();
    } else {
        return false;
    }
}

// Importantly, we don't increment the timer on the 10 ball screen after selecting a stage
// since the time between doing so and entering the next stage can be highly variable
// To properly increment the timer on the 10 ball screen, we run this
// update function inside a hook for mkb::g_handle_storymode_stageselect_state
void update_timers_on_10_ball_screen(mkb::StoryModeStageSelectState state) {
    for (u16 k = 0; k < WORLD_COUNT; k++) {
        if (mkb::scen_info.world == k) {  // World (k+1)'s timer
            if (mode::is_on_10_ball_spin_in(state) || mode::is_idle_on_10_ball_screen(state) ||
                mode::has_selected_stage_on_10_ball_screen_init(state)) {
                s_timer_group[k].full_world += 1;
                s_timer_group[k].segment = s_timer_group[k].full_world;
            }
        }
    }
}

// The above function does not get run when the game is paused, so we also need this
void update_timers_while_paused_on_10_ball_screen() {
    for (u16 k = 0; k < WORLD_COUNT; k++) {
        if (mkb::scen_info.world == k) {
            if (mode::is_on_10_ball_screen(mkb::sub_mode, mkb::scen_info) && mode::is_paused()) {
                s_timer_group[k].full_world += 1;
                s_timer_group[k].segment = s_timer_group[k].full_world;
            }
        }
    }
}

// mode::is_on_stage_with_endpoints(mkb::sub_mode)
// Increment the timer on every submode on the stage (and exit game screen)
void update_timers_on_stage() {
    for (u16 k = 0; k < WORLD_COUNT; k++) {
        if (mkb::scen_info.world == k) {  // World (k+1)'s timer
            if (mode::is_on_stage(mkb::sub_mode) || mode::is_story_exit_game(mkb::sub_mode)) {
                s_timer_group[k].full_world += 1;

                if (should_segment_timer_run_on_stage(k)) {
                    s_timer_group[k].segment = s_timer_group[k].full_world;
                }
            }
        }
    }
}

void tick() {
    if (mode::is_main_game_mode_story(mkb::main_game_mode)) {
        if (mode::is_storymode_file_screen_init(mkb::scen_info)) {
            reset_timer();
        }

        update_timers_on_stage();
        update_timers_while_paused_on_10_ball_screen();
    }
}

// --- display stuff ---

// To make things less verbose/cluttered
// bool is_between_worlds() { return mode::is_between_worlds(validate::has_entered_goal()); }
// bool is_run_complete() { return mode::is_run_complete(validate::has_entered_goal()); }

// The run breakdown screen replaces the segment timer at the end of the run
// if the pref for it is on
bool should_display_timer(TimerType type) {
    u8 pref;
    if (type == TimerType::Fullgame) {
        pref = pref::get(pref::U8Pref::FullgameTimerOptions);
    } else {  // Segment timer
        pref = pref::get(pref::U8Pref::SegmentTimerOptions);
    }

    // using namespace validate;

    switch (TimerOptions(pref)) {
        case TimerOptions::AlwaysShow:
            if (type == TimerType::Fullgame) {
                return true;
            } else if (pref::get(pref::BoolPref::ShowRunBreakdown)) {
                // type is segment timer + show breakdown on
                return !validate::is_run_complete();
            } else {  // type is segment timer + show breakdown off
                return true;
            }
        case TimerOptions::BetweenWorlds:
            if (type == TimerType::Fullgame) {
                return validate::is_between_worlds();
            } else if (pref::get(pref::BoolPref::ShowRunBreakdown)) {
                return validate::is_between_worlds() && !validate::is_run_complete();
            } else {
                return validate::is_between_worlds();
            }
        case TimerOptions::EndOfRun:
            if (type == TimerType::Fullgame) {
                return validate::is_run_complete();
            } else if (pref::get(pref::BoolPref::ShowRunBreakdown)) {
                return false;
            } else {
                return validate::is_run_complete();
            }
        case TimerOptions::DontShow:
            return false;
    }
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
    u32 loadless_time = get_loadless_time();

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
        return {BREAKDOWN_ROW_LOCATION_X, pos_y};
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
        mkb::sprintf(row_info_buf[idx], "W%d:%s (%s)", idx + 1, split_buf[idx], seg_buf[idx]);

        draw::debug_text(pos.x, pos.y, draw::WHITE, "%s", row_info_buf[idx]);
    }
}

void disp() {
    if (!mode::is_main_game_mode_story(mkb::main_game_mode) || freecam::should_hide_hud()) {
        return;
    }

    draw_timers();

    if (pref::get(pref::BoolPref::ShowRunBreakdown) && validate::is_run_complete()) {
        draw_breakdown_screen();
    }

    /*
    u16 pos_y = get_timer_y_pos(TimerType::Segment);

    timerdisp::draw_timer(SEGMENT_TIMER_LOCATION_X, pos_y + 1, SEGMENT_TIMER_TEXT_OFFSET,
                          "Dbg:", 60 * s_counter, true, draw::WHITE);
    timerdisp::draw_timer(SEGMENT_TIMER_LOCATION_X, pos_y + 2, SEGMENT_TIMER_TEXT_OFFSET,
                          "Sub:", 60 * mkb::sub_mode, true, draw::WHITE);
    timerdisp::draw_timer(SEGMENT_TIMER_LOCATION_X, pos_y + 3, SEGMENT_TIMER_TEXT_OFFSET,
                          "Sta:", 60 * mkb::g_storymode_stageselect_state, true, draw::WHITE);
    timerdisp::draw_timer(SEGMENT_TIMER_LOCATION_X, pos_y + 4, SEGMENT_TIMER_TEXT_OFFSET,
                          "Scn:", 60 * mkb::scen_info.mode, true, draw::WHITE);
    */
}

// for easier timer testing
static patch::Tramp<decltype(&mkb::fade_screen_to_color)> s_fade_screen_to_color_tramp;
static patch::Tramp<decltype(&mkb::g_handle_storymode_stageselect_state)>
    s_g_handle_storymode_stageselect_state_tramp;

void init_main_loop() {
    /* patch::hook_function(s_fade_screen_to_color_tramp, mkb::fade_screen_to_color,
                         [](mkb::uint flags, u32 color, mkb::uint frames) {
                             // don't run for testing purposes
                             // s_fade_screen_to_color_tramp.dest(flags, color, frames);
                         }); */
    patch::hook_function(s_g_handle_storymode_stageselect_state_tramp,
                         mkb::g_handle_storymode_stageselect_state, []() {
                             s_g_handle_storymode_stageselect_state_tramp.dest();
                             update_timers_on_10_ball_screen(mkb::g_storymode_stageselect_state);
                         });
}

}  // namespace storytimer