#include "watch_ui.h"
#include "watch_design.h"
#include "watch_time_tools.h"
#include "watch_2048.h"
#include "watch_voice_feedback.h"
#include "watch_astronomy.h"
#include "watch_planets.h"
#include "watch_perf_monitor.h"
#include "mochi_halo.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <initializer_list>

namespace {

// Shared visual tokens; no hardware or activity logic belongs in this module.
namespace design = watch_design;
constexpr uint32_t kSurface = design::Surface;
constexpr uint32_t kMuted = design::Secondary;
constexpr uint32_t kAccent = design::Connected;
constexpr uint32_t kWhite = design::Primary;
constexpr uint32_t kWarning = design::Warning;
constexpr uint32_t kMediaAccent = 0x95D5AE;
constexpr uint32_t kTimeouts[] = {5000, 10000, 30000, 60000, 0};

enum class WatchScreen { Home, Launcher, Activity, Settings, Wifi, Bluetooth, Brightness, Timeout, Timer, Stopwatch, Weather, Notification, Pairing, ForgetPhone, Media, Game2048, Voice, Astronomy, Planets };
static WatchScreen current_screen = WatchScreen::Home;
static lv_obj_t *screens[19] = {};
static void refresh_planets();
static void refresh_astronomy();
static watch_ui_voice_cb_t voice_callback;
static lv_obj_t *voice_status, *voice_details, *voice_level, *voice_hold, *voice_clear;
static lv_obj_t *voice_transcription, *voice_answer;
static lv_obj_t *voice_note, *voice_question_heading, *voice_answer_heading;
static lv_obj_t *voice_face, *voice_name, *voice_happy;
static lv_timer_t *voice_face_timer;
static uint8_t voice_face_level = 0;
static bool voice_face_blink = false;
static void refresh_voice_face();
static lv_obj_t *voice_microphone, *voice_waiting;
static uint8_t voice_state = 0;
static bool voice_held = false;
static bool voice_notification_pending = false;
static uint32_t voice_confirmed = 0, voice_total = 0;
static void cancel_voice()
{
    voice_held = false;
    if (voice_callback) voice_callback(3);
    watch_ui_set_voice_text("","");
    watch_ui_set_voice_progress(0,0);
    watch_ui_set_voice(0,0,0,nullptr);
}
static Watch2048 game_2048;
static watch_ui_2048_best_cb_t game_best_callback = nullptr;
static lv_obj_t *game_score, *game_best, *game_board, *game_tiles[16], *game_values[16];
static lv_obj_t *game_overlay, *game_dialog_title, *game_dialog_body, *game_dialog_left, *game_dialog_right;
enum class GameDialog { None, NewGame, Win, Over };
static GameDialog game_dialog = GameDialog::None;
static bool game_save_failed = false;
static void render_game();
static void game_swipe(lv_dir_t direction);
static void save_game_best(bool force)
{
    if (game_best_callback && game_2048.started)
        game_save_failed = !game_best_callback(game_2048.best, force);
}
static lv_obj_t *media_title, *media_artist, *media_status, *media_previous, *media_toggle, *media_next;
static lv_obj_t *media_volume_down, *media_volume_up;
static lv_obj_t *media_record = nullptr;
static bool media_playing = false, media_record_spinning = false;
static void refresh_media_record();
static watch_ui_media_command_cb_t media_callback = nullptr;
static uint32_t media_session = 0;
static lv_obj_t *pairing_code, *pairing_accept, *pairing_reject, *security_label, *forget_phone_button;
static uint32_t pairing_token;
static WatchScreen pairing_return = WatchScreen::Home;
static watch_ui_pairing_cb_t pairing_callback = nullptr;
static watch_ui_forget_phone_cb_t forget_callback = nullptr;
static lv_obj_t *notification_app, *notification_title, *notification_body;
static WatchScreen notification_return = WatchScreen::Home;
static watch_ui_notification_dismiss_cb_t notification_dismiss_callback = nullptr;
static lv_obj_t *forecast_status, *forecast_names[7], *forecast_values[7], *forecast_icons[7];
static watch_ui_forecast_day_t forecast_days[7] = {};
static bool forecast_online = false;
static uint32_t forecast_age = 0;
static void render_forecast();
static bool display_active = true;
static WatchTimeTool countdown, stopwatch;
static bool pomodoro_break=false;
static unsigned pomodoro_completed=0;
static uint64_t pomodoro_duration() { return uint64_t(pomodoro_break ? (pomodoro_completed==4 ? 15 : 5) : 25)*60000; }
static void pomodoro_next_cb(lv_event_t *);
static lv_obj_t *timer_value, *timer_status, *timer_action_label;
static lv_obj_t *pomodoro_progress;
static lv_obj_t *pomodoro_phase, *pomodoro_sessions[4], *pomodoro_start;
static lv_obj_t *stopwatch_value, *stopwatch_status, *stopwatch_action_label;
static void refresh_time_tools();
static lv_obj_t *time_label, *date_label, *steps_label, *activity_steps_label;
static lv_obj_t *battery_label, *wifi_status_label, *wifi_detail_label;
static lv_obj_t *bluetooth_status_label;
static lv_obj_t *wifi_name_label;
static lv_obj_t *wifi_signal_label, *launcher_wifi_label;
static lv_obj_t *wifi_action, *wifi_action_label;
static lv_obj_t *history_names[7], *history_counts[7];
static lv_obj_t *weather_label, *weather_icon;
static lv_obj_t *weather_category_label;
static lv_obj_t *walk_icon;
static lv_obj_t *steps_goal_label, *steps_progress, *activity_goal_label, *goal_minus, *goal_plus;
static lv_obj_t *weather_range_label;
static uint32_t daily_step_goal = 8000, today_steps = 0;
static watch_ui_step_goal_cb_t step_goal_callback = nullptr;
static void refresh_step_goal();
static char clock_text[6] = "--:--";
static watch_ui_weather_condition_t weather_condition = WATCH_UI_WEATHER_SUNNY;
static watch_ui_wifi_state_t wifi_state = WATCH_UI_WIFI_DISCONNECTED;
static watch_ui_wifi_control_cb_t wifi_callback = nullptr;
static watch_ui_bluetooth_control_cb_t bluetooth_callback = nullptr;
static watch_ui_bluetooth_state_t bluetooth_state = WATCH_UI_BLUETOOTH_DISABLED;
static lv_obj_t *launcher_bluetooth_label, *bluetooth_detail_label, *bluetooth_action, *bluetooth_action_label;
static lv_obj_t *brightness_label, *brightness_slider, *screen_timeout_dropdown;
static lv_obj_t *settings_brightness_value, *settings_timeout_value;
static lv_obj_t *settings_icon(lv_obj_t *parent, bool stopwatch, uint32_t color);
static void refresh_settings_values();
static uint8_t current_brightness = 60;
static uint32_t display_timeout_ms = 10000;
static watch_ui_brightness_cb_t brightness_callback = nullptr;
static watch_ui_brightness_committed_cb_t brightness_committed_callback = nullptr;
static watch_ui_timeout_cb_t timeout_callback = nullptr;

static lv_obj_t *screen_for(WatchScreen screen)
{
    return screens[static_cast<unsigned>(screen)];
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                       uint32_t color, lv_align_t align, int x, int y)
{
    lv_obj_t *object = lv_label_create(parent);
    lv_label_set_text(object, text);
    lv_obj_set_style_text_font(object, font, 0);
    lv_obj_set_style_text_color(object, lv_color_hex(color), 0);
    lv_obj_align(object, align, x, y);
    return object;
}

static lv_screen_load_anim_t navigation_animation(WatchScreen from, WatchScreen to)
{
    // Home buttons map spatially: Settings is left, Apps is right.
    if (from == WatchScreen::Home && to == WatchScreen::Settings) return LV_SCR_LOAD_ANIM_MOVE_RIGHT;
    if (from == WatchScreen::Settings && to == WatchScreen::Home) return LV_SCR_LOAD_ANIM_MOVE_LEFT;
    const auto settings_option = [](WatchScreen screen) {
        return screen == WatchScreen::Wifi || screen == WatchScreen::Bluetooth ||
               screen == WatchScreen::Brightness || screen == WatchScreen::Timeout;
    };
    if (from == WatchScreen::Settings && settings_option(to)) return LV_SCR_LOAD_ANIM_MOVE_RIGHT;
    if (settings_option(from) && to == WatchScreen::Settings) return LV_SCR_LOAD_ANIM_MOVE_LEFT;
    const bool back = to == WatchScreen::Home ||
        ((to == WatchScreen::Launcher || to == WatchScreen::Settings) && from != WatchScreen::Home);
    return back ? LV_SCR_LOAD_ANIM_MOVE_RIGHT : LV_SCR_LOAD_ANIM_MOVE_LEFT;
}

static void navigate_to(WatchScreen screen, bool animate = true)
{
    if (screen == current_screen) return;
    if (current_screen == WatchScreen::Game2048) save_game_best(true);
    if (current_screen == WatchScreen::Voice && screen != WatchScreen::Notification) {
        voice_notification_pending = false;
        cancel_voice();
    }
    if (animate) lv_screen_load_anim(screen_for(screen), navigation_animation(current_screen, screen), design::TransitionMs, 0, false);
    else lv_screen_load(screen_for(screen));
    current_screen = screen;
    refresh_voice_face();
    refresh_media_record();
    refresh_time_tools();
    refresh_astronomy();
    refresh_planets();
    if (screen == WatchScreen::Weather) render_forecast();
    if (screen == WatchScreen::Game2048) render_game();
}

static void open_launcher_cb(lv_event_t *) { navigate_to(WatchScreen::Launcher); }
static void open_activity_cb(lv_event_t *) { navigate_to(WatchScreen::Activity); }
static void open_settings_cb(lv_event_t *) { navigate_to(WatchScreen::Settings); }
static void open_wifi_cb(lv_event_t *) { navigate_to(WatchScreen::Wifi); }
static void open_bluetooth_cb(lv_event_t *) { navigate_to(WatchScreen::Bluetooth); }
static void open_brightness_cb(lv_event_t *) { navigate_to(WatchScreen::Brightness); }
static void open_timeout_cb(lv_event_t *) { navigate_to(WatchScreen::Timeout); }
static void open_timer_cb(lv_event_t *) { navigate_to(WatchScreen::Timer); }
static void open_stopwatch_cb(lv_event_t *) { navigate_to(WatchScreen::Stopwatch); }
static void open_weather_cb(lv_event_t *) { navigate_to(WatchScreen::Weather); }
static void open_astronomy_cb(lv_event_t *) { navigate_to(WatchScreen::Astronomy); }
static void open_planets_cb(lv_event_t *) { navigate_to(WatchScreen::Planets); }
static void open_media_cb(lv_event_t *) { navigate_to(WatchScreen::Media); }
static void open_game_cb(lv_event_t *)
{
    if (!game_2048.started) game_2048.new_game();
    navigate_to(WatchScreen::Game2048);
}
static void open_voice_cb(lv_event_t *) { navigate_to(WatchScreen::Voice); }
static void step_goal_changed_cb(lv_event_t *event)
{
    const bool increase = lv_event_get_target(event) == goal_plus;
    if ((increase && daily_step_goal >= 30000) || (!increase && daily_step_goal <= 1000)) return;
    const uint32_t goal = increase ? daily_step_goal + 1000 : daily_step_goal - 1000;
    if (step_goal_callback && !step_goal_callback(goal)) {
        lv_label_set_text(activity_goal_label, "Save failed / retry");
        return;
    }
    watch_ui_set_step_goal(goal);
}
static void bluetooth_action_cb(lv_event_t *)
{
    if (!bluetooth_callback) return;
    const bool enable = bluetooth_state == WATCH_UI_BLUETOOTH_DISABLED;
    if (bluetooth_callback(enable)) {
        lv_obj_add_state(bluetooth_action, LV_STATE_DISABLED);
        lv_label_set_text(bluetooth_action_label, enable ? "Enabling..." : "Disabling...");
    }
}
static void wifi_action_cb(lv_event_t *)
{
    if (wifi_state == WATCH_UI_WIFI_CONNECTING || !wifi_callback) return;
    const bool enable = wifi_state == WATCH_UI_WIFI_DISCONNECTED;
    if (wifi_callback(enable)) {
        if (enable) watch_ui_set_wifi_state(WATCH_UI_WIFI_CONNECTING, 0);
        else {
            lv_obj_add_state(wifi_action, LV_STATE_DISABLED);
            lv_label_set_text(wifi_action_label, "Disconnecting...");
        }
    }
}
static void back_cb(lv_event_t *)
{
    if (current_screen == WatchScreen::Pairing) {
        if (pairing_callback) pairing_callback(pairing_token, false);
        return;
    }
    if (current_screen == WatchScreen::ForgetPhone) { navigate_to(WatchScreen::Bluetooth); return; }
    if (current_screen == WatchScreen::Notification) {
        if (notification_dismiss_callback) notification_dismiss_callback();
        else watch_ui_hide_notification();
        return;
    }
    const bool settings_child = current_screen == WatchScreen::Wifi || current_screen == WatchScreen::Bluetooth ||
        current_screen == WatchScreen::Brightness || current_screen == WatchScreen::Timeout;
    navigate_to(current_screen == WatchScreen::Launcher || current_screen == WatchScreen::Settings ?
                WatchScreen::Home : settings_child ? WatchScreen::Settings : WatchScreen::Launcher);
}

static void process_swipe(lv_dir_t direction)
{
    if (current_screen == WatchScreen::Voice && voice_held) { cancel_voice(); return; }
    if (current_screen == WatchScreen::Game2048) { game_swipe(direction); return; }
    if (current_screen == WatchScreen::Home && direction == LV_DIR_LEFT) navigate_to(WatchScreen::Launcher);
    else if (current_screen != WatchScreen::Home && direction == LV_DIR_RIGHT) back_cb(nullptr);
}
static void navigation_gesture_cb(lv_event_t *event)
{
    lv_indev_t *input = lv_indev_active();
    if (input == nullptr) return;
    const lv_dir_t direction = lv_indev_get_gesture_dir(input);
    lv_indev_wait_release(input); // Exactly one game move per touch gesture.
    if (current_screen == WatchScreen::Game2048 && event) lv_event_stop_bubbling(event);
    process_swipe(direction);
}

static void style_surface(lv_obj_t *object)
{
    lv_obj_set_style_bg_color(object, lv_color_hex(kSurface), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(object, design::Radius, 0);
    lv_obj_set_style_border_width(object, design::BorderWidth, 0);
    lv_obj_set_style_border_color(object, lv_color_hex(design::Border), 0);
    lv_obj_set_style_shadow_width(object, 0, 0);
    lv_obj_set_style_pad_all(object, 0, 0);
}

static void style_press_feedback(lv_obj_t *object)
{
    lv_obj_set_style_bg_color(object, lv_color_hex(design::Pressed), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(object, lv_color_hex(design::Primary), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(object, LV_OPA_50, LV_STATE_DISABLED);
}

static lv_obj_t *button(lv_obj_t *screen, const char *text, int width, int height,
                        lv_align_t align, int x, int y, lv_event_cb_t callback)
{
    lv_obj_t *object = lv_button_create(screen);
    lv_obj_set_size(object, width < design::TouchHeight ? design::TouchHeight : width,
                    height < design::TouchHeight ? design::TouchHeight : height);
    lv_obj_align(object, align, x, y);
    style_surface(object);
    style_press_feedback(object);
    lv_obj_set_style_text_color(object, lv_color_hex(design::Disabled), LV_STATE_DISABLED);
    lv_obj_set_style_text_color(object, lv_color_hex(kWhite), 0);
    if (callback) lv_obj_add_event_cb(object, callback, LV_EVENT_CLICKED, nullptr);
    // Inherit text color from the button so disabled/pressed states apply to
    // every app consistently (a fixed child color previously masked them).
    auto *text_label = lv_label_create(object);
    lv_label_set_text(text_label,text);
    lv_obj_set_style_text_font(text_label,design::body(),0);
    lv_obj_center(text_label);
    return object;
}

static void center_navigation_content(lv_obj_t *object)
{
    auto *caption = lv_obj_get_child(object,0);
    const char *text = lv_label_get_text(caption);
    const char *separator = strstr(text,"  ");
    if (!separator || separator-text >= 8) return;
    char icon_text[8] = {}, title[32];
    memcpy(icon_text,text,separator-text);
    snprintf(title,sizeof(title),"%s",separator+2);
    lv_label_set_text(caption,icon_text);
    auto *title_label = lv_label_create(object);
    lv_label_set_text(title_label,title);
    lv_obj_t *parts[] = {caption,title_label};
    for (auto *part : parts) {
        lv_obj_set_style_text_font(part,&lv_font_montserrat_16,0);
        lv_obj_set_style_text_align(part,LV_TEXT_ALIGN_CENTER,0);
        lv_obj_remove_flag(part,LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_set_flex_flow(object,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(object,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(object,8,0);
}

static lv_obj_t *home_navigation_button(lv_obj_t *screen, bool settings)
{
    lv_obj_t *object = button(screen, settings ? LV_SYMBOL_SETTINGS "  SETTINGS" : LV_SYMBOL_LIST "  APPS",
                             136, design::TouchHeight, LV_ALIGN_BOTTOM_MID,
                             settings ? -76 : 76, -design::BackInset - design::Unit,
                             settings ? open_settings_cb : open_launcher_cb);
    center_navigation_content(object);
    lv_obj_set_style_radius(object,design::TouchHeight/2,0);
    return object;
}

static lv_obj_t *create_screen(WatchScreen id, const char *title)
{
    lv_obj_t *screen = id == WatchScreen::Home ? lv_screen_active() : lv_obj_create(nullptr);
    screens[static_cast<unsigned>(id)] = screen;
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen, navigation_gesture_cb, LV_EVENT_GESTURE, nullptr);
    if (title != nullptr) {
        char heading[40];
        snprintf(heading, sizeof(heading), "%s", title);
        for (char *p = heading; *p; ++p) *p = static_cast<char>(toupper(static_cast<unsigned char>(*p)));
        lv_obj_t *heading_label = label(screen, heading, design::title(), kWhite, LV_ALIGN_TOP_MID, 0, design::HeaderY);
        lv_obj_set_width(heading_label,design::ContentWidth);
        lv_obj_set_style_text_align(heading_label,LV_TEXT_ALIGN_CENTER,0);
        lv_label_set_long_mode(heading_label,LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_letter_space(heading_label, 2, 0);
        const bool parent_screen = id == WatchScreen::Launcher || id == WatchScreen::Settings;
        auto *navigation = button(screen, id == WatchScreen::Notification ? LV_SYMBOL_CLOSE "  DISMISS" : parent_screen ? LV_SYMBOL_HOME "  HOME" : LV_SYMBOL_LEFT "  BACK",
               id == WatchScreen::Notification ? 240 : design::BackWidth,
               design::TouchHeight, LV_ALIGN_BOTTOM_MID, 0, -design::BackInset, back_cb);
        if (id != WatchScreen::Notification) {
            lv_obj_set_style_radius(navigation,design::TouchHeight/2,0);
            center_navigation_content(navigation);
        }
    }
    return screen;
}

// Tiny procedural icons: no image buffers/assets or Unicode emoji font required.
static uint32_t weather_icon_color(watch_ui_weather_condition_t condition)
{
    switch (condition) {
    case WATCH_UI_WEATHER_SUNNY: return design::Weather;
    case WATCH_UI_WEATHER_RAIN: return design::WeatherRain;
    case WATCH_UI_WEATHER_THUNDERSTORM: return design::WeatherStorm;
    case WATCH_UI_WEATHER_WINDY: return design::WeatherWind;
    case WATCH_UI_WEATHER_CLOUDY: default: return design::WeatherCloudy;
    }
}

static void weather_draw_cb(lv_event_t *event)
{
    lv_area_t area;
    lv_obj_get_coords(static_cast<lv_obj_t *>(lv_event_get_target(event)), &area);
    const auto *day = static_cast<watch_ui_forecast_day_t *>(lv_event_get_user_data(event));
    const auto condition = day ? day->condition : weather_condition;
    const auto color = lv_color_hex(weather_icon_color(condition));
    lv_layer_t *layer = lv_event_get_layer(event);
    auto line = [&](int x1, int y1, int x2, int y2) {
        lv_draw_line_dsc_t dsc;
        lv_draw_line_dsc_init(&dsc);
        dsc.color = color;
        dsc.width = design::IconStroke;
        dsc.p1 = {static_cast<lv_value_precise_t>(area.x1 + x1), static_cast<lv_value_precise_t>(area.y1 + y1)};
        dsc.p2 = {static_cast<lv_value_precise_t>(area.x1 + x2), static_cast<lv_value_precise_t>(area.y1 + y2)};
        lv_draw_line(layer, &dsc);
    };
    auto disc = [&](int x, int y, int width, int height) {
        lv_draw_rect_dsc_t dsc;
        lv_draw_rect_dsc_init(&dsc);
        dsc.bg_color = color;
        dsc.radius = LV_RADIUS_CIRCLE;
        lv_area_t box = {area.x1 + x, area.y1 + y, area.x1 + x + width - 1, area.y1 + y + height - 1};
        lv_draw_rect(layer, &dsc, &box);
    };
    if (condition == WATCH_UI_WEATHER_SUNNY) {
        disc(10, 10, 14, 14);
        line(17, 1, 17, 6); line(17, 28, 17, 33);
        line(1, 17, 6, 17); line(28, 17, 33, 17);
        line(4, 4, 8, 8); line(26, 26, 30, 30);
        line(4, 30, 8, 26); line(26, 8, 30, 4);
    } else if (condition == WATCH_UI_WEATHER_WINDY) {
        line(2, 9, 25, 9); line(7, 17, 32, 17); line(2, 25, 23, 25);
        line(25, 9, 28, 5); line(32, 17, 29, 21);
    } else {
        disc(3, 11, 28, 12); disc(8, 5, 16, 16); disc(19, 9, 13, 13);
        if (condition == WATCH_UI_WEATHER_RAIN) {
            line(9, 26, 6, 32); line(18, 26, 15, 32); line(27, 26, 24, 32);
        } else if (condition == WATCH_UI_WEATHER_THUNDERSTORM) {
            line(20, 23, 13, 28); line(13, 28, 20, 28); line(20, 28, 13, 34);
        }
    }
}

static void footprints_draw_cb(lv_event_t *event)
{
    lv_obj_t *object = static_cast<lv_obj_t *>(lv_event_get_target(event));
    lv_area_t area;
    lv_obj_get_coords(object, &area);
    lv_layer_t *layer = lv_event_get_layer(event);
    const int size = lv_obj_get_width(object);
    auto oval = [&](int x, int y, int width, int height) {
        // Exchange the current upper-left/lower-right footprints as whole shapes.
        // Preserve each foot's toe orientation rather than mirroring the pair again.
        const bool lower_right_foot = x < 16;
        x = 32 - x - width + (lower_right_foot ? -14 : 14);
        y += lower_right_foot ? -7 : 7;
        lv_draw_rect_dsc_t dsc;
        lv_draw_rect_dsc_init(&dsc);
        dsc.bg_color = lv_obj_get_style_text_color(object, LV_PART_MAIN);
        dsc.bg_opa = LV_OPA_TRANSP;
        dsc.border_color = dsc.bg_color;
        dsc.border_width = design::IconStroke;
        dsc.radius = LV_RADIUS_CIRCLE;
        lv_area_t box = {area.x1 + x * size / 32, area.y1 + y * size / 32,
            area.x1 + (x + width) * size / 32 - 1,
            area.y1 + (y + height) * size / 32 - 1};
        lv_draw_rect(layer, &dsc, &box);
    };
    // Staggered soles and separate toes suggest two walking footprints.
    oval(5, 15, 8, 15); oval(5, 8, 5, 5); oval(11, 10, 3, 4);
    oval(19, 8, 8, 15); oval(22, 1, 5, 5); oval(18, 3, 3, 4);
}

static lv_obj_t *footprints_icon(lv_obj_t *parent, int size, uint32_t color)
{
    lv_obj_t *icon = lv_obj_create(parent);
    lv_obj_remove_style_all(icon);
    lv_obj_set_size(icon, size, size);
    lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(icon, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_color(icon, lv_color_hex(color), 0);
    lv_obj_add_event_cb(icon, footprints_draw_cb, LV_EVENT_DRAW_MAIN, nullptr);
    return icon;
}

// Original 5x7 numeric dot patterns: no external typeface or bitmap assets.
static void clock_draw_cb(lv_event_t *event)
{
    static constexpr uint8_t digits[10][7] = {
        {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
        {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
        {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
        {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8},
        {14,17,17,14,17,17,14}, {14,17,17,15,1,1,14}
    };
    lv_area_t area;
    lv_obj_get_coords(time_label, &area);
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_draw_rect_dsc_t dot;
    lv_draw_rect_dsc_init(&dot);
    dot.bg_color = lv_color_hex(kWhite);
    dot.radius = LV_RADIUS_CIRCLE;
    constexpr int drawn_clock_width = 278;
    int x = area.x1 + (design::ClockWidth - drawn_clock_width) / 2;
    for (unsigned i = 0; i < 5; ++i) {
        const char c = clock_text[i];
        for (int row = 0; row < 7; ++row) {
            const uint8_t bits = c >= '0' && c <= '9' ? digits[c - '0'][row] :
                c == '-' && row == 3 ? 14 : 0;
            for (int col = 0; col < (c == ':' ? 1 : 5); ++col) {
                const bool on = c == ':' ? row == 2 || row == 4 : bits & (1 << (4 - col));
                if (!on) continue;
                lv_area_t box = {x + col * 12, area.y1 + row * 12,
                    x + col * 12 + 7, area.y1 + row * 12 + 7};
                lv_draw_rect(layer, &dot, &box);
            }
        }
        x += c == ':' ? 24 : 66;
    }
}

#include "watch_astronomy_ui.inc"
#include "watch_planets_ui.inc"

static void create_home_screen()
{
    lv_obj_t *screen = create_screen(WatchScreen::Home, nullptr);
    battery_label = label(screen, LV_SYMBOL_BATTERY_FULL "  --%", &lv_font_montserrat_18,
                          kMuted, LV_ALIGN_TOP_RIGHT, -34, 34);
    wifi_status_label = label(screen, LV_SYMBOL_WIFI, &lv_font_montserrat_18,
                              kMuted, LV_ALIGN_TOP_LEFT, 34, 34);
    bluetooth_status_label = label(screen, LV_SYMBOL_BLUETOOTH " Disabled", &lv_font_montserrat_18,
                                    kMuted, LV_ALIGN_TOP_MID, 0, 34);
    time_label = lv_obj_create(screen);
    lv_obj_remove_style_all(time_label);
    lv_obj_set_size(time_label, design::ClockWidth, design::ClockHeight);
    lv_obj_align(time_label, LV_ALIGN_TOP_MID, 0, design::ClockY);
    lv_obj_remove_flag(time_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(time_label, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(time_label, clock_draw_cb, LV_EVENT_DRAW_MAIN, nullptr);
    date_label = label(screen, "Waiting for local date", &lv_font_montserrat_20, kMuted,
                       LV_ALIGN_TOP_MID, 0, design::DateY);
    lv_obj_t *steps = lv_button_create(screen);
    lv_obj_remove_style_all(steps);
    lv_obj_set_size(steps, design::TileWidth, design::WeatherHeight);
    lv_obj_align(steps, LV_ALIGN_TOP_MID, -design::TileOffset, design::WeatherY);
    style_surface(steps);
    style_press_feedback(steps);
    lv_obj_remove_flag(steps, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(steps, open_activity_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *steps_title = label(steps, "STEPS", design::caption(), kMuted,
                                 LV_ALIGN_TOP_MID, 12, 12);
    walk_icon = footprints_icon(steps, 20, design::Activity);
    lv_obj_align_to(walk_icon, steps_title, LV_ALIGN_OUT_LEFT_MID, -6, 0);
    steps_label = label(steps, "0", &lv_font_montserrat_26, design::Activity,
                          LV_ALIGN_TOP_MID, 0, 44);
    steps_goal_label = label(steps, "Goal 8,000", design::caption(), kMuted, LV_ALIGN_TOP_MID, 0, 78);
    steps_progress = lv_bar_create(steps);
    lv_obj_remove_style_all(steps_progress);
    lv_obj_set_size(steps_progress, design::TileWidth - 24, 3);
    lv_obj_align(steps_progress, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_bg_color(steps_progress, lv_color_hex(design::Border), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(steps_progress, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(steps_progress, lv_color_hex(design::Activity), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(steps_progress, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_remove_flag(steps_progress, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *weather = lv_button_create(screen);
    lv_obj_set_size(weather, design::TileWidth, design::WeatherHeight);
    lv_obj_align(weather, LV_ALIGN_TOP_MID, design::TileOffset, design::WeatherY);
    style_surface(weather);
    style_press_feedback(weather);
    lv_obj_add_event_cb(weather, open_weather_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_remove_flag(weather, LV_OBJ_FLAG_SCROLLABLE);
    label(weather, "MADRID", design::caption(), kMuted, LV_ALIGN_TOP_MID, 0, 12);
    weather_icon = lv_obj_create(weather);
    lv_obj_remove_style_all(weather_icon);
    lv_obj_set_size(weather_icon, 36, 36);
    lv_obj_align(weather_icon, LV_ALIGN_TOP_MID, 48, 40);
    lv_obj_remove_flag(weather_icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(weather_icon, weather_draw_cb, LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_add_flag(weather_icon, LV_OBJ_FLAG_HIDDEN);
    weather_label = label(weather, "--", &lv_font_montserrat_26, kWhite,
                          LV_ALIGN_TOP_MID, -20, 44);
    weather_range_label = label(weather, "H -- / L --", design::caption(), kMuted, LV_ALIGN_TOP_MID, 0, 84);
    weather_category_label = label(screen, "WEATHER / UNAVAILABLE", design::caption(), kMuted,
                                   LV_ALIGN_TOP_MID, 0, 400);
    lv_obj_add_flag(weather_category_label, LV_OBJ_FLAG_HIDDEN);
    home_navigation_button(screen, true);
    home_navigation_button(screen, false);
}

static lv_obj_t *app_tile(lv_obj_t *screen, const char *text, const char *symbol,
                          int x, int y, uint32_t color, lv_event_cb_t callback)
{
    lv_obj_t *tile = button(screen, text, design::TileWidth, design::TileHeight,
                            LV_ALIGN_TOP_MID, x, y, callback);
    lv_obj_align(lv_obj_get_child(tile, 0), LV_ALIGN_TOP_MID, 0, 66);
    label(tile, symbol, &lv_font_montserrat_26, color, LV_ALIGN_TOP_MID, 0, 22);
    return tile;
}

// Launcher-only sizing: Settings deliberately retains the original app_tile.
static lv_obj_t *launcher_tile(lv_obj_t *screen, const char *text, const char *symbol,
                               int x, int y, uint32_t color, lv_event_cb_t callback)
{
    auto *tile = button(screen,text,108,100,LV_ALIGN_TOP_MID,x,y,callback);
    auto *caption = lv_obj_get_child(tile,0);
    lv_obj_set_style_text_font(caption,design::caption(),0);
    lv_obj_align(caption,LV_ALIGN_TOP_MID,0,70);
    label(tile,symbol,&lv_font_montserrat_30,color,LV_ALIGN_TOP_MID,0,24);
    return tile;
}

static void voice_happy_draw(lv_event_t *event);

static void tomato_draw_cb(lv_event_t *event)
{
    lv_area_t a; lv_obj_get_coords(static_cast<lv_obj_t *>(lv_event_get_target(event)),&a);
    auto *layer=lv_event_get_layer(event);
    lv_draw_rect_dsc_t body; lv_draw_rect_dsc_init(&body);
    body.bg_color=lv_color_hex(0xEE594E); body.bg_opa=LV_OPA_COVER; body.radius=LV_RADIUS_CIRCLE;
    lv_area_t bounds={a.x1+2,a.y1+10,a.x1+37,a.y1+37}; lv_draw_rect(layer,&body,&bounds);
    body.bg_color=lv_color_hex(0x86C978); body.radius=3;
    bounds={a.x1+18,a.y1+2,a.x1+22,a.y1+16}; lv_draw_rect(layer,&body,&bounds);
    bounds={a.x1+10,a.y1+10,a.x1+30,a.y1+14}; lv_draw_rect(layer,&body,&bounds);
}
static void tomato_icon(lv_obj_t *parent,lv_align_t align,int x,int y)
{
    auto *icon=lv_obj_create(parent); lv_obj_remove_style_all(icon);
    lv_obj_set_size(icon,40,40); lv_obj_align(icon,align,x,y);
    lv_obj_remove_flag(icon,LV_OBJ_FLAG_CLICKABLE); lv_obj_remove_flag(icon,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(icon,tomato_draw_cb,LV_EVENT_DRAW_MAIN,nullptr);
}
static void create_launcher_screen()
{
    lv_obj_t *screen = create_screen(WatchScreen::Launcher, "Apps");
    lv_obj_t *activity = launcher_tile(screen, "ACTIVITY", "", -118, 92, design::Activity, open_activity_cb);
    lv_obj_t *activity_icon = footprints_icon(activity, 36, design::Activity);
    lv_obj_align(activity_icon, LV_ALIGN_TOP_MID, 0, 26);
    auto *pomodoro_tile=launcher_tile(screen, "POMODORO", "", 0, 92, 0xEE594E, open_timer_cb);
    tomato_icon(pomodoro_tile,LV_ALIGN_TOP_MID,0,20);
    launcher_tile(screen, "STOPWATCH", LV_SYMBOL_PLAY, 118, 92, design::Stopwatch, open_stopwatch_cb);
    lv_obj_t *weather = launcher_tile(screen, "WEATHER", "", -118, 202, design::Weather, open_weather_cb);
    launcher_tile(screen, "MEDIA", LV_SYMBOL_AUDIO, 0, 202, kMediaAccent, open_media_cb);
    auto *game = launcher_tile(screen, "2048", "", 118, 202, design::Activity, open_game_cb);
    auto *ai = launcher_tile(screen, "AI ASSISTANT", "", 118, 312, design::Timeout, open_voice_cb);
    lv_label_set_text(lv_obj_get_child(ai,0),"AI ASSISTANT");
    lv_obj_set_style_text_font(lv_obj_get_child(ai,0),design::caption(),0);
    lv_obj_align(lv_obj_get_child(ai,0),LV_ALIGN_TOP_MID,0,70);
    auto *bot = lv_obj_create(ai); lv_obj_remove_style_all(bot);
    lv_obj_set_size(bot,52,52); lv_obj_align(bot,LV_ALIGN_TOP_MID,0,12);
    lv_obj_remove_flag(bot,LV_OBJ_FLAG_CLICKABLE); lv_obj_remove_flag(bot,LV_OBJ_FLAG_SCROLLABLE);
    static bool open_eyes=true;
    lv_obj_add_event_cb(bot,voice_happy_draw,LV_EVENT_DRAW_MAIN,&open_eyes);
    auto *astronomy_tile = launcher_tile(screen,"SUN & MOON","",-118,312,design::Weather,open_astronomy_cb);
    auto *astronomy_caption = lv_obj_get_child(astronomy_tile,0);
    lv_obj_set_style_text_font(astronomy_caption,design::caption(),0);
    lv_obj_align(astronomy_caption,LV_ALIGN_TOP_MID,0,70);
    auto *astronomy_icon = lv_obj_create(astronomy_tile); lv_obj_remove_style_all(astronomy_icon);
    lv_obj_set_size(astronomy_icon,44,28); lv_obj_align(astronomy_icon,LV_ALIGN_TOP_MID,0,24);
    lv_obj_remove_flag(astronomy_icon,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_add_event_cb(astronomy_icon,astronomy_icon_draw,LV_EVENT_DRAW_MAIN,nullptr);
    auto *planets_tile=launcher_tile(screen,"PLANETS","",0,312,design::Bluetooth,open_planets_cb);
    lv_obj_set_style_text_font(lv_obj_get_child(planets_tile,0),design::caption(),0);
    lv_obj_align(lv_obj_get_child(planets_tile,0),LV_ALIGN_TOP_MID,0,70);
    auto *planet_icon=lv_obj_create(planets_tile); lv_obj_remove_style_all(planet_icon);
    lv_obj_set_size(planet_icon,44,32); lv_obj_align(planet_icon,LV_ALIGN_TOP_MID,0,24);
    lv_obj_remove_flag(planet_icon,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_add_event_cb(planet_icon,planets_icon_draw,LV_EVENT_DRAW_MAIN,nullptr);
    for (unsigned i = 0; i < 4; ++i) {
        auto *square = lv_obj_create(game);
        lv_obj_remove_style_all(square);
        lv_obj_set_size(square, 12, 12);
        lv_obj_set_pos(square, 40 + (i % 2) * 16, 24 + (i / 2) * 16);
        lv_obj_set_style_bg_color(square, lv_color_hex(design::Activity), 0);
        lv_obj_set_style_bg_opa(square, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(square, 2, 0);
        lv_obj_remove_flag(square, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_t *icon = lv_obj_create(weather);
    lv_obj_remove_style_all(icon);
    lv_obj_set_size(icon, 36, 36);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(icon, LV_OBJ_FLAG_SCROLLABLE);
    static watch_ui_forecast_day_t sunny = {true, 0, 0, 0, 0, WATCH_UI_WEATHER_SUNNY};
    lv_obj_add_event_cb(icon, weather_draw_cb, LV_EVENT_DRAW_MAIN, &sunny);
}

static void create_forecast_screen()
{
    lv_obj_t *screen = create_screen(WatchScreen::Weather, "Madrid weather");
    forecast_status = label(screen, "Forecast unavailable", design::caption(), kMuted, LV_ALIGN_TOP_MID, 0, 82);
    label(screen, "7 DAYS / HIGH - LOW / \xC2\xB0" "C", design::caption(), kMuted, LV_ALIGN_TOP_MID, 0, 108);
    for (unsigned i = 0; i < 7; ++i) {
        const int y = 138 + i * 36;
        forecast_names[i] = label(screen, "--", design::caption(), i == 0 ? kWhite : kMuted, LV_ALIGN_TOP_LEFT, design::Margin, y + 8);
        lv_obj_t *icon = lv_obj_create(screen);
        lv_obj_remove_style_all(icon);
        lv_obj_set_size(icon, 36, 36);
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, y);
        lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(icon, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(icon, weather_draw_cb, LV_EVENT_DRAW_MAIN, &forecast_days[i]);
        forecast_icons[i] = icon;
        forecast_values[i] = label(screen, "--", design::body(), kWhite, LV_ALIGN_TOP_RIGHT, -design::Margin, y + 8);
    }
    label(screen, "Weather data: Open-Meteo", design::caption(), kMuted, LV_ALIGN_TOP_MID, 0, 400);
    render_forecast();
}

static void render_forecast()
{
    if (!forecast_status) return;
    bool any = false;
    for (const auto &day : forecast_days) any |= day.available;
    if (!any) lv_label_set_text(forecast_status, forecast_online ? "Loading forecast..." : "Forecast unavailable / offline");
    else lv_label_set_text_fmt(forecast_status, "%s / %lum old",
        !forecast_online ? "OFFLINE / CACHED" : forecast_age >= 35 ? "STALE / CACHED" : "MADRID",
        static_cast<unsigned long>(forecast_age));
    static const char *weekdays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    for (unsigned i = 0; i < 7; ++i) {
        const auto &day = forecast_days[i];
        if (day.date) lv_label_set_text_fmt(forecast_names[i], "%s %02lu/%02lu", i == 0 ? "TODAY" : weekdays[day.weekday % 7],
            static_cast<unsigned long>(day.date % 100), static_cast<unsigned long>(day.date / 100 % 100));
        else lv_label_set_text(forecast_names[i], "--");
        if (day.available) {
            lv_label_set_text_fmt(forecast_values[i], "%.0f / %.0f", day.high_c, day.low_c);
            lv_obj_remove_flag(forecast_icons[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_invalidate(forecast_icons[i]);
        } else {
            lv_label_set_text(forecast_values[i], "-- / --");
            lv_obj_add_flag(forecast_icons[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void create_settings_menu()
{
    lv_obj_t *screen = create_screen(WatchScreen::Settings, "Settings");
    lv_obj_t *wifi = app_tile(screen, "WI-FI", LV_SYMBOL_WIFI, -design::TileOffset, design::TileFirstY, design::Wifi, open_wifi_cb);
    launcher_wifi_label = label(wifi, "Waiting", &lv_font_montserrat_14,
                                kMuted, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_t *bluetooth = app_tile(screen, "BLUETOOTH", LV_SYMBOL_BLUETOOTH, design::TileOffset, design::TileFirstY, design::Bluetooth, open_bluetooth_cb);
    launcher_bluetooth_label = label(bluetooth, "Disabled", &lv_font_montserrat_14,
                                      kMuted, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_t *brightness = app_tile(screen, "BRIGHTNESS", "", -design::TileOffset, design::TileSecondY, design::Brightness, open_brightness_cb);
    lv_obj_align(settings_icon(brightness, false, design::Brightness), LV_ALIGN_TOP_MID, 0, 22);
    settings_brightness_value = label(brightness, "", design::caption(), design::Brightness, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_t *timeout = app_tile(screen, "TIMEOUT", "", design::TileOffset, design::TileSecondY, design::Timeout, open_timeout_cb);
    lv_obj_align(settings_icon(timeout, true, design::Timeout), LV_ALIGN_TOP_MID, 0, 22);
    settings_timeout_value = label(timeout, "", design::caption(), design::Timeout, LV_ALIGN_BOTTOM_MID, 0, -16);
    refresh_settings_values();
}

static void create_bluetooth_screen()
{
    lv_obj_t *screen = create_screen(WatchScreen::Bluetooth, "Bluetooth");
    label(screen, LV_SYMBOL_BLUETOOTH, &lv_font_montserrat_26, design::Bluetooth,
          LV_ALIGN_TOP_MID, 0, 130);
    bluetooth_detail_label = label(screen, "Disabled", &lv_font_montserrat_26, kMuted,
                                   LV_ALIGN_TOP_MID, 0, 192);
    security_label = label(screen, "No paired phone", &lv_font_montserrat_16, kMuted,
          LV_ALIGN_TOP_MID, 0, 236);
    lv_obj_set_width(security_label,design::ContentWidth);
    lv_obj_set_style_text_align(security_label,LV_TEXT_ALIGN_CENTER,0);
    bluetooth_action = button(screen, "Enable Bluetooth", design::ContentWidth, design::TouchHeight, LV_ALIGN_TOP_MID, 0, 286,
                              bluetooth_action_cb);
    bluetooth_action_label = lv_obj_get_child(bluetooth_action, 0);
    lv_obj_add_state(bluetooth_action, LV_STATE_DISABLED);
    forget_phone_button = button(screen, "Forget paired phone", design::ContentWidth, design::TouchHeight, LV_ALIGN_TOP_MID, 0, 350,
        [](lv_event_t *) { navigate_to(WatchScreen::ForgetPhone); });
    lv_obj_add_state(forget_phone_button, LV_STATE_DISABLED);
}

static void create_activity_screen()
{
    lv_obj_t *screen = create_screen(WatchScreen::Activity, "Activity");
    label(screen, "TODAY / STEPS", design::caption(), kMuted,
          LV_ALIGN_TOP_LEFT, design::Margin, 78);
    activity_steps_label = label(screen, "0", design::number(), design::Activity,
                                 LV_ALIGN_TOP_LEFT, design::Margin, design::ActivityCountY);
    goal_minus = button(screen, "-", 56, 56, LV_ALIGN_TOP_LEFT, design::Margin, 150, step_goal_changed_cb);
    activity_goal_label = label(screen, "Goal 8,000", design::caption(), kMuted, LV_ALIGN_TOP_MID, 0, 174);
    goal_plus = button(screen, "+", 56, 56, LV_ALIGN_TOP_RIGHT, -design::Margin, 150, step_goal_changed_cb);
    label(screen, "7 DAYS / LOCAL", design::caption(), kMuted,
          LV_ALIGN_TOP_LEFT, design::Margin, 218);
    for (unsigned i = 0; i < 7; ++i) {
        const int y = 242 + i * 24;
        history_names[i] = label(screen, "--", &lv_font_montserrat_18, i ? kMuted : kWhite,
                                 LV_ALIGN_TOP_LEFT, 44, y);
        history_counts[i] = label(screen, "0", &lv_font_montserrat_20, i ? kWhite : design::Activity,
                                   LV_ALIGN_TOP_RIGHT, -44, y);
    }
    refresh_step_goal();
}

static void create_wifi_screen()
{
    lv_obj_t *screen = create_screen(WatchScreen::Wifi, "Wi-Fi");
    label(screen, LV_SYMBOL_WIFI, &lv_font_montserrat_26, design::Wifi,
          LV_ALIGN_TOP_MID, 0, 130);
    wifi_detail_label = label(screen, "Waiting for status", &lv_font_montserrat_26,
                              kMuted, LV_ALIGN_TOP_MID, 0, 192);
    wifi_name_label = label(screen, "", &lv_font_montserrat_18, kWhite,
                            LV_ALIGN_TOP_MID, 0, 240);
    lv_obj_set_width(wifi_name_label, 300);
    lv_label_set_long_mode(wifi_name_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(wifi_name_label, LV_TEXT_ALIGN_CENTER, 0);
    wifi_signal_label = label(screen, "", &lv_font_montserrat_20, kMuted,
                               LV_ALIGN_TOP_MID, 0, 272);
    wifi_action = button(screen, "Connect", design::ContentWidth, 64, LV_ALIGN_TOP_MID, 0, 314, wifi_action_cb);
    wifi_action_label = lv_obj_get_child(wifi_action, 0);
    lv_obj_add_state(wifi_action, LV_STATE_DISABLED);
}

static void brightness_changed_cb(lv_event_t *event)
{
    current_brightness = static_cast<uint8_t>(lv_slider_get_value(
        static_cast<lv_obj_t *>(lv_event_get_target(event))));
    lv_label_set_text_fmt(brightness_label, "%u%%", current_brightness);
    if (brightness_callback != nullptr) brightness_callback(current_brightness);
    refresh_settings_values();
}

static void brightness_released_cb(lv_event_t *)
{
    if (brightness_committed_callback != nullptr) brightness_committed_callback(current_brightness);
}

static void timeout_changed_cb(lv_event_t *event)
{
    const uint32_t selected = lv_dropdown_get_selected(
        static_cast<lv_obj_t *>(lv_event_get_target(event)));
    display_timeout_ms = selected < 5 ? kTimeouts[selected] : 10000;
    if (timeout_callback != nullptr) timeout_callback(display_timeout_ms);
    refresh_settings_values();
}

static void settings_icon_draw_cb(lv_event_t *event)
{
    lv_area_t area;
    lv_obj_get_coords(static_cast<lv_obj_t *>(lv_event_get_target(event)), &area);
    lv_layer_t *layer = lv_event_get_layer(event);
    const bool stopwatch = lv_event_get_user_data(event) != nullptr;
    const lv_color_t color = lv_obj_get_style_text_color(static_cast<lv_obj_t *>(lv_event_get_target(event)), LV_PART_MAIN);
    lv_draw_rect_dsc_t circle;
    lv_draw_rect_dsc_init(&circle);
    circle.bg_opa = LV_OPA_TRANSP;
    circle.border_color = color;
    circle.border_width = design::IconStroke;
    circle.radius = LV_RADIUS_CIRCLE;
    lv_area_t box = {area.x1 + 3, area.y1 + (stopwatch ? 5 : 1), area.x1 + 20, area.y1 + 19};
    lv_draw_rect(layer, &circle, &box);
    auto line = [&](int x1, int y1, int x2, int y2) {
        lv_draw_line_dsc_t dsc;
        lv_draw_line_dsc_init(&dsc);
        dsc.color = color;
        dsc.width = design::IconStroke;
        dsc.p1 = {static_cast<lv_value_precise_t>(area.x1 + x1), static_cast<lv_value_precise_t>(area.y1 + y1)};
        dsc.p2 = {static_cast<lv_value_precise_t>(area.x1 + x2), static_cast<lv_value_precise_t>(area.y1 + y2)};
        lv_draw_line(layer, &dsc);
    };
    if (stopwatch) {
        line(9, 1, 15, 1); line(12, 1, 12, 5);
        line(12, 8, 12, 13); line(12, 13, 16, 13); line(19, 3, 21, 5);
    } else {
        line(8, 19, 8, 22); line(16, 19, 16, 22);
        line(8, 22, 16, 22); line(10, 24, 14, 24);
        line(12, 12, 12, 19);
    }
}

static lv_obj_t *settings_icon(lv_obj_t *parent, bool stopwatch, uint32_t color)
{
    lv_obj_t *icon = lv_obj_create(parent);
    lv_obj_remove_style_all(icon);
    lv_obj_set_size(icon, 26, 26);
    lv_obj_set_style_text_color(icon, lv_color_hex(color), 0);
    lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(icon, LV_OBJ_FLAG_SCROLLABLE);
    static bool timer_icon = true;
    lv_obj_add_event_cb(icon, settings_icon_draw_cb, LV_EVENT_DRAW_MAIN, stopwatch ? &timer_icon : nullptr);
    return icon;
}

static void settings_heading(lv_obj_t *screen, const char *text, int y, bool stopwatch)
{
    lv_obj_t *icon = settings_icon(screen, stopwatch, stopwatch ? design::Timeout : design::Brightness);
    lv_obj_align(icon, LV_ALIGN_TOP_LEFT, design::Margin, y - 2);
    label(screen, text, design::body(), kWhite, LV_ALIGN_TOP_LEFT, design::Margin + 34, y);
}

static void refresh_settings_values()
{
    if (settings_brightness_value) lv_label_set_text_fmt(settings_brightness_value, "%u%%", current_brightness);
    if (settings_timeout_value) {
        if (display_timeout_ms == 0) lv_label_set_text(settings_timeout_value, "Never");
        else lv_label_set_text_fmt(settings_timeout_value, "%lu sec", static_cast<unsigned long>(display_timeout_ms / 1000));
    }
}

static void brightness_step_cb(lv_event_t *event)
{
    const int delta = lv_event_get_user_data(event) ? 5 : -5;
    int next = static_cast<int>(current_brightness) + delta;
    if (next < 10) next = 10;
    if (next > 100) next = 100;
    if (next == current_brightness) return;
    current_brightness = static_cast<uint8_t>(next);
    lv_slider_set_value(brightness_slider, next, LV_ANIM_OFF);
    lv_label_set_text_fmt(brightness_label, "%u%%", current_brightness);
    if (brightness_callback) brightness_callback(current_brightness);
    if (brightness_committed_callback) brightness_committed_callback(current_brightness);
    refresh_settings_values();
}

static void create_settings_screen()
{
    lv_obj_t *screen = create_screen(WatchScreen::Brightness, "Brightness");
    label(screen, "DISPLAY", design::caption(), kMuted, LV_ALIGN_TOP_LEFT, design::Margin, 96);
    settings_heading(screen, "Brightness", 126, false);
    brightness_label = label(screen, "60%", &lv_font_montserrat_26, design::Brightness,
                             LV_ALIGN_TOP_MID, 0, 157);
    brightness_slider = lv_slider_create(screen);
    lv_obj_set_size(brightness_slider, 186, 16);
    lv_obj_align(brightness_slider, LV_ALIGN_TOP_MID, 0, 213);
    lv_obj_set_ext_click_area(brightness_slider, 20); // 56px-high touch region, no overlap with +/-.
    lv_obj_remove_flag(brightness_slider,LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_t *minus = button(screen, "-", 48, 56, LV_ALIGN_TOP_LEFT, design::Margin, 193, nullptr);
    lv_obj_t *plus = button(screen, "+", 48, 56, LV_ALIGN_TOP_RIGHT, -design::Margin, 193, nullptr);
    static bool increment = true;
    lv_obj_add_event_cb(minus, brightness_step_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(plus, brightness_step_cb, LV_EVENT_CLICKED, &increment);
    lv_slider_set_range(brightness_slider, 10, 100);
    lv_slider_set_value(brightness_slider, current_brightness, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(kSurface), LV_PART_MAIN);
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(kAccent), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(kWhite), LV_PART_KNOB);
    lv_obj_add_event_cb(brightness_slider, brightness_changed_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(brightness_slider, brightness_released_cb, LV_EVENT_RELEASED, nullptr);
    label(screen, "5% PER TAP / 10-100%", design::caption(), kMuted, LV_ALIGN_TOP_MID, 0, 290);
    screen = create_screen(WatchScreen::Timeout, "Screen timeout");
    label(screen, "SCREEN", design::caption(), kMuted, LV_ALIGN_TOP_LEFT, design::Margin, 110);
    settings_heading(screen, "Timeout", 142, true);
    screen_timeout_dropdown = lv_dropdown_create(screen);
    lv_dropdown_set_options(screen_timeout_dropdown,
                            "5 seconds\n10 seconds\n30 seconds\n60 seconds\nNever");
    lv_dropdown_set_selected(screen_timeout_dropdown, 1);
    lv_obj_set_size(screen_timeout_dropdown, 320, 60);
    lv_obj_align(screen_timeout_dropdown, LV_ALIGN_TOP_MID, 0, 206);
    style_surface(screen_timeout_dropdown);
    style_press_feedback(screen_timeout_dropdown);
    lv_obj_set_style_pad_all(screen_timeout_dropdown, 18, 0);
    lv_obj_set_style_text_font(screen_timeout_dropdown, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(screen_timeout_dropdown, lv_color_hex(kWhite), 0);
    lv_obj_t *list = lv_dropdown_get_list(screen_timeout_dropdown);
    lv_obj_set_style_bg_color(list, lv_color_hex(kSurface), 0);
    lv_obj_set_style_text_color(list, lv_color_hex(kWhite), 0);
    lv_obj_set_style_text_font(list, &lv_font_montserrat_20, 0);
    lv_obj_set_style_pad_ver(list, 12, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(list, lv_color_hex(design::Pressed), LV_PART_SELECTED);
    lv_obj_add_event_cb(screen_timeout_dropdown, timeout_changed_cb, LV_EVENT_VALUE_CHANGED, nullptr);
}

static void format_elapsed(char *text, unsigned size, uint64_t ms, bool fractions)
{
    const uint64_t seconds = ms / 1000;
    if (fractions) snprintf(text, size, "%02llu:%02llu:%02llu.%01llu",
        (unsigned long long)(seconds / 3600), (unsigned long long)(seconds / 60 % 60),
        (unsigned long long)(seconds % 60), (unsigned long long)(ms / 100 % 10));
    else snprintf(text, size, "%02llu:%02llu:%02llu", (unsigned long long)(seconds / 3600),
        (unsigned long long)(seconds / 60 % 60), (unsigned long long)(seconds % 60));
}

static void refresh_time_tools()
{
    if (!display_active) return;
    char text[40];
    if (current_screen == WatchScreen::Timer && timer_value) {
        const uint64_t duration = pomodoro_duration();
        const bool finished = countdown.elapsed_ms >= duration;
        const uint64_t remaining = finished ? 0 : duration - countdown.elapsed_ms;
        const uint64_t seconds=(remaining+999)/1000;
        snprintf(text,sizeof(text),"%02llu:%02llu",(unsigned long long)(seconds/60),(unsigned long long)(seconds%60));
        lv_label_set_text(timer_value, text);
        lv_arc_set_value(pomodoro_progress,static_cast<int32_t>((duration-remaining)*1000/duration));
        lv_obj_set_style_arc_color(pomodoro_progress,lv_color_hex(pomodoro_break ? 0x86C978 : 0xEE594E),LV_PART_INDICATOR);
        const uint32_t accent=pomodoro_break ? 0x86C978 : 0xEE594E;
        lv_label_set_text(pomodoro_phase,pomodoro_break ? (pomodoro_completed==4 ? "LONG BREAK" : "SHORT BREAK") : "FOCUS TIME");
        lv_obj_set_style_text_color(pomodoro_phase,lv_color_hex(accent),0);
        lv_obj_set_style_bg_color(pomodoro_start,lv_color_hex(accent),0);
        snprintf(text,sizeof(text),"%s",finished ? "DONE / TAKE A BREATH" : countdown.running ? (pomodoro_break ? "REST AND RECHARGE" : "ONE THING AT A TIME") : countdown.elapsed_ms ? "PAUSED" : "READY WHEN YOU ARE");
        lv_label_set_text(timer_status,text);
        for (unsigned i=0;i<4;++i) {
            lv_obj_set_style_bg_color(pomodoro_sessions[i],lv_color_hex(i<pomodoro_completed ? 0xEE594E : design::Border),0);
            lv_obj_set_style_border_color(pomodoro_sessions[i],lv_color_hex(!pomodoro_break && i==pomodoro_completed ? 0xEE594E : design::Border),0);
        }
        lv_label_set_text(timer_action_label, finished ? "START NEXT" : countdown.running ? "PAUSE" : countdown.elapsed_ms ? "RESUME" : "START");
    }
    if (current_screen == WatchScreen::Stopwatch && stopwatch_value) {
        format_elapsed(text, sizeof(text), stopwatch.elapsed_ms, true);
        lv_label_set_text(stopwatch_value, text);
        lv_label_set_text(stopwatch_status, stopwatch.running ? "RUNNING" : stopwatch.elapsed_ms ? "PAUSED" : "READY");
        lv_label_set_text(stopwatch_action_label, stopwatch.running ? "PAUSE" : stopwatch.elapsed_ms ? "RESUME" : "START");
    }
}

static void time_tools_tick(lv_timer_t *)
{
    const uint32_t now = lv_tick_get();
    countdown.advance(now); stopwatch.advance(now);
    const uint64_t duration = pomodoro_duration();
    const bool finished_now = countdown.running && countdown.elapsed_ms >= duration;
    if (countdown.elapsed_ms >= duration) { countdown.elapsed_ms = duration; countdown.running = false; }
    static uint64_t last_timer_second = UINT64_MAX;
    const uint64_t timer_second = countdown.elapsed_ms / 1000;
    if (display_active && ((current_screen == WatchScreen::Stopwatch && stopwatch.running) ||
        (current_screen == WatchScreen::Timer && (finished_now || last_timer_second != timer_second)))) refresh_time_tools();
    last_timer_second = timer_second;
}

static void timer_control_cb(lv_event_t *)
{
    const uint32_t now = lv_tick_get();
    countdown.advance(now);
    if (countdown.elapsed_ms >= pomodoro_duration()) pomodoro_next_cb(nullptr);
    countdown.toggle(now); refresh_time_tools();
}
static void timer_reset_cb(lv_event_t *) { pomodoro_break=false; pomodoro_completed=0; countdown.reset(lv_tick_get()); refresh_time_tools(); }
static void pomodoro_next_cb(lv_event_t *)
{
    if (pomodoro_break) { pomodoro_break=false; if (pomodoro_completed==4) pomodoro_completed=0; }
    else { pomodoro_break=true; ++pomodoro_completed; }
    countdown.reset(lv_tick_get());
    refresh_time_tools();
}
static void stopwatch_control_cb(lv_event_t *) { stopwatch.toggle(lv_tick_get()); refresh_time_tools(); }
static void stopwatch_reset_cb(lv_event_t *) { stopwatch.reset(lv_tick_get()); refresh_time_tools(); }

static void create_time_tools()
{
    lv_obj_t *screen = create_screen(WatchScreen::Timer, "Pomodoro");
    pomodoro_progress=lv_arc_create(screen);
    lv_obj_set_size(pomodoro_progress,216,216);
    lv_obj_align(pomodoro_progress,LV_ALIGN_TOP_MID,0,72);
    lv_arc_set_rotation(pomodoro_progress,270);
    lv_arc_set_bg_angles(pomodoro_progress,0,360);
    lv_arc_set_range(pomodoro_progress,0,1000);
    lv_arc_set_value(pomodoro_progress,0);
    lv_obj_remove_style(pomodoro_progress,nullptr,LV_PART_KNOB);
    lv_obj_remove_flag(pomodoro_progress,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(pomodoro_progress,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_arc_width(pomodoro_progress,10,LV_PART_MAIN);
    lv_obj_set_style_arc_width(pomodoro_progress,10,LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(pomodoro_progress,lv_color_hex(design::Border),LV_PART_MAIN);
    lv_obj_set_style_arc_color(pomodoro_progress,lv_color_hex(0xEE594E),LV_PART_INDICATOR);
    tomato_icon(screen,LV_ALIGN_TOP_MID,0,92);
    pomodoro_phase=label(screen,"FOCUS TIME",design::caption(),0xEE594E,LV_ALIGN_TOP_MID,0,139);
    timer_value = label(screen, "25:00", &lv_font_montserrat_48, kWhite, LV_ALIGN_TOP_MID, 0, 164);
    timer_status = label(screen, "READY WHEN YOU ARE", design::caption(), kMuted, LV_ALIGN_TOP_MID, 0, 224);
    for (unsigned i=0;i<4;++i) {
        auto *dot=pomodoro_sessions[i]=lv_obj_create(screen);
        lv_obj_remove_style_all(dot); lv_obj_set_size(dot,10,10);
        lv_obj_align(dot,LV_ALIGN_TOP_MID,int(i)*20-30,258);
        lv_obj_set_style_radius(dot,LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_bg_color(dot,lv_color_hex(design::Border),0);
        lv_obj_set_style_bg_opa(dot,LV_OPA_COVER,0);
        lv_obj_set_style_border_width(dot,2,0);
        lv_obj_set_style_border_color(dot,lv_color_hex(i==0 ? 0xEE594E : design::Border),0);
        lv_obj_remove_flag(dot,LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_t *action = pomodoro_start = button(screen, "START", 304, 56, LV_ALIGN_TOP_MID, 0, 294, timer_control_cb);
    lv_obj_set_style_bg_color(action,lv_color_hex(0xEE594E),0);
    lv_obj_set_style_bg_opa(action,LV_OPA_COVER,0);
    lv_obj_set_style_text_color(lv_obj_get_child(action,0),lv_color_hex(0x101010),0);
    timer_action_label = lv_obj_get_child(action, 0);
    button(screen, "RESET", 144, 56, LV_ALIGN_TOP_MID, -80, 358, timer_reset_cb);
    button(screen,"SKIP PHASE",144,56,LV_ALIGN_TOP_MID,80,358,pomodoro_next_cb);

    screen = create_screen(WatchScreen::Stopwatch, "Stopwatch");
    stopwatch_status = label(screen, "READY", design::caption(), kMuted, LV_ALIGN_TOP_MID, 0, 116);
    stopwatch_value = label(screen, "00:00:00.0", &lv_font_montserrat_26, kWhite, LV_ALIGN_TOP_MID, 0, 182);
    action = button(screen, "START", 148, 56, LV_ALIGN_TOP_MID, -84, 290, stopwatch_control_cb);
    stopwatch_action_label = lv_obj_get_child(action, 0);
    button(screen, "RESET", 148, 56, LV_ALIGN_TOP_MID, 84, 290, stopwatch_reset_cb);
    label(screen, "CONTINUES WHILE SCREEN IS OFF", design::caption(), kMuted, LV_ALIGN_TOP_MID, 0, 378);
    countdown.reset(lv_tick_get()); stopwatch.reset(lv_tick_get());
    lv_timer_create(time_tools_tick, 100, nullptr);
}

static void create_notification_screen()
{
    lv_obj_t *screen = create_screen(WatchScreen::Notification, "Notification");
    notification_app = label(screen, "PHONE", design::body(), kMuted, LV_ALIGN_TOP_LEFT, design::Margin, 82);
    lv_obj_set_width(notification_app, design::ContentWidth);
    lv_label_set_long_mode(notification_app, LV_LABEL_LONG_DOT);
    lv_obj_t *content = lv_obj_create(screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, design::ContentWidth, 298);
    lv_obj_align(content, LV_ALIGN_TOP_LEFT, design::Margin, 114);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content, 16, 0);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    notification_title = label(content, "", design::title(), kWhite, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_width(notification_title, design::ContentWidth);
    lv_label_set_long_mode(notification_title, LV_LABEL_LONG_WRAP);
    notification_body = label(content, "", design::body(), kWhite, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_width(notification_body, design::ContentWidth);
    lv_label_set_long_mode(notification_body, LV_LABEL_LONG_WRAP);
}
static void create_security_screens()
{
    auto *screen = create_screen(WatchScreen::Pairing, "Pair phone");
    auto *info = label(screen, "Compare this code with the phone.\nAccept only if both codes match.", design::body(), kWhite, LV_ALIGN_TOP_MID, 0, 110);
    lv_obj_set_width(info, design::ContentWidth); lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
    pairing_code = label(screen, "000000", &lv_font_montserrat_48, design::Bluetooth, LV_ALIGN_TOP_MID, 0, 210);
    auto respond = [](lv_event_t *event) {
        if (pairing_callback && pairing_callback(pairing_token, lv_event_get_target(event) == pairing_accept)) {
            lv_obj_add_state(pairing_accept, LV_STATE_DISABLED); lv_obj_add_state(pairing_reject, LV_STATE_DISABLED);
        }
    };
    pairing_accept = button(screen, "ACCEPT", 148, 64, LV_ALIGN_TOP_MID, -84, 320, respond);
    pairing_reject = button(screen, "REJECT", 148, 64, LV_ALIGN_TOP_MID, 84, 320, respond);
    screen = create_screen(WatchScreen::ForgetPhone, "Forget phone?");
    info = label(screen, "Remove the trusted phone and keys?\n\nAlso forget ESP32 Smartwatch in\nAndroid Bluetooth settings before\npairing again.", design::body(), kWhite, LV_ALIGN_TOP_MID, 0, 114);
    lv_obj_set_width(info, design::ContentWidth); lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
    button(screen, "FORGET", 148, 64, LV_ALIGN_TOP_MID, -84, 320, [](lv_event_t *) {
        if (forget_callback && forget_callback()) navigate_to(WatchScreen::Bluetooth);
    });
    button(screen, "CANCEL", 148, 64, LV_ALIGN_TOP_MID, 84, 320, [](lv_event_t *) { navigate_to(WatchScreen::Bluetooth); });
}
static void rotate_media_record(void *object, int32_t angle)
{
    lv_arc_set_rotation(static_cast<lv_obj_t *>(object), static_cast<uint16_t>(angle));
}
static void show_game_dialog(GameDialog dialog)
{
    game_dialog = dialog;
    if (dialog == GameDialog::None) { lv_obj_add_flag(game_overlay, LV_OBJ_FLAG_HIDDEN); return; }
    const bool confirm = dialog == GameDialog::NewGame, win = dialog == GameDialog::Win;
    lv_label_set_text(game_dialog_title, confirm ? "NEW GAME?" : win ? "2048!" : "GAME OVER");
    if (confirm) lv_label_set_text(game_dialog_body, "Current progress will be lost.\nBest score will be kept.");
    else lv_label_set_text_fmt(game_dialog_body, "Score: %lu\nBest: %lu", static_cast<unsigned long>(game_2048.score),
                               static_cast<unsigned long>(game_2048.best));
    lv_label_set_text(lv_obj_get_child(game_dialog_left, 0), confirm ? "CANCEL" : win ? "CONTINUE" : "NEW GAME");
    lv_label_set_text(lv_obj_get_child(game_dialog_right, 0), dialog == GameDialog::Over ? "BACK" : "NEW GAME");
    lv_obj_remove_flag(game_overlay, LV_OBJ_FLAG_HIDDEN);
}
static void render_game()
{
    if (!game_score) return;
    lv_label_set_text_fmt(game_score, "SCORE %lu", static_cast<unsigned long>(game_2048.score));
    lv_label_set_text_fmt(game_best, "BEST %lu%s", static_cast<unsigned long>(game_2048.best), game_save_failed ? " *" : "");
    const uint32_t colors[] = {0xBABABA, 0xFFE59A, 0xFFBE75, 0xFFAA55, 0xFF887A, 0xFF6655,
                              0xFFD166, 0x74DCC8, 0x68D9EF, 0x67A9FF, 0xB8A1FF};
    for (unsigned i = 0; i < 16; ++i) {
        const uint32_t tile = game_2048.board[i];
        unsigned shade = 0; for (uint32_t n = tile; n > 2 && shade < 10; n >>= 1) ++shade;
        if (tile) lv_label_set_text_fmt(game_values[i], "%lu", static_cast<unsigned long>(tile));
        else lv_label_set_text(game_values[i], "");
        const lv_font_t *font = tile <= 64 ? design::title() : tile <= 1024 ? &lv_font_montserrat_22 :
                                tile <= 65536 ? design::body() : design::caption();
        lv_obj_set_style_text_font(game_values[i], font, 0);
        lv_obj_set_style_text_color(game_values[i], lv_color_hex(colors[shade]), 0);
        lv_obj_set_style_border_color(game_tiles[i], lv_color_hex(tile ? colors[shade] : design::Border), 0);
        lv_obj_set_style_bg_color(game_tiles[i], lv_color_hex(tile ? 0x151515 : 0x080808), 0);
    }
    if (game_dialog == GameDialog::NewGame) return;
    if (game_2048.win_pending) show_game_dialog(GameDialog::Win);
    else if (game_2048.game_over) show_game_dialog(GameDialog::Over);
    else show_game_dialog(GameDialog::None);
}
static void game_swipe(lv_dir_t direction)
{
    if (!display_active || game_dialog != GameDialog::None) return;
    Watch2048Direction move;
    switch (direction) {
    case LV_DIR_LEFT: move = Watch2048Direction::Left; break;
    case LV_DIR_RIGHT: move = Watch2048Direction::Right; break;
    case LV_DIR_TOP: move = Watch2048Direction::Up; break;
    case LV_DIR_BOTTOM: move = Watch2048Direction::Down; break;
    default: return;
    }
    const uint32_t best = game_2048.best;
    game_2048.move(move);
    if (game_2048.best != best) save_game_best(false);
    if (game_2048.game_over || game_2048.win_pending) save_game_best(true);
    render_game();
}
static void game_reset()
{
    save_game_best(true);
    game_2048.new_game();
    show_game_dialog(GameDialog::None);
    render_game();
}
static void game_new_cb(lv_event_t *)
{
    if (game_2048.moves && !game_2048.game_over) show_game_dialog(GameDialog::NewGame);
    else game_reset();
}
static void game_dialog_left_cb(lv_event_t *)
{
    if (game_dialog == GameDialog::NewGame) { show_game_dialog(GameDialog::None); render_game(); }
    else if (game_dialog == GameDialog::Win) { game_2048.continue_game(); show_game_dialog(GameDialog::None); render_game(); }
    else if (game_dialog == GameDialog::Over) game_reset();
}
static void game_dialog_right_cb(lv_event_t *)
{
    if (game_dialog == GameDialog::Over) back_cb(nullptr);
    else game_reset();
}
static void voice_hold_cb(lv_event_t *event)
{
    const auto code = lv_event_get_code(event);
    if (code == LV_EVENT_PRESSED && display_active) {
        voice_held = voice_callback && voice_callback(1);
        if (!voice_held) watch_ui_set_voice(5,0,0,voice_callback ? "Microphone busy or unavailable" : "Simulator: microphone unavailable");
    } else if (code == LV_EVENT_RELEASED && voice_held) {
        voice_held = false; if (voice_callback) voice_callback(2);
    } else if (code == LV_EVENT_PRESS_LOST) cancel_voice();
}
static void voice_cancel_cb(lv_event_t *) { cancel_voice(); }
static void microphone_draw_cb(lv_event_t *event)
{
    lv_area_t area; lv_obj_get_coords(static_cast<lv_obj_t *>(lv_event_get_target(event)),&area);
    auto *layer = lv_event_get_layer(event);
    lv_draw_rect_dsc_t capsule; lv_draw_rect_dsc_init(&capsule);
    capsule.bg_opa = LV_OPA_TRANSP; capsule.border_color = lv_color_hex(kWhite);
    capsule.border_width = 2; capsule.radius = 5;
    lv_area_t body = {area.x1+9,area.y1+2,area.x1+19,area.y1+20};
    lv_draw_rect(layer,&capsule,&body);
    auto line = [&](int x1,int y1,int x2,int y2) {
        lv_draw_line_dsc_t stroke; lv_draw_line_dsc_init(&stroke);
        stroke.color = lv_color_hex(kWhite); stroke.width = 2; stroke.round_start = 1; stroke.round_end = 1;
        stroke.p1 = {static_cast<lv_value_precise_t>(area.x1+x1),static_cast<lv_value_precise_t>(area.y1+y1)};
        stroke.p2 = {static_cast<lv_value_precise_t>(area.x1+x2),static_cast<lv_value_precise_t>(area.y1+y2)};
        lv_draw_line(layer,&stroke);
    };
    line(5,15,5,21); line(5,21,9,25); line(9,25,19,25);
    line(19,25,23,21); line(23,21,23,15); line(14,25,14,31); line(9,31,19,31);
}

static void refresh_voice_face()
{
    if (!voice_face_timer) return;
    if (display_active && current_screen == WatchScreen::Voice && voice_state != 7 && voice_state != 8 && !lv_obj_has_flag(voice_face,LV_OBJ_FLAG_HIDDEN))
        lv_timer_resume(voice_face_timer);
    else lv_timer_pause(voice_face_timer);
}

static void voice_face_draw(lv_event_t *event)
{
    lv_area_t area; lv_obj_get_coords(voice_face,&area);
    auto *layer = lv_event_get_layer(event);
    const int cx = (area.x1+area.x2)/2, cy = (area.y1+area.y2)/2;
    static const lv_image_dsc_t halo = [] {
        lv_image_dsc_t image = {};
        image.header.magic = LV_IMAGE_HEADER_MAGIC;
        image.header.cf = LV_COLOR_FORMAT_RGB565;
        image.header.w = image.header.h = mochi_halo_size;
        image.header.stride = mochi_halo_size*2;
        image.data_size = sizeof(mochi_halo_pixels);
        image.data = mochi_halo_pixels;
        return image;
    }();
    lv_draw_image_dsc_t image; lv_draw_image_dsc_init(&image);
    image.src = &halo;
    lv_draw_image(layer,&image,&area);
    const bool blink = voice_state == 7 || (voice_face_blink && voice_state != 2);
    for (int eye = 0; eye < 2; ++eye) {
        if (voice_state == 8) {
            const int x=cx+(eye ? 17 : -17);
            const int points[][2]={{-9,0},{-6,-5},{-3,-8},{0,-9},{3,-8},{6,-5},{9,0}};
            for (unsigned segment=0;segment<6;++segment) {
                lv_draw_line_dsc_t line; lv_draw_line_dsc_init(&line);
                line.color=lv_color_hex(kWhite); line.width=3; line.round_start=line.round_end=1;
                line.p1={static_cast<lv_value_precise_t>(x+points[segment][0]),static_cast<lv_value_precise_t>(cy+points[segment][1])};
                line.p2={static_cast<lv_value_precise_t>(x+points[segment+1][0]),static_cast<lv_value_precise_t>(cy+points[segment+1][1])};
                lv_draw_line(layer,&line);
            }
            continue;
        }
        if (voice_state == 7) {
            // Resting eyelids: raised ends and a gently flattened curved base.
            // Fits inside the existing eye-only invalidation rectangle.
            const int x=cx+(eye ? 17 : -17);
            const int points[][2]={{-10,-8},{-7,-5},{-4,-4},{4,-4},{7,-5},{10,-8}};
            for (unsigned segment=0;segment<5;++segment) {
                lv_draw_line_dsc_t line; lv_draw_line_dsc_init(&line);
                line.color=lv_color_hex(kWhite); line.width=3;
                line.round_start=line.round_end=1;
                line.p1={static_cast<lv_value_precise_t>(x+points[segment][0]),static_cast<lv_value_precise_t>(cy+points[segment][1])};
                line.p2={static_cast<lv_value_precise_t>(x+points[segment+1][0]),static_cast<lv_value_precise_t>(cy+points[segment+1][1])};
                lv_draw_line(layer,&line);
            }
            continue;
        }
        lv_draw_rect_dsc_t dsc; lv_draw_rect_dsc_init(&dsc);
        dsc.bg_color = lv_color_hex(kWhite); dsc.bg_opa = LV_OPA_COVER;
        dsc.radius = LV_RADIUS_CIRCLE;
        const int x = cx + (eye ? 17 : -17), height = blink ? 3 : 29 + (voice_state == 2 ? voice_face_level/20 : 0);
        lv_area_t bounds = {x-9,cy-height/2-4,x+9,cy+height/2-4};
        lv_draw_rect(layer,&dsc,&bounds);
    }
}

static void voice_face_tick(lv_timer_t *)
{
    if (!display_active || current_screen != WatchScreen::Voice || voice_state == 7 || voice_state == 8 || lv_obj_has_flag(voice_face,LV_OBJ_FLAG_HIDDEN)) return;
    const bool blink = lv_tick_get()%3000 < 160;
    if (blink != voice_face_blink) {
        voice_face_blink = blink;
        lv_area_t eyes; lv_obj_get_coords(voice_face,&eyes);
        const int cx = (eyes.x1+eyes.x2)/2, cy = (eyes.y1+eyes.y2)/2;
        eyes = {cx-28,cy-24,cx+28,cy+24};
        lv_obj_invalidate_area(voice_face,&eyes); // Blink does not invalidate the outer halo.
    }
}

static void voice_happy_draw(lv_event_t *event)
{
    lv_area_t area; lv_obj_get_coords(static_cast<lv_obj_t *>(lv_event_get_target(event)),&area);
    auto *layer=lv_event_get_layer(event);
    const int cx=(area.x1+area.x2)/2,cy=(area.y1+area.y2)/2;
    const int size=lv_obj_get_width(static_cast<lv_obj_t *>(lv_event_get_target(event)));
    for (int glow=3;glow>=0;--glow) {
        lv_draw_arc_dsc_t ring; lv_draw_arc_dsc_init(&ring);
        ring.center={cx,cy}; ring.radius=(23+glow*2)*size/64; ring.width=glow ? 1 : 2;
        ring.start_angle=0; ring.end_angle=360; ring.color=lv_color_hex(lv_event_get_user_data(event) ? kWhite : 0xC4A7FF);
        ring.opa=glow ? static_cast<lv_opa_t>(32/glow) : LV_OPA_COVER;
        lv_draw_arc(layer,&ring);
    }
    for (int eye=0;eye<2;++eye) {
        const int x=cx+(eye ? 8 : -8)*size/64;
        if (lv_event_get_user_data(event)) {
            lv_draw_rect_dsc_t pupil; lv_draw_rect_dsc_init(&pupil);
            pupil.bg_color=lv_color_hex(kWhite); pupil.bg_opa=LV_OPA_COVER;
            pupil.radius=LV_RADIUS_CIRCLE;
            const int half_width=(size+10)/20,half_height=size/10;
            lv_area_t bounds={x-half_width,cy-half_height,x+half_width-1,cy+half_height-1};
            lv_draw_rect(layer,&pupil,&bounds);
            continue;
        }
        const int points[][2]={{-4,1},{-2,-2},{0,-3},{2,-2},{4,1}};
        for (unsigned segment=0;segment<4;++segment) {
            lv_draw_line_dsc_t line; lv_draw_line_dsc_init(&line);
            line.color=lv_color_hex(kWhite); line.width=2; line.round_start=line.round_end=1;
            line.p1={static_cast<lv_value_precise_t>(x+points[segment][0]*size/64),static_cast<lv_value_precise_t>(cy+points[segment][1]*size/64)};
            line.p2={static_cast<lv_value_precise_t>(x+points[segment+1][0]*size/64),static_cast<lv_value_precise_t>(cy+points[segment+1][1]*size/64)};
            lv_draw_line(layer,&line);
        }
    }
}

static void create_voice_screen()
{
    auto *screen = create_screen(WatchScreen::Voice,"AI Assistant");
    voice_name = label(screen,"Mochi",design::body(),kWhite,LV_ALIGN_TOP_MID,0,66);
    voice_face = lv_obj_create(screen); lv_obj_remove_style_all(voice_face);
    lv_obj_set_size(voice_face,mochi_halo_size,mochi_halo_size); lv_obj_align(voice_face,LV_ALIGN_TOP_MID,0,64);
    lv_obj_remove_flag(voice_face,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_add_event_cb(voice_face,voice_face_draw,LV_EVENT_DRAW_MAIN,nullptr);
    voice_face_timer = lv_timer_create(voice_face_tick,80,nullptr);
    lv_timer_pause(voice_face_timer);
    voice_status = label(screen,"",design::body(),kWhite,LV_ALIGN_TOP_MID,0,84);
    voice_happy=lv_obj_create(screen); lv_obj_remove_style_all(voice_happy);
    lv_obj_set_size(voice_happy,64,64); lv_obj_align(voice_happy,LV_ALIGN_TOP_MID,0,68);
    lv_obj_remove_flag(voice_happy,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_add_event_cb(voice_happy,voice_happy_draw,LV_EVENT_DRAW_MAIN,nullptr);
    lv_obj_add_flag(voice_happy,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(voice_status,design::ContentWidth);
    lv_obj_set_style_text_align(voice_status,LV_TEXT_ALIGN_CENTER,0);
    lv_label_set_long_mode(voice_status,LV_LABEL_LONG_WRAP);
    auto *note = voice_note = label(screen,"",design::body(),kMuted,LV_ALIGN_TOP_MID,0,136);
    lv_obj_set_width(note,design::ContentWidth); lv_obj_set_style_text_align(note,LV_TEXT_ALIGN_CENTER,0);
    voice_details = label(screen,"Max 8 seconds",design::caption(),kMuted,LV_ALIGN_TOP_MID,0,190);
    lv_obj_set_width(voice_details,design::ContentWidth); lv_obj_set_style_text_align(voice_details,LV_TEXT_ALIGN_CENTER,0);
    voice_level = lv_bar_create(screen); lv_obj_set_size(voice_level,design::ContentWidth,12);
    lv_obj_align(voice_level,LV_ALIGN_TOP_MID,0,224); lv_bar_set_range(voice_level,0,100);
    lv_obj_set_style_bg_color(voice_level,lv_color_hex(design::Border),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(voice_level,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_remove_flag(voice_level,LV_OBJ_FLAG_CLICKABLE);
    voice_hold = button(screen,"HOLD TO TALK",design::ContentWidth,96,LV_ALIGN_TOP_MID,0,246,nullptr);
    lv_obj_set_style_bg_color(voice_hold,lv_color_hex(0xC4A7FF),0);
    lv_obj_set_style_bg_opa(voice_hold,LV_OPA_20,0);
    lv_obj_set_style_border_color(voice_hold,lv_color_hex(0x8B72B5),0);
    lv_obj_set_style_bg_color(voice_hold,lv_color_hex(0xC4A7FF),LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(voice_hold,LV_OPA_40,LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(voice_hold,LV_OPA_10,LV_STATE_DISABLED);
    lv_obj_align(lv_obj_get_child(voice_hold,0),LV_ALIGN_CENTER,18,0);
    voice_microphone = lv_obj_create(voice_hold); lv_obj_remove_style_all(voice_microphone);
    lv_obj_set_size(voice_microphone,28,34); lv_obj_align(voice_microphone,LV_ALIGN_CENTER,-100,0);
    lv_obj_remove_flag(voice_microphone,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(voice_microphone,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(voice_microphone,microphone_draw_cb,LV_EVENT_DRAW_MAIN,nullptr);
    lv_obj_add_event_cb(voice_hold,voice_hold_cb,LV_EVENT_ALL,nullptr);
    lv_obj_remove_flag(voice_hold,LV_OBJ_FLAG_GESTURE_BUBBLE);
    voice_clear = button(screen,"CLEAR",design::ContentWidth,56,LV_ALIGN_TOP_MID,0,354,voice_cancel_cb);
    auto *results = lv_obj_create(screen);
    lv_obj_set_size(results,design::ContentWidth,190); lv_obj_align(results,LV_ALIGN_TOP_MID,0,162);
    style_surface(results); lv_obj_set_style_pad_all(results,12,0);
    // Readable AMOLED card, without changing the rest of the monochrome theme.
    lv_obj_set_style_bg_color(results,lv_color_hex(0x202020),0);
    lv_obj_set_style_bg_opa(results,LV_OPA_COVER,0);
    lv_obj_set_style_border_color(results,lv_color_hex(0x707070),0);
    lv_obj_set_style_pad_row(results,10,0);
    lv_obj_set_flex_flow(results,LV_FLEX_FLOW_COLUMN); lv_obj_set_scroll_dir(results,LV_DIR_VER);
    lv_obj_remove_flag(results,LV_OBJ_FLAG_GESTURE_BUBBLE);
    // Handle horizontal back locally once. Vertical scrolling remains owned by
    // the card, without bubbling a second navigation event to the screen.
    lv_obj_add_event_cb(results,navigation_gesture_cb,LV_EVENT_GESTURE,nullptr);
    voice_question_heading = lv_label_create(results);
    lv_obj_set_style_text_font(voice_question_heading,design::body(),0);
    lv_obj_set_style_text_color(voice_question_heading,lv_color_hex(0xB8B8B8),0);
    lv_label_set_text(voice_question_heading,"QUESTION:");
    voice_transcription = lv_label_create(results); lv_obj_set_width(voice_transcription,LV_PCT(100));
    lv_obj_set_style_text_font(voice_transcription,design::body(),0);
    lv_obj_set_style_text_color(voice_transcription,lv_color_hex(0xE0E0E0),0);
    lv_label_set_long_mode(voice_transcription,LV_LABEL_LONG_WRAP);
    voice_answer_heading = lv_label_create(results);
    lv_obj_set_style_text_font(voice_answer_heading,design::body(),0);
    lv_obj_set_style_text_color(voice_answer_heading,lv_color_hex(0xB8B8B8),0);
    lv_label_set_text(voice_answer_heading,"ANSWER:");
    voice_answer = lv_label_create(results); lv_obj_set_width(voice_answer,LV_PCT(100));
    lv_label_set_long_mode(voice_answer,LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(voice_answer,design::body(),0);
    lv_obj_set_style_text_color(voice_answer,lv_color_hex(0xFFFFFF),0);
    lv_obj_set_style_text_line_space(voice_answer,4,0);
    lv_label_set_text(voice_transcription,""); lv_label_set_text(voice_answer,"");
    voice_waiting = label(results,"Your phone is preparing the answer.\nCANCEL or BACK stops the request.",design::body(),kMuted,LV_ALIGN_TOP_LEFT,0,0);
    lv_obj_set_width(voice_waiting,LV_PCT(100));
    lv_label_set_long_mode(voice_waiting,LV_LABEL_LONG_WRAP);
    lv_obj_add_flag(voice_waiting,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(results,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_user_data(voice_answer,results);
    // The expanded halo's black image bounds overlap the name; draw the name
    // last so the bitmap cannot cover its lower glyphs.
    lv_obj_move_foreground(voice_name);
    watch_ui_set_voice(0,0,0,nullptr);
}
static void create_game_screen()
{
    auto *screen = create_screen(WatchScreen::Game2048, nullptr);
    label(screen, "2048", design::title(), kWhite, LV_ALIGN_TOP_LEFT, design::Margin, 24);
    game_best = label(screen, "BEST 0", design::caption(), kMuted, LV_ALIGN_TOP_RIGHT, -design::Margin, 34);
    game_score = label(screen, "SCORE 0", design::body(), kWhite, LV_ALIGN_TOP_LEFT, design::Margin, 60);
    game_board = lv_obj_create(screen);
    lv_obj_remove_style_all(game_board);
    lv_obj_set_size(game_board, 306, 306);
    lv_obj_align(game_board, LV_ALIGN_TOP_MID, 0, 98);
    lv_obj_remove_flag(game_board, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(game_board, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_GESTURE_BUBBLE));
    for (unsigned i = 0; i < 16; ++i) {
        game_tiles[i] = lv_obj_create(game_board);
        style_surface(game_tiles[i]);
        lv_obj_set_size(game_tiles[i], 72, 72);
        lv_obj_set_pos(game_tiles[i], (i % 4) * 78, (i / 4) * 78);
        lv_obj_remove_flag(game_tiles[i], static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        lv_obj_add_flag(game_tiles[i], LV_OBJ_FLAG_GESTURE_BUBBLE);
        game_values[i] = label(game_tiles[i], "", design::title(), kWhite, LV_ALIGN_CENTER, 0, 0);
    }
    button(screen, "NEW GAME", 160, 56, LV_ALIGN_TOP_MID, -88, 422, game_new_cb);
    auto *game_back = button(screen, LV_SYMBOL_LEFT "  BACK", design::BackWidth, 56, LV_ALIGN_TOP_MID, 88, 422, back_cb);
    lv_obj_set_style_radius(game_back,design::TouchHeight/2,0);
    center_navigation_content(game_back);
    // Full-screen input shield; all game dialogs survive notification interruption.
    game_overlay = lv_obj_create(screen);
    lv_obj_remove_style_all(game_overlay);
    lv_obj_set_size(game_overlay, 410, 502);
    lv_obj_set_style_bg_color(game_overlay, lv_color_hex(design::Black), 0);
    lv_obj_set_style_bg_opa(game_overlay, LV_OPA_90, 0);
    lv_obj_remove_flag(game_overlay, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE));
    lv_obj_add_flag(game_overlay, LV_OBJ_FLAG_CLICKABLE);
    game_dialog_title = label(game_overlay, "", design::title(), kWhite, LV_ALIGN_TOP_MID, 0, 132);
    game_dialog_body = label(game_overlay, "", design::body(), kMuted, LV_ALIGN_TOP_MID, 0, 198);
    lv_obj_set_width(game_dialog_body, design::ContentWidth);
    lv_obj_set_style_text_align(game_dialog_body, LV_TEXT_ALIGN_CENTER, 0);
    game_dialog_left = button(game_overlay, "CANCEL", 160, 56, LV_ALIGN_TOP_MID, -88, 300, game_dialog_left_cb);
    game_dialog_right = button(game_overlay, "NEW GAME", 160, 56, LV_ALIGN_TOP_MID, 88, 300, game_dialog_right_cb);
    show_game_dialog(GameDialog::None);
    render_game();
}
static void refresh_media_record()
{
    if (!media_record) return;
    const bool spin = media_playing && display_active && current_screen == WatchScreen::Media;
    if (spin == media_record_spinning) return;
    media_record_spinning = spin;
    lv_anim_delete(media_record, rotate_media_record);
    if (!spin) return; // Keep current record position when paused/hidden/asleep.
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, media_record);
    lv_anim_set_exec_cb(&animation, rotate_media_record);
    lv_anim_set_values(&animation, 0, 360);
    lv_anim_set_duration(&animation, 4000);
    lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&animation);
}
static void create_media_screen()
{
    auto *screen = create_screen(WatchScreen::Media, "Media");
    media_status = label(screen, "MEDIA UNAVAILABLE", design::caption(), kMuted, LV_ALIGN_TOP_MID, 0, 86);
    media_title = label(screen, "Media unavailable", watch_font_30(), kWhite, LV_ALIGN_TOP_MID, 0, 150);
    lv_obj_set_size(media_title, design::ContentWidth, LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(media_title, 80, 0);
    lv_obj_set_style_text_align(media_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(media_title, LV_LABEL_LONG_DOT);
    media_artist = label(screen, "", design::body(), kMuted, LV_ALIGN_TOP_MID, 0, 252);
    lv_obj_set_size(media_artist, design::ContentWidth, LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(media_artist, 48, 0);
    lv_obj_set_style_text_align(media_artist, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(media_artist, LV_LABEL_LONG_DOT);
    auto command = [](lv_event_t *event) {
        const auto *target = lv_event_get_target(event);
        const uint8_t value = target == media_previous ? 1 : target == media_toggle ? 2 : target == media_next ? 3 : target == media_volume_down ? 4 : 5;
        if (media_callback && !media_callback(value, media_session)) lv_label_set_text(media_status, "COMMAND UNAVAILABLE / RETRY");
    };
    media_previous = button(screen, LV_SYMBOL_PREV, 64, 64, LV_ALIGN_TOP_MID, -112, 266, command);
    media_toggle = button(screen, LV_SYMBOL_PLAY, 80, 80, LV_ALIGN_TOP_MID, 0, 258, command);
    media_next = button(screen, LV_SYMBOL_NEXT, 64, 64, LV_ALIGN_TOP_MID, 112, 266, command);
    lv_obj_t *playback_controls[] = {media_previous, media_toggle, media_next};
    for (auto *control : playback_controls)
        lv_obj_set_style_radius(control, LV_RADIUS_CIRCLE, 0);
    // Original lightweight record groove/highlight; no artwork or image rotation.
    media_record = lv_arc_create(media_toggle);
    lv_obj_set_size(media_record, 68, 68);
    lv_obj_center(media_record);
    lv_arc_set_bg_angles(media_record, 0, 360);
    lv_arc_set_angles(media_record, 0, 52);
    lv_obj_set_style_arc_width(media_record, 2, LV_PART_MAIN);
    lv_obj_set_style_arc_color(media_record, lv_color_hex(design::Border), LV_PART_MAIN);
    lv_obj_set_style_arc_width(media_record, 3, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(media_record, lv_color_hex(kMediaAccent), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(media_record, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_remove_flag(media_record, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(media_record, LV_OBJ_FLAG_SCROLLABLE);
    media_volume_down = button(screen, "-", 56, 56, LV_ALIGN_TOP_MID, -100, 346, command);
    media_volume_up = button(screen, "+", 56, 56, LV_ALIGN_TOP_MID, 100, 346, command);
    lv_obj_set_style_radius(media_volume_down, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_radius(media_volume_up, LV_RADIUS_CIRCLE, 0);
    label(screen, LV_SYMBOL_VOLUME_MID "  VOLUME", design::caption(), kMuted, LV_ALIGN_TOP_MID, 0, 364);
    watch_ui_set_media(0, 0, 0, "", "");
}
} // namespace

void watch_ui_create(void)
{
    create_home_screen();
    create_launcher_screen();
    create_settings_menu();
    create_activity_screen();
    create_settings_screen();
    create_wifi_screen();
    create_bluetooth_screen();
    create_time_tools();
    create_forecast_screen();
    create_notification_screen();
    create_security_screens();
    create_media_screen();
    create_game_screen();
    create_voice_screen();
    create_astronomy_screen();
    create_planets_screen();
    watch_perf_monitor_create();
    current_screen = WatchScreen::Home;
}

void watch_ui_set_display_active(bool active)
{
    const bool changed = display_active != active;
    display_active = active;
    if (changed && active) refresh_astronomy();
    if (changed && active) refresh_planets();
    refresh_voice_face();
    if (changed && !active && current_screen == WatchScreen::Voice) cancel_voice();
    if (changed && !active) save_game_best(true);
    refresh_media_record();
    if (changed && active) {
        time_tools_tick(nullptr);
        refresh_time_tools(); // IDLE may have consumed the last changed second without drawing.
    }
}

void watch_ui_set_notification_dismiss_callback(watch_ui_notification_dismiss_cb_t callback) { notification_dismiss_callback = callback; }
void watch_ui_show_notification(const char *app, const char *title, const char *body)
{
    if (current_screen != WatchScreen::Notification) notification_return = current_screen;
    lv_label_set_text(notification_app, app && *app ? app : "Phone");
    lv_label_set_text(notification_title, title ? title : "");
    lv_label_set_text(notification_body, body ? body : "");
    lv_obj_scroll_to_y(lv_obj_get_parent(notification_body), 0, LV_ANIM_OFF);
    // Do not steal the held recording touch. The latest notification is
    // already stored in the labels and can open once recording finishes.
    if (current_screen == WatchScreen::Voice && (voice_held || voice_state == 1 || voice_state == 2)) {
        voice_notification_pending = true;
        return;
    }
    voice_notification_pending = false;
    navigate_to(WatchScreen::Notification);
}
void watch_ui_hide_notification(void)
{
    voice_notification_pending = false;
    if (current_screen == WatchScreen::Notification) navigate_to(notification_return);
}
void watch_ui_set_security_callbacks(watch_ui_pairing_cb_t pairing, watch_ui_forget_phone_cb_t forget)
{
    pairing_callback = pairing; forget_callback = forget;
}
void watch_ui_show_pairing(uint32_t token, uint32_t code)
{
    if (current_screen != WatchScreen::Pairing) pairing_return = current_screen;
    pairing_token = token;
    lv_label_set_text_fmt(pairing_code, "%06lu", static_cast<unsigned long>(code));
    lv_obj_remove_state(pairing_accept, LV_STATE_DISABLED); lv_obj_remove_state(pairing_reject, LV_STATE_DISABLED);
    navigate_to(WatchScreen::Pairing);
}
void watch_ui_hide_pairing(void) { if (current_screen == WatchScreen::Pairing) navigate_to(pairing_return); }
void watch_ui_set_security_status(const char *text, bool has_phone)
{
    lv_label_set_text(security_label, text ? text : "No paired phone");
    if (has_phone && forget_callback) lv_obj_remove_state(forget_phone_button, LV_STATE_DISABLED);
    else lv_obj_add_state(forget_phone_button, LV_STATE_DISABLED);
}
void watch_ui_set_media_callback(watch_ui_media_command_cb_t callback) { media_callback = callback; }
void watch_ui_set_astronomy_time(int64_t utc_seconds, bool valid)
{
    valid = valid && astronomy::valid_clock(utc_seconds);
    const bool planet_clock_changed = astronomy_clock_valid != valid ||
        watch_planets::valid_clock(astronomy_utc) != watch_planets::valid_clock(utc_seconds);
    const bool changed = astronomy_clock_valid != valid || astronomy_utc/60 != utc_seconds/60;
    astronomy_utc = utc_seconds; astronomy_clock_valid = valid;
    if (changed) refresh_astronomy();
    // Static orbital map: refreshed on entry/wake, not each clock tick.
    if (planet_clock_changed) refresh_planets();
}
void watch_ui_set_voice_callback(watch_ui_voice_cb_t callback) { voice_callback = callback; }
bool watch_ui_open_voice()
{
    if (!display_active || current_screen == WatchScreen::Pairing) return false;
    navigate_to(WatchScreen::Voice,false);
    return true;
}
static void render_voice_progress()
{
    if (!voice_status || voice_state != 6) return;
    const auto percent = watch_voice_feedback::percent(voice_confirmed,voice_total);
    lv_label_set_text_fmt(voice_status,"Sending to phone · %u%%",unsigned(percent));
    lv_label_set_text_fmt(voice_details,"%lu / %lu bytes confirmed",
        static_cast<unsigned long>(voice_confirmed > voice_total ? voice_total : voice_confirmed),static_cast<unsigned long>(voice_total));
    lv_bar_set_value(voice_level,percent,LV_ANIM_OFF);
}
void watch_ui_set_voice_progress(uint32_t confirmed_bytes, uint32_t total_bytes)
{
    voice_confirmed = confirmed_bytes; voice_total = total_bytes;
    render_voice_progress();
}
void watch_ui_set_voice(uint8_t state, uint32_t duration_ms, uint8_t level, const char *error)
{
    if (state == 4) state = 5; // Removed local playback state is not actionable.
    const bool thinking_changed = (voice_state == 7) != (state == 7);
    voice_state = state;
    voice_face_level = level > 100 ? 100 : level;
    if (!voice_status) return;
    const char *states[] = {"","Starting microphone","Recording · release to send","Waiting for phone","","Connection error","Sending to phone","Thinking...",""};
    const bool disconnected = state == 3 && bluetooth_state != WATCH_UI_BLUETOOTH_CONNECTED;
    lv_label_set_text(voice_status,state == 5 && error ? error : disconnected ? "Phone disconnected" : states[state < 9 ? state : 5]);
    if (state == 8) {
        lv_obj_add_flag(voice_status,LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(voice_happy,LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(voice_status,LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(voice_happy,LV_OBJ_FLAG_HIDDEN);
    }
    const bool test = error && strstr(error,"Test complete");
    lv_obj_set_style_text_color(voice_status,lv_color_hex(state == 2 || (state == 5 && !test) ? kWarning : kWhite),0);
    lv_label_set_text(voice_note,state == 5 ? watch_voice_feedback::hint(error) : disconnected ?
        "Connect your phone and open the app.\nAudio is kept for 30 s." : state == 3 ?
        "Waiting for the app to collect audio.\nCANCEL or BACK stops the request." : state == 6 ?
        "Progress confirmed by your phone.\nCANCEL or BACK stops the request." :
        "");
    if (error) lv_label_set_text(voice_details,"Hold to retry.");
    else lv_label_set_text_fmt(voice_details,"%lu.%lu s / max 8 s",static_cast<unsigned long>(duration_ms/1000),
        static_cast<unsigned long>((duration_ms%1000)/100));
    lv_bar_set_value(voice_level,level > 100 ? 100 : level,LV_ANIM_OFF);
    lv_obj_set_style_bg_color(voice_level,lv_color_hex(state == 6 ? design::Bluetooth : design::Activity),LV_PART_INDICATOR);
    render_voice_progress();
    if (state == 6 || state == 7) lv_obj_add_state(voice_hold,LV_STATE_DISABLED); else lv_obj_remove_state(voice_hold,LV_STATE_DISABLED);
    const bool showing = state == 8 || (state == 5 && lv_label_get_text(voice_answer)[0]);
    if (showing) lv_obj_add_flag(voice_note,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(voice_note,LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(voice_answer_heading,state == 5 ? "MESSAGE" : "ANSWER:");
    const bool question = lv_label_get_text(voice_transcription)[0] != 0;
    if (question) { lv_obj_remove_flag(voice_question_heading,LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(voice_transcription,LV_OBJ_FLAG_HIDDEN); }
    else { lv_obj_add_flag(voice_question_heading,LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(voice_transcription,LV_OBJ_FLAG_HIDDEN); }
    lv_obj_add_flag(voice_waiting,LV_OBJ_FLAG_HIDDEN);
    if (state == 5 && showing && error) lv_label_set_text(voice_answer,watch_voice_feedback::hint(error));
    auto *results = static_cast<lv_obj_t *>(lv_obj_get_user_data(voice_answer));
    if (showing) { lv_obj_remove_flag(results,LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(voice_hold,LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(voice_level,LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(voice_details,LV_OBJ_FLAG_HIDDEN); }
    else { lv_obj_add_flag(results,LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(voice_hold,LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(voice_level,LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(voice_details,LV_OBJ_FLAG_HIDDEN); }
    if (state != 2 && state != 6) lv_obj_add_flag(voice_level,LV_OBJ_FLAG_HIDDEN);
    if (state == 0 || state == 1 || state == 7) lv_obj_add_flag(voice_details,LV_OBJ_FLAG_HIDDEN);
    if (state == 7) lv_obj_add_flag(voice_hold,LV_OBJ_FLAG_HIDDEN);
    if (state == 5 && !showing) lv_obj_remove_flag(voice_hold,LV_OBJ_FLAG_HIDDEN);
    const bool retry = state == 8 || state == 5;
    lv_label_set_text(lv_obj_get_child(voice_hold,0),retry ? "HOLD / RETRY" : state == 2 ? "RELEASE TO SEND" : "HOLD TO TALK");
    lv_obj_align(lv_obj_get_child(voice_hold,0),LV_ALIGN_CENTER,retry || state == 2 ? 0 : 18,0);
    if (retry || state == 2) lv_obj_add_flag(voice_microphone,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(voice_microphone,LV_OBJ_FLAG_HIDDEN);
    if (state == 8) { lv_obj_align(results,LV_ALIGN_TOP_MID,0,226); lv_obj_set_height(results,184); }
    else if (showing) { lv_obj_align(results,LV_ALIGN_TOP_MID,0,136); lv_obj_set_height(results,274); }
    else { lv_obj_align(results,LV_ALIGN_TOP_MID,0,162); lv_obj_set_height(results,190); }
    if (state == 8 || state == 5) { lv_obj_set_size(voice_hold,160,56); lv_obj_align(voice_hold,LV_ALIGN_TOP_MID,-88,354); }
    else { lv_obj_set_size(voice_hold,design::ContentWidth,72); lv_obj_align(voice_hold,LV_ALIGN_TOP_MID,0,282); }
    lv_obj_set_style_radius(voice_hold,retry ? design::Radius : 20,0);
    lv_obj_set_width(voice_clear,state == 8 || state == 5 ? 160 : design::ContentWidth);
    lv_obj_align(voice_clear,LV_ALIGN_TOP_MID,state == 8 || state == 5 ? 88 : 0,state == 8 || state == 5 || state == 7 ? 354 : 362);
    const bool cancellable = state == 1 || state == 2 || state == 3 || state == 6 || state == 7;
    lv_label_set_text(lv_obj_get_child(voice_clear,0),cancellable ? "CANCEL" : "CLEAR");
    if (state == 0 || state == 1 || state == 3 || state == 6 || state == 7 || showing) lv_obj_add_flag(voice_clear,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(voice_clear,LV_OBJ_FLAG_HIDDEN);
    if (showing && state != 8) { lv_obj_add_flag(voice_face,LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(voice_name,LV_OBJ_FLAG_HIDDEN); }
    else { lv_obj_remove_flag(voice_face,LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(voice_name,LV_OBJ_FLAG_HIDDEN); }
    lv_obj_align(voice_name,LV_ALIGN_TOP_MID,0,66);
    lv_obj_align(voice_face,LV_ALIGN_TOP_MID,0,64);
    lv_obj_align(voice_status,LV_ALIGN_TOP_MID,0,showing ? 84 : 218);
    // The face replaces the introductory space; retain errors in the result
    // card and actual state/duration/progress below it without overlap.
    lv_obj_add_flag(voice_note,LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(voice_details,LV_ALIGN_TOP_MID,0,246);
    lv_obj_align(voice_level,LV_ALIGN_TOP_MID,0,268);
    refresh_voice_face();
    if (thinking_changed && display_active && current_screen == WatchScreen::Voice) {
        lv_area_t eyes; lv_obj_get_coords(voice_face,&eyes);
        const int cx=(eyes.x1+eyes.x2)/2,cy=(eyes.y1+eyes.y2)/2;
        eyes={cx-28,cy-24,cx+28,cy+24};
        lv_obj_invalidate_area(voice_face,&eyes);
    }
    if (voice_notification_pending && !voice_held && state != 1 && state != 2 && current_screen == WatchScreen::Voice) {
        voice_notification_pending = false;
        navigate_to(WatchScreen::Notification);
    }
}
void watch_ui_set_voice_text(const char *transcription, const char *answer)
{
    if (!voice_answer) return;
    lv_label_set_text(voice_transcription,transcription ? transcription : "");
    lv_label_set_text(voice_answer,answer ? answer : "");
    auto *results = static_cast<lv_obj_t *>(lv_obj_get_user_data(voice_answer));
    lv_obj_scroll_to_y(results,0,LV_ANIM_OFF);
}
void watch_ui_configure_2048(uint32_t best, uint32_t random_seed, watch_ui_2048_best_cb_t callback)
{
    if (best > game_2048.best) game_2048.best = best;
    game_2048.random_state = random_seed ? random_seed : 0x20482048;
    game_best_callback = callback;
    render_game();
}
void watch_ui_set_media(uint8_t state, uint8_t actions, uint32_t session, const char *title, const char *artist)
{
    media_session = session;
    media_playing = state == 1 && session != 0;
    refresh_media_record();
    lv_label_set_text(media_status, state == 1 ? "PLAYING" : state == 2 ? "PAUSED" : "MEDIA UNAVAILABLE");
    lv_label_set_text(media_title, state && title && *title ? title : state ? "Unknown track" : "Media unavailable");
    if (state) lv_label_set_text_fmt(media_artist, "Artist: %s", artist && *artist ? artist : "Unknown artist");
    else lv_label_set_text(media_artist, "");
    lv_obj_update_layout(media_title);
    lv_obj_update_layout(media_artist);
    const int title_height = lv_obj_get_height(media_title), artist_height = lv_obj_get_height(media_artist);
    const int text_y = 110 + (140 - title_height - 8 - artist_height) / 2;
    lv_obj_align(media_title, LV_ALIGN_TOP_MID, 0, text_y);
    lv_obj_align(media_artist, LV_ALIGN_TOP_MID, 0, text_y + title_height + 8);
    lv_label_set_text(lv_obj_get_child(media_toggle, 0), state == 1 ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    lv_obj_t *buttons[] = {media_previous, media_toggle, media_next, media_volume_down, media_volume_up};
    for (unsigned i=0; i<5; ++i) {
        if (state && session && (actions & (1 << i))) lv_obj_remove_state(buttons[i], LV_STATE_DISABLED);
        else lv_obj_add_state(buttons[i], LV_STATE_DISABLED);
    }
}

void watch_ui_set_settings(uint8_t brightness, uint32_t timeout_ms)
{
    uint16_t selection = 1;
    for (uint16_t i = 0; i < 5; ++i) {
        if (kTimeouts[i] == timeout_ms) { selection = i; break; }
    }
    current_brightness = brightness;
    display_timeout_ms = kTimeouts[selection];
    refresh_settings_values();
    if (brightness_slider != nullptr) lv_slider_set_value(brightness_slider, brightness, LV_ANIM_OFF);
    if (brightness_label != nullptr) lv_label_set_text_fmt(brightness_label, "%u%%", brightness);
    if (screen_timeout_dropdown != nullptr) lv_dropdown_set_selected(screen_timeout_dropdown, selection);
}

void watch_ui_set_time(const char *time)
{
    if (time_label == nullptr || time == nullptr) return;
    // Display HH:MM rather than seconds to keep the main clock calm and legible.
    char text[6];
    snprintf(text, sizeof(text), "%.5s", time);
    if (strcmp(clock_text, text) != 0) {
        snprintf(clock_text, sizeof(clock_text), "%s", text);
        lv_obj_invalidate(time_label);
    }
}

void watch_ui_set_date(const char *date)
{
    if (date_label != nullptr && date != nullptr) {
        char text[64];
        const char *separator = strchr(date, ' ');
        const char *numeric_date = separator;
        while (numeric_date && *numeric_date == ' ') ++numeric_date;
        if (separator && numeric_date && isdigit(static_cast<unsigned char>(*numeric_date)))
            snprintf(text, sizeof(text), "%.*s - %s", static_cast<int>(separator - date), date, numeric_date);
        else snprintf(text, sizeof(text), "%s", date);
        for (char *p = text; *p; ++p) *p = static_cast<char>(toupper(static_cast<unsigned char>(*p)));
        lv_label_set_text(date_label, text);
    }
}

void watch_ui_set_steps(uint32_t steps)
{
    today_steps = steps;
    char raw[16], text[20];
    snprintf(raw, sizeof(raw), "%lu", static_cast<unsigned long>(steps));
    const unsigned length = strlen(raw);
    unsigned out = 0;
    for (unsigned i = 0; i < length; ++i) {
        if (i && (length - i) % 3 == 0) text[out++] = ' ';
        text[out++] = raw[i];
    }
    text[out] = '\0';
    if (steps_label) lv_obj_set_style_text_font(steps_label, steps >= 1000000 ? design::body() : &lv_font_montserrat_26, 0);
    if (activity_steps_label) lv_obj_set_style_text_font(activity_steps_label, steps >= 1000000 ? design::title() : design::number(), 0);
    if (steps_label != nullptr) lv_label_set_text(steps_label, text);
    if (activity_steps_label != nullptr) lv_label_set_text(activity_steps_label, text);
    refresh_step_goal();
}

namespace {
static void refresh_step_goal()
{
    char text[32];
    snprintf(text, sizeof(text), "Goal %lu,%03lu", static_cast<unsigned long>(daily_step_goal / 1000),
             static_cast<unsigned long>(daily_step_goal % 1000));
    if (steps_goal_label) lv_label_set_text(steps_goal_label, text);
    if (activity_goal_label) lv_label_set_text(activity_goal_label, text);
    if (steps_progress) {
        lv_bar_set_range(steps_progress, 0, daily_step_goal);
        lv_bar_set_value(steps_progress, today_steps >= daily_step_goal ? daily_step_goal : today_steps, LV_ANIM_OFF);
    }
    if (goal_minus) {
        if (daily_step_goal == 1000) lv_obj_add_state(goal_minus, LV_STATE_DISABLED);
        else lv_obj_remove_state(goal_minus, LV_STATE_DISABLED);
    }
    if (goal_plus) {
        if (daily_step_goal == 30000) lv_obj_add_state(goal_plus, LV_STATE_DISABLED);
        else lv_obj_remove_state(goal_plus, LV_STATE_DISABLED);
    }
}

} // namespace

void watch_ui_set_step_goal(uint32_t goal)
{
    if (goal < 1000 || goal > 30000 || goal % 1000) return;
    daily_step_goal = goal;
    refresh_step_goal();
}

void watch_ui_set_step_goal_callback(watch_ui_step_goal_cb_t callback) { step_goal_callback = callback; }

void watch_ui_set_wifi(bool connected, int rssi)
{
    watch_ui_set_wifi_state(connected ? WATCH_UI_WIFI_CONNECTED : WATCH_UI_WIFI_DISCONNECTED, rssi);
}

void watch_ui_set_wifi_state(watch_ui_wifi_state_t state, int rssi)
{
    wifi_state = state;
    const bool connected = state == WATCH_UI_WIFI_CONNECTED;
    const uint32_t color = state == WATCH_UI_WIFI_DISCONNECTED ? kMuted : design::Wifi;
    const char *status = connected ? "CONNECTED" : state == WATCH_UI_WIFI_CONNECTING ? "CONNECTING" : "DISCONNECTED";
    if (wifi_status_label != nullptr) {
        lv_label_set_text_fmt(wifi_status_label, LV_SYMBOL_WIFI " %s", connected ? "ON" : state == WATCH_UI_WIFI_CONNECTING ? "LINK" : "OFF");
        lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(color), 0);
    }
    if (launcher_wifi_label != nullptr) {
        lv_label_set_text(launcher_wifi_label, status);
        lv_obj_set_style_text_color(launcher_wifi_label, lv_color_hex(color), 0);
    }
    if (wifi_detail_label != nullptr) {
        lv_label_set_text(wifi_detail_label, status);
        lv_obj_set_style_text_color(wifi_detail_label, lv_color_hex(color), 0);
    }
    if (wifi_signal_label != nullptr) {
        if (connected && rssi != 0) lv_label_set_text_fmt(wifi_signal_label, "Signal: %d dBm", rssi);
        else lv_label_set_text(wifi_signal_label, connected ? "Signal unavailable" : "No network connection");
    }
    if (wifi_action != nullptr) {
        lv_label_set_text(wifi_action_label, connected ? "Disconnect" : state == WATCH_UI_WIFI_CONNECTING ? "Connecting..." : "Connect");
        if (state == WATCH_UI_WIFI_CONNECTING || !wifi_callback) lv_obj_add_state(wifi_action, LV_STATE_DISABLED);
        else lv_obj_remove_state(wifi_action, LV_STATE_DISABLED);
    }
}

void watch_ui_set_wifi_control_callback(watch_ui_wifi_control_cb_t callback)
{
    wifi_callback = callback;
}

void watch_ui_set_bluetooth_control_callback(watch_ui_bluetooth_control_cb_t callback)
{
    bluetooth_callback = callback;
}

void watch_ui_set_wifi_name(const char *name)
{
    if (wifi_name_label) lv_label_set_text(wifi_name_label, name ? name : "");
}

void watch_ui_set_bluetooth_state(watch_ui_bluetooth_state_t state)
{
    bluetooth_state = state;
    const char *text = state == WATCH_UI_BLUETOOTH_DISABLED ? "DISABLED" :
                       state == WATCH_UI_BLUETOOTH_CONNECTED ? "CONNECTED" : "AVAILABLE";
    const uint32_t color = state == WATCH_UI_BLUETOOTH_DISABLED ? kMuted : design::Bluetooth;
    if (bluetooth_status_label) {
        lv_label_set_text_fmt(bluetooth_status_label, LV_SYMBOL_BLUETOOTH " %s",
                              state == WATCH_UI_BLUETOOTH_DISABLED ? "OFF" : state == WATCH_UI_BLUETOOTH_CONNECTED ? "ON" : "READY");
        lv_obj_set_style_text_color(bluetooth_status_label, lv_color_hex(color), 0);
    }
    if (launcher_bluetooth_label) {
        lv_label_set_text(launcher_bluetooth_label, text);
        lv_obj_set_style_text_color(launcher_bluetooth_label, lv_color_hex(color), 0);
    }
    if (bluetooth_detail_label) {
        lv_label_set_text(bluetooth_detail_label, text);
        lv_obj_set_style_text_color(bluetooth_detail_label, lv_color_hex(color), 0);
    }
    if (bluetooth_action) {
        lv_label_set_text(bluetooth_action_label, state == WATCH_UI_BLUETOOTH_DISABLED ?
                          "Enable Bluetooth" : "Disable Bluetooth");
        if (bluetooth_callback) lv_obj_remove_state(bluetooth_action, LV_STATE_DISABLED);
        else lv_obj_add_state(bluetooth_action, LV_STATE_DISABLED);
    }
}

void watch_ui_set_weather(bool available, float temperature, watch_ui_weather_condition_t condition)
{
    if (!weather_label) return;
    if (available) {
        weather_condition = condition;
        lv_obj_remove_flag(weather_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_invalidate(weather_icon);
        char text[40];
        snprintf(text, sizeof(text), "%.0f\xC2\xB0" "C", temperature);
        lv_label_set_text(weather_label, text);
        lv_obj_align_to(weather_icon, weather_label, LV_ALIGN_OUT_RIGHT_MID, 12, 0);
        const char *category = condition == WATCH_UI_WEATHER_SUNNY ? "SUNNY" :
            condition == WATCH_UI_WEATHER_CLOUDY ? "CLOUDY" :
            condition == WATCH_UI_WEATHER_RAIN ? "RAIN" :
            condition == WATCH_UI_WEATHER_THUNDERSTORM ? "STORM" : "WIND";
        lv_label_set_text_fmt(weather_category_label, "WEATHER / %s", category);
    } else {
        lv_obj_add_flag(weather_icon, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(weather_label, "--");
        lv_label_set_text(weather_category_label, "WEATHER / UNAVAILABLE");
    }
}

void watch_ui_set_forecast(const watch_ui_forecast_day_t days[7], bool online, uint32_t age_minutes)
{
    for (unsigned i = 0; i < 7; ++i) forecast_days[i] = days ? days[i] : watch_ui_forecast_day_t{};
    forecast_online = online;
    forecast_age = age_minutes;
    if (weather_range_label) {
        if (forecast_days[0].available) lv_label_set_text_fmt(weather_range_label, "H %.0f\xC2\xB0 / L %.0f\xC2\xB0",
            forecast_days[0].high_c, forecast_days[0].low_c);
        else lv_label_set_text(weather_range_label, "H -- / L --");
    }
    if (display_active && current_screen == WatchScreen::Weather) render_forecast();
}

void watch_ui_set_history(const watch_ui_activity_day_t days[7])
{
    static const char *weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    for (unsigned i = 0; i < 7; ++i) {
        if (!history_names[i]) return;
        if (!days) {
            lv_label_set_text(history_names[i], "--");
            lv_label_set_text(history_counts[i], "--");
        } else {
            char day[4];
            snprintf(day, sizeof(day), "%.3s", weekdays[days[i].weekday % 7]);
            for (char *p = day; *p; ++p) *p = static_cast<char>(toupper(static_cast<unsigned char>(*p)));
            lv_label_set_text_fmt(history_names[i], "%s%s", day, i == 0 ? " / TODAY" : "");
            lv_label_set_text_fmt(history_counts[i], "%lu", static_cast<unsigned long>(days[i].steps));
        }
    }
}

void watch_ui_set_battery(int percentage, bool charging)
{
    if (battery_label == nullptr) return;
    if (percentage < 0) {
        lv_label_set_text(battery_label, LV_SYMBOL_BATTERY_EMPTY "  --%");
        lv_obj_set_style_text_color(battery_label, lv_color_hex(kMuted), 0);
        return;
    }
    if (percentage > 100) percentage = 100;
    const char *symbol = percentage <= 20 ? LV_SYMBOL_BATTERY_EMPTY :
        percentage <= 50 ? LV_SYMBOL_BATTERY_2 : LV_SYMBOL_BATTERY_FULL;
    lv_label_set_text_fmt(battery_label, "%s%s %d%%", charging ? LV_SYMBOL_CHARGE " " : "",
                         symbol, percentage);
    const uint32_t color = percentage >= 70 ? design::BatteryHigh :
        percentage >= 50 ? design::BatteryMedium :
        percentage >= 30 ? design::BatteryLow : design::BatteryCritical;
    lv_obj_set_style_text_color(battery_label, lv_color_hex(color), 0);
}

void watch_ui_set_brightness_callback(watch_ui_brightness_cb_t callback)
{
    brightness_callback = callback;
}

void watch_ui_set_brightness_committed_callback(watch_ui_brightness_committed_cb_t callback)
{
    brightness_committed_callback = callback;
}

void watch_ui_set_timeout_callback(watch_ui_timeout_cb_t callback)
{
    timeout_callback = callback;
}
