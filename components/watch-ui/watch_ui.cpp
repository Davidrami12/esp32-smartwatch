#include "watch_ui.h"
#include "watch_design.h"
#include "watch_time_tools.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

namespace {

// Shared visual tokens; no hardware or activity logic belongs in this module.
namespace design = watch_design;
constexpr uint32_t kSurface = design::Surface;
constexpr uint32_t kMuted = design::Secondary;
constexpr uint32_t kAccent = design::Connected;
constexpr uint32_t kWhite = design::Primary;
constexpr uint32_t kWarning = design::Warning;
constexpr uint32_t kTimeouts[] = {5000, 10000, 30000, 60000, 0};

enum class WatchScreen { Home, Launcher, Activity, Settings, Wifi, Bluetooth, Brightness, Timeout, Timer, Stopwatch, Weather };
static WatchScreen current_screen = WatchScreen::Home;
static lv_obj_t *screens[11] = {};
static lv_obj_t *forecast_status, *forecast_names[7], *forecast_values[7], *forecast_icons[7];
static watch_ui_forecast_day_t forecast_days[7] = {};
static bool forecast_online = false;
static uint32_t forecast_age = 0;
static void render_forecast();
static bool display_active = true;
static WatchTimeTool countdown, stopwatch;
static uint32_t timer_minutes = 5;
static lv_obj_t *timer_value, *timer_status, *timer_action_label, *timer_minus, *timer_plus;
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
    const bool back = to == WatchScreen::Home ||
        ((to == WatchScreen::Launcher || to == WatchScreen::Settings) && from != WatchScreen::Home);
    return back ? LV_SCR_LOAD_ANIM_MOVE_RIGHT : LV_SCR_LOAD_ANIM_MOVE_LEFT;
}

static void navigate_to(WatchScreen screen)
{
    if (screen == current_screen) return;
    lv_screen_load_anim(screen_for(screen), navigation_animation(current_screen, screen), design::TransitionMs, 0, false);
    current_screen = screen;
    refresh_time_tools();
    if (screen == WatchScreen::Weather) render_forecast();
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
    const bool settings_child = current_screen == WatchScreen::Wifi || current_screen == WatchScreen::Bluetooth ||
        current_screen == WatchScreen::Brightness || current_screen == WatchScreen::Timeout;
    navigate_to(current_screen == WatchScreen::Launcher || current_screen == WatchScreen::Settings ?
                WatchScreen::Home : settings_child ? WatchScreen::Settings : WatchScreen::Launcher);
}

static void navigation_gesture_cb(lv_event_t *)
{
    lv_indev_t *input = lv_indev_active();
    if (input == nullptr) return;
    const lv_dir_t direction = lv_indev_get_gesture_dir(input);
    if (current_screen == WatchScreen::Home && direction == LV_DIR_LEFT) {
        lv_indev_wait_release(input);
        navigate_to(WatchScreen::Launcher);
    } else if (current_screen != WatchScreen::Home && direction == LV_DIR_RIGHT) {
        lv_indev_wait_release(input);
        back_cb(nullptr);
    }
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
    lv_obj_set_size(object, width, height);
    lv_obj_align(object, align, x, y);
    style_surface(object);
    style_press_feedback(object);
    lv_obj_set_style_text_color(object, lv_color_hex(design::Disabled), LV_STATE_DISABLED);
    if (callback) lv_obj_add_event_cb(object, callback, LV_EVENT_CLICKED, nullptr);
    label(object, text, design::body(), kWhite, LV_ALIGN_CENTER, 0, 0);
    return object;
}

static lv_obj_t *home_navigation_button(lv_obj_t *screen, bool settings)
{
    lv_obj_t *object = button(screen, settings ? LV_SYMBOL_SETTINGS "  SETTINGS" : LV_SYMBOL_LIST "  APPS",
                             design::TileWidth, design::TouchHeight, LV_ALIGN_BOTTOM_MID,
                             settings ? -design::TileOffset : design::TileOffset, -design::BackInset,
                             settings ? open_settings_cb : open_launcher_cb);
    lv_obj_set_style_text_font(lv_obj_get_child(object, 0), &lv_font_montserrat_16, 0);
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
        lv_obj_set_style_text_letter_space(heading_label, 2, 0);
        const bool parent_screen = id == WatchScreen::Launcher || id == WatchScreen::Settings;
        button(screen, parent_screen ? LV_SYMBOL_HOME "  HOME" : LV_SYMBOL_LEFT "  BACK",
               240, design::TouchHeight, LV_ALIGN_BOTTOM_MID, 0, -design::BackInset, back_cb);
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

static void create_launcher_screen()
{
    lv_obj_t *screen = create_screen(WatchScreen::Launcher, "Apps");
    lv_obj_t *activity = app_tile(screen, "ACTIVITY", "", -design::TileOffset, design::TileFirstY, design::Activity, open_activity_cb);
    lv_obj_t *activity_icon = footprints_icon(activity, 36, design::Activity);
    lv_obj_align(activity_icon, LV_ALIGN_TOP_MID, 0, 26);
    app_tile(screen, "TIMER", LV_SYMBOL_BELL, design::TileOffset, design::TileFirstY, design::Timer, open_timer_cb);
    app_tile(screen, "STOPWATCH", LV_SYMBOL_PLAY, -design::TileOffset, design::TileSecondY, design::Stopwatch, open_stopwatch_cb);
    lv_obj_t *weather = app_tile(screen, "WEATHER", "", design::TileOffset, design::TileSecondY, design::Weather, open_weather_cb);
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
    label(screen, "ESP32 Smartwatch", &lv_font_montserrat_18, kMuted,
          LV_ALIGN_TOP_MID, 0, 244);
    bluetooth_action = button(screen, "Enable Bluetooth", 300, 64, LV_ALIGN_TOP_MID, 0, 314,
                              bluetooth_action_cb);
    bluetooth_action_label = lv_obj_get_child(bluetooth_action, 0);
    lv_obj_add_state(bluetooth_action, LV_STATE_DISABLED);
}

static void create_activity_screen()
{
    lv_obj_t *screen = create_screen(WatchScreen::Activity, "Activity");
    label(screen, "TODAY / STEPS", design::caption(), kMuted,
          LV_ALIGN_TOP_LEFT, design::Margin, 78);
    activity_steps_label = label(screen, "0", design::number(), design::Activity,
                                 LV_ALIGN_TOP_LEFT, design::Margin, design::ActivityCountY);
    goal_minus = button(screen, "-", 48, 48, LV_ALIGN_TOP_LEFT, design::Margin, 158, step_goal_changed_cb);
    activity_goal_label = label(screen, "Goal 8,000", design::caption(), kMuted, LV_ALIGN_TOP_MID, 0, 174);
    goal_plus = button(screen, "+", 48, 48, LV_ALIGN_TOP_RIGHT, -design::Margin, 158, step_goal_changed_cb);
    label(screen, "7 DAYS / LOCAL", design::caption(), kMuted,
          LV_ALIGN_TOP_LEFT, design::Margin, 212);
    for (unsigned i = 0; i < 7; ++i) {
        const int y = 238 + i * 26;
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
    wifi_action = button(screen, "Connect", 300, 64, LV_ALIGN_TOP_MID, 0, 314, wifi_action_cb);
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
    lv_obj_set_size(brightness_slider, 202, 16);
    lv_obj_align(brightness_slider, LV_ALIGN_TOP_MID, 0, 213);
    lv_obj_set_ext_click_area(brightness_slider, 12);
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
        const uint64_t duration = static_cast<uint64_t>(timer_minutes) * 60000;
        const bool finished = countdown.elapsed_ms >= duration;
        const uint64_t remaining = finished ? 0 : duration - countdown.elapsed_ms;
        format_elapsed(text, sizeof(text), ((remaining + 999) / 1000) * 1000, false);
        lv_label_set_text(timer_value, text);
        lv_label_set_text(timer_status, finished ? "TIME'S UP" : countdown.running ? "RUNNING" : countdown.elapsed_ms ? "PAUSED" : "SET MINUTES / 1-120");
        lv_label_set_text(timer_action_label, finished ? "RESTART" : countdown.running ? "PAUSE" : countdown.elapsed_ms ? "RESUME" : "START");
        if (countdown.running || countdown.elapsed_ms) {
            lv_obj_add_state(timer_minus, LV_STATE_DISABLED); lv_obj_add_state(timer_plus, LV_STATE_DISABLED);
        } else {
            lv_obj_remove_state(timer_minus, LV_STATE_DISABLED); lv_obj_remove_state(timer_plus, LV_STATE_DISABLED);
        }
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
    const uint64_t duration = static_cast<uint64_t>(timer_minutes) * 60000;
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
    if (countdown.elapsed_ms >= static_cast<uint64_t>(timer_minutes) * 60000) countdown.reset(now);
    countdown.toggle(now); refresh_time_tools();
}
static void timer_reset_cb(lv_event_t *) { countdown.reset(lv_tick_get()); refresh_time_tools(); }
static void timer_adjust_cb(lv_event_t *event)
{
    if (countdown.running || countdown.elapsed_ms) return;
    int value = static_cast<int>(timer_minutes) + (lv_event_get_user_data(event) ? 1 : -1);
    if (value >= 1 && value <= 120) timer_minutes = value;
    refresh_time_tools();
}
static void stopwatch_control_cb(lv_event_t *) { stopwatch.toggle(lv_tick_get()); refresh_time_tools(); }
static void stopwatch_reset_cb(lv_event_t *) { stopwatch.reset(lv_tick_get()); refresh_time_tools(); }

static void create_time_tools()
{
    lv_obj_t *screen = create_screen(WatchScreen::Timer, "Timer");
    timer_status = label(screen, "SET MINUTES / 1-120", design::caption(), kMuted, LV_ALIGN_TOP_MID, 0, 116);
    timer_value = label(screen, "00:05:00", &lv_font_montserrat_48, kWhite, LV_ALIGN_TOP_MID, 0, 162);
    timer_minus = button(screen, "- 1 MIN", 148, 56, LV_ALIGN_TOP_MID, -84, 240, nullptr);
    timer_plus = button(screen, "+ 1 MIN", 148, 56, LV_ALIGN_TOP_MID, 84, 240, nullptr);
    static bool increment = true;
    lv_obj_add_event_cb(timer_minus, timer_adjust_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(timer_plus, timer_adjust_cb, LV_EVENT_CLICKED, &increment);
    lv_obj_t *action = button(screen, "START", 148, 56, LV_ALIGN_TOP_MID, -84, 320, timer_control_cb);
    timer_action_label = lv_obj_get_child(action, 0);
    button(screen, "RESET", 148, 56, LV_ALIGN_TOP_MID, 84, 320, timer_reset_cb);
    label(screen, "VISUAL ALERT ONLY", design::caption(), kMuted, LV_ALIGN_TOP_MID, 0, 392);

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
    current_screen = WatchScreen::Home;
}

void watch_ui_set_display_active(bool active)
{
    const bool changed = display_active != active;
    display_active = active;
    if (changed && active) {
        time_tools_tick(nullptr);
        refresh_time_tools(); // IDLE may have consumed the last changed second without drawing.
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
                              state == WATCH_UI_BLUETOOTH_DISABLED ? "OFF" : state == WATCH_UI_BLUETOOTH_CONNECTED ? "LINK" : "READY");
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
