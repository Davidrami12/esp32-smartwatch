#include "watch_ui.h"

#include <stdio.h>

namespace {

// Shared visual tokens; no hardware or activity logic belongs in this module.
constexpr uint32_t kSurface = 0x151C26;
constexpr uint32_t kMuted = 0xA3AFBF;
constexpr uint32_t kAccent = 0x74DCC8;
constexpr uint32_t kWhite = 0xF4F7FB;
constexpr uint32_t kWarning = 0xFFB26B;
constexpr uint32_t kTimeouts[] = {5000, 10000, 30000, 60000, 0};

enum class WatchScreen { Home, Launcher, Activity, Settings, Wifi };
static WatchScreen current_screen = WatchScreen::Home;
static lv_obj_t *screens[5] = {};
static lv_obj_t *time_label, *date_label, *steps_label, *activity_steps_label;
static lv_obj_t *battery_label, *wifi_status_label, *wifi_detail_label;
static lv_obj_t *wifi_signal_label, *launcher_wifi_label;
static lv_obj_t *wifi_action, *wifi_action_label;
static lv_obj_t *history_names[7], *history_counts[7];
static lv_obj_t *weather_label, *weather_icon;
static watch_ui_weather_condition_t weather_condition = WATCH_UI_WEATHER_SUNNY;
static watch_ui_wifi_state_t wifi_state = WATCH_UI_WIFI_DISCONNECTED;
static watch_ui_wifi_control_cb_t wifi_callback = nullptr;
static lv_obj_t *brightness_label, *brightness_slider, *screen_timeout_dropdown;
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

static void navigate_to(WatchScreen screen)
{
    if (screen == current_screen) return;
    const bool back = screen == WatchScreen::Home ||
        (screen == WatchScreen::Launcher && current_screen != WatchScreen::Home);
    lv_screen_load_anim(screen_for(screen), back ? LV_SCR_LOAD_ANIM_MOVE_RIGHT :
                        LV_SCR_LOAD_ANIM_MOVE_LEFT, 250, 0, false);
    current_screen = screen;
}

static void open_launcher_cb(lv_event_t *) { navigate_to(WatchScreen::Launcher); }
static void open_activity_cb(lv_event_t *) { navigate_to(WatchScreen::Activity); }
static void open_settings_cb(lv_event_t *) { navigate_to(WatchScreen::Settings); }
static void open_wifi_cb(lv_event_t *) { navigate_to(WatchScreen::Wifi); }
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
    navigate_to(current_screen == WatchScreen::Launcher ? WatchScreen::Home :
                WatchScreen::Launcher);
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
    lv_obj_set_style_radius(object, 24, 0);
    lv_obj_set_style_border_width(object, 0, 0);
    lv_obj_set_style_shadow_width(object, 0, 0);
    lv_obj_set_style_pad_all(object, 0, 0);
}

static lv_obj_t *button(lv_obj_t *screen, const char *text, int width, int height,
                        lv_align_t align, int x, int y, lv_event_cb_t callback)
{
    lv_obj_t *object = lv_button_create(screen);
    lv_obj_set_size(object, width, height);
    lv_obj_align(object, align, x, y);
    style_surface(object);
    lv_obj_set_style_bg_color(object, lv_color_hex(0x293C48), LV_STATE_PRESSED);
    lv_obj_add_event_cb(object, callback, LV_EVENT_CLICKED, nullptr);
    label(object, text, &lv_font_montserrat_20, kWhite, LV_ALIGN_CENTER, 0, 0);
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
        label(screen, title, &lv_font_montserrat_26, kWhite, LV_ALIGN_TOP_MID, 0, 36);
        button(screen, LV_SYMBOL_LEFT "  Back", 240, 60, LV_ALIGN_BOTTOM_MID, 0, -32, back_cb);
    }
    return screen;
}

// Tiny procedural icons: no image buffers/assets or Unicode emoji font required.
static void weather_draw_cb(lv_event_t *event)
{
    lv_area_t area;
    lv_obj_get_coords(weather_icon, &area);
    lv_layer_t *layer = lv_event_get_layer(event);
    auto line = [&](int x1, int y1, int x2, int y2) {
        lv_draw_line_dsc_t dsc;
        lv_draw_line_dsc_init(&dsc);
        dsc.color = lv_color_hex(kAccent);
        dsc.width = 3;
        dsc.p1 = {static_cast<lv_value_precise_t>(area.x1 + x1), static_cast<lv_value_precise_t>(area.y1 + y1)};
        dsc.p2 = {static_cast<lv_value_precise_t>(area.x1 + x2), static_cast<lv_value_precise_t>(area.y1 + y2)};
        lv_draw_line(layer, &dsc);
    };
    auto disc = [&](int x, int y, int width, int height) {
        lv_draw_rect_dsc_t dsc;
        lv_draw_rect_dsc_init(&dsc);
        dsc.bg_color = lv_color_hex(kAccent);
        dsc.radius = LV_RADIUS_CIRCLE;
        lv_area_t box = {area.x1 + x, area.y1 + y, area.x1 + x + width - 1, area.y1 + y + height - 1};
        lv_draw_rect(layer, &dsc, &box);
    };
    if (weather_condition == WATCH_UI_WEATHER_SUNNY) {
        disc(10, 10, 14, 14);
        line(17, 1, 17, 6); line(17, 28, 17, 33);
        line(1, 17, 6, 17); line(28, 17, 33, 17);
        line(4, 4, 8, 8); line(26, 26, 30, 30);
        line(4, 30, 8, 26); line(26, 8, 30, 4);
    } else if (weather_condition == WATCH_UI_WEATHER_WINDY) {
        line(2, 9, 25, 9); line(7, 17, 32, 17); line(2, 25, 23, 25);
        line(25, 9, 28, 5); line(32, 17, 29, 21);
    } else {
        disc(3, 11, 28, 12); disc(8, 5, 16, 16); disc(19, 9, 13, 13);
        if (weather_condition == WATCH_UI_WEATHER_RAIN) {
            line(9, 26, 6, 32); line(18, 26, 15, 32); line(27, 26, 24, 32);
        } else if (weather_condition == WATCH_UI_WEATHER_THUNDERSTORM) {
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
        lv_draw_rect_dsc_t dsc;
        lv_draw_rect_dsc_init(&dsc);
        dsc.bg_color = lv_obj_get_style_text_color(object, LV_PART_MAIN);
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

static void create_home_screen()
{
    lv_obj_t *screen = create_screen(WatchScreen::Home, nullptr);
    battery_label = label(screen, LV_SYMBOL_BATTERY_FULL "  --%", &lv_font_montserrat_16,
                          kMuted, LV_ALIGN_TOP_RIGHT, -34, 34);
    wifi_status_label = label(screen, LV_SYMBOL_WIFI "  Waiting", &lv_font_montserrat_16,
                              kMuted, LV_ALIGN_TOP_LEFT, 34, 34);
    // HH:MM gets priority; the firmware still supplies its unchanged time string.
    time_label = label(screen, "--:--", &lv_font_montserrat_48, kWhite,
                       LV_ALIGN_TOP_MID, 0, 139);
    // Reuse the embedded font at 150% scale: visually 72px, up from 48px.
    lv_obj_set_style_transform_scale_x(time_label, 384, 0);
    lv_obj_set_style_transform_scale_y(time_label, 384, 0);
    lv_obj_set_style_transform_pivot_x(time_label, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(time_label, LV_PCT(50), 0);
    date_label = label(screen, "Waiting for local date", &lv_font_montserrat_18, kMuted,
                       LV_ALIGN_TOP_MID, 0, 211);
    lv_obj_t *steps = lv_obj_create(screen);
    lv_obj_set_size(steps, 324, 86);
    lv_obj_align(steps, LV_ALIGN_TOP_MID, 0, 264);
    style_surface(steps);
    lv_obj_remove_flag(steps, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(steps, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *steps_title = label(steps, "Today's steps", &lv_font_montserrat_16,
                                 kMuted, LV_ALIGN_TOP_MID, 12, 12);
    lv_obj_t *steps_icon = footprints_icon(steps, 26, kMuted);
    lv_obj_align_to(steps_icon, steps_title, LV_ALIGN_OUT_LEFT_MID, -8, 0);
    steps_label = label(steps, "0", &lv_font_montserrat_26, kAccent,
                        LV_ALIGN_BOTTOM_MID, 0, -12);
    weather_icon = lv_obj_create(screen);
    lv_obj_remove_style_all(weather_icon);
    lv_obj_set_size(weather_icon, 36, 36);
    lv_obj_align(weather_icon, LV_ALIGN_TOP_MID, 102, 366);
    lv_obj_remove_flag(weather_icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(weather_icon, weather_draw_cb, LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_add_flag(weather_icon, LV_OBJ_FLAG_HIDDEN);
    weather_label = label(screen, "Madrid: unavailable", &lv_font_montserrat_18, kMuted,
                          LV_ALIGN_TOP_MID, -24, 374);
    button(screen, LV_SYMBOL_LIST "  Apps", 240, 60, LV_ALIGN_BOTTOM_MID, 0, -32,
           open_launcher_cb);
}

static void create_launcher_screen()
{
    lv_obj_t *screen = create_screen(WatchScreen::Launcher, "Apps");
    // Two large primary tiles, then a full-width connectivity/status card.
    lv_obj_t *activity = button(screen, "Activity", 154, 154, LV_ALIGN_TOP_MID,
                                -85, 112, open_activity_cb);
    lv_obj_t *settings = button(screen, "Settings", 154, 154, LV_ALIGN_TOP_MID,
                                85, 112, open_settings_cb);
    lv_obj_align(lv_obj_get_child(activity, 0), LV_ALIGN_BOTTOM_MID, 0, -24);
    lv_obj_align(lv_obj_get_child(settings, 0), LV_ALIGN_BOTTOM_MID, 0, -24);
    lv_obj_t *activity_icon = footprints_icon(activity, 36, kAccent);
    lv_obj_align(activity_icon, LV_ALIGN_TOP_MID, 0, 26);
    label(settings, LV_SYMBOL_SETTINGS, &lv_font_montserrat_26, kAccent,
          LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_t *wifi = button(screen, "", 324, 100, LV_ALIGN_TOP_MID, 0, 282, open_wifi_cb);
    label(wifi, LV_SYMBOL_WIFI "  Wi-Fi", &lv_font_montserrat_20, kWhite,
          LV_ALIGN_TOP_LEFT, 24, 16);
    launcher_wifi_label = label(wifi, "Waiting for status", &lv_font_montserrat_16,
                                kMuted, LV_ALIGN_BOTTOM_LEFT, 24, -16);
    label(wifi, LV_SYMBOL_RIGHT, &lv_font_montserrat_20, kMuted,
          LV_ALIGN_RIGHT_MID, -24, 0);
}

static void create_activity_screen()
{
    lv_obj_t *screen = create_screen(WatchScreen::Activity, "Activity");
    label(screen, "Last 7 local days", &lv_font_montserrat_16, kMuted,
          LV_ALIGN_TOP_MID, 0, 78);
    for (unsigned i = 0; i < 7; ++i) {
        const int y = 118 + i * 40;
        history_names[i] = label(screen, "--", &lv_font_montserrat_18, i ? kMuted : kWhite,
                                 LV_ALIGN_TOP_LEFT, 44, y);
        history_counts[i] = label(screen, "0", &lv_font_montserrat_20, i ? kWhite : kAccent,
                                  LV_ALIGN_TOP_RIGHT, -44, y);
    }
    activity_steps_label = history_counts[0];
}

static void create_wifi_screen()
{
    lv_obj_t *screen = create_screen(WatchScreen::Wifi, "Wi-Fi");
    label(screen, LV_SYMBOL_WIFI, &lv_font_montserrat_26, kAccent,
          LV_ALIGN_TOP_MID, 0, 130);
    wifi_detail_label = label(screen, "Waiting for status", &lv_font_montserrat_26,
                              kMuted, LV_ALIGN_TOP_MID, 0, 192);
    wifi_signal_label = label(screen, "", &lv_font_montserrat_20, kMuted,
                              LV_ALIGN_TOP_MID, 0, 244);
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
}

static void create_settings_screen()
{
    lv_obj_t *screen = create_screen(WatchScreen::Settings, "Settings");
    label(screen, "Display brightness", &lv_font_montserrat_20, kMuted,
          LV_ALIGN_TOP_LEFT, 45, 120);
    brightness_label = label(screen, "60%", &lv_font_montserrat_26, kWhite,
                             LV_ALIGN_TOP_MID, 0, 157);
    brightness_slider = lv_slider_create(screen);
    lv_obj_set_size(brightness_slider, 290, 16);
    lv_obj_align(brightness_slider, LV_ALIGN_TOP_MID, 0, 213);
    lv_obj_set_ext_click_area(brightness_slider, 20);
    lv_slider_set_range(brightness_slider, 10, 100);
    lv_slider_set_value(brightness_slider, current_brightness, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(kSurface), LV_PART_MAIN);
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(kAccent), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(kWhite), LV_PART_KNOB);
    lv_obj_add_event_cb(brightness_slider, brightness_changed_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(brightness_slider, brightness_released_cb, LV_EVENT_RELEASED, nullptr);
    label(screen, "Screen timeout", &lv_font_montserrat_20, kMuted,
          LV_ALIGN_TOP_LEFT, 45, 275);
    screen_timeout_dropdown = lv_dropdown_create(screen);
    lv_dropdown_set_options(screen_timeout_dropdown,
                            "5 seconds\n10 seconds\n30 seconds\n60 seconds\nNever");
    lv_dropdown_set_selected(screen_timeout_dropdown, 1);
    lv_obj_set_size(screen_timeout_dropdown, 320, 60);
    lv_obj_align(screen_timeout_dropdown, LV_ALIGN_TOP_MID, 0, 315);
    style_surface(screen_timeout_dropdown);
    lv_obj_set_style_pad_all(screen_timeout_dropdown, 18, 0);
    lv_obj_set_style_text_font(screen_timeout_dropdown, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(screen_timeout_dropdown, lv_color_hex(kWhite), 0);
    lv_obj_t *list = lv_dropdown_get_list(screen_timeout_dropdown);
    lv_obj_set_style_bg_color(list, lv_color_hex(kSurface), 0);
    lv_obj_set_style_text_color(list, lv_color_hex(kWhite), 0);
    lv_obj_set_style_text_font(list, &lv_font_montserrat_20, 0);
    lv_obj_set_style_pad_ver(list, 12, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x293C48), LV_PART_SELECTED);
    lv_obj_add_event_cb(screen_timeout_dropdown, timeout_changed_cb, LV_EVENT_VALUE_CHANGED, nullptr);
}

} // namespace

void watch_ui_create(void)
{
    create_home_screen();
    create_launcher_screen();
    create_activity_screen();
    create_settings_screen();
    create_wifi_screen();
    current_screen = WatchScreen::Home;
}

void watch_ui_set_settings(uint8_t brightness, uint32_t timeout_ms)
{
    uint16_t selection = 1;
    for (uint16_t i = 0; i < 5; ++i) {
        if (kTimeouts[i] == timeout_ms) { selection = i; break; }
    }
    current_brightness = brightness;
    display_timeout_ms = kTimeouts[selection];
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
    lv_label_set_text(time_label, text);
}

void watch_ui_set_date(const char *date)
{
    if (date_label != nullptr && date != nullptr) lv_label_set_text(date_label, date);
}

void watch_ui_set_steps(uint32_t steps)
{
    char text[16];
    snprintf(text, sizeof(text), "%lu", static_cast<unsigned long>(steps));
    if (steps_label != nullptr) lv_label_set_text(steps_label, text);
    if (activity_steps_label != nullptr) lv_label_set_text(activity_steps_label, text);
}

void watch_ui_set_wifi(bool connected, int rssi)
{
    watch_ui_set_wifi_state(connected ? WATCH_UI_WIFI_CONNECTED : WATCH_UI_WIFI_DISCONNECTED, rssi);
}

void watch_ui_set_wifi_state(watch_ui_wifi_state_t state, int rssi)
{
    wifi_state = state;
    const bool connected = state == WATCH_UI_WIFI_CONNECTED;
    const uint32_t color = connected ? kAccent : kMuted;
    const char *status = connected ? "Connected" : state == WATCH_UI_WIFI_CONNECTING ? "Connecting..." : "Disconnected";
    if (wifi_status_label != nullptr) {
        lv_label_set_text_fmt(wifi_status_label, LV_SYMBOL_WIFI " %s", status);
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

void watch_ui_set_weather(bool available, float temperature, watch_ui_weather_condition_t condition)
{
    if (!weather_label) return;
    if (available) {
        weather_condition = condition;
        lv_obj_remove_flag(weather_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_invalidate(weather_icon);
        char text[40];
        snprintf(text, sizeof(text), "Madrid  %.0f\xC2\xB0" "C", temperature);
        lv_label_set_text(weather_label, text);
        lv_obj_align_to(weather_icon, weather_label, LV_ALIGN_OUT_RIGHT_MID, 12, 0);
    } else {
        lv_obj_add_flag(weather_icon, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(weather_label, "Madrid: unavailable");
    }
}

void watch_ui_set_history(const watch_ui_activity_day_t days[7])
{
    static const char *weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    for (unsigned i = 0; i < 7; ++i) {
        if (!history_names[i]) return;
        if (!days) {
            lv_label_set_text(history_names[i], "Date unavailable");
            lv_label_set_text(history_counts[i], "--");
        } else {
            lv_label_set_text_fmt(history_names[i], "%s%s", i == 0 ? "(Today) " : "",
                                  weekdays[days[i].weekday % 7]);
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
    lv_obj_set_style_text_color(battery_label,
        lv_color_hex(charging ? kAccent : percentage <= 20 ? kWarning : kMuted), 0);
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
