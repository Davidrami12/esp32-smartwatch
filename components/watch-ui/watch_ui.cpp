#include "watch_ui.h"

#include <stdio.h>
#include <stdint.h>

static lv_obj_t *home_screen = NULL;
static lv_obj_t *settings_screen = NULL;

static lv_obj_t *time_label = NULL;
static lv_obj_t *date_label = NULL;
static lv_obj_t *wifi_status_label = NULL;

static lv_obj_t *battery_label = NULL;
static lv_obj_t *charging_label = NULL;
static lv_obj_t *battery_icon_body = NULL;
static lv_obj_t *battery_icon_level = NULL;
static lv_obj_t *battery_icon_tip = NULL;

static lv_obj_t *brightness_label = NULL;
static lv_obj_t *brightness_slider = NULL;
static lv_obj_t *screen_timeout_dropdown = NULL;

static uint8_t current_brightness = 60;
static uint32_t display_timeout_ms = 10000;

static watch_ui_brightness_cb_t brightness_callback = NULL;
static watch_ui_brightness_committed_cb_t brightness_committed_callback = NULL;
static watch_ui_timeout_cb_t timeout_callback = NULL;

enum class WatchScreen {
    Home,
    Settings
};

static WatchScreen current_screen = WatchScreen::Home;

/* ---------------- NAVIGATION ---------------- */

static void navigate_to(WatchScreen screen)
{
    if (screen == current_screen) {
        return;
    }

    if (screen == WatchScreen::Settings) {
        lv_screen_load_anim(
            settings_screen,
            LV_SCR_LOAD_ANIM_MOVE_LEFT,
            250,
            0,
            false
        );
    }
    else {
        lv_screen_load_anim(
            home_screen,
            LV_SCR_LOAD_ANIM_MOVE_RIGHT,
            250,
            0,
            false
        );
    }

    current_screen = screen;
}

static void open_settings_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    navigate_to(WatchScreen::Settings);
}

static void back_home_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    navigate_to(WatchScreen::Home);
}

static void navigation_gesture_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_GESTURE) {
        return;
    }

    lv_indev_t *indev = lv_indev_active();

    if (indev == NULL) {
        return;
    }

    lv_dir_t direction =
        lv_indev_get_gesture_dir(indev);

    if (
        current_screen == WatchScreen::Home &&
        direction == LV_DIR_LEFT
    ) {
        navigate_to(WatchScreen::Settings);
    }
    else if (
        current_screen == WatchScreen::Settings &&
        direction == LV_DIR_RIGHT
    ) {
        navigate_to(WatchScreen::Home);
    }
}

/* ---------------- SETTINGS ---------------- */

static void brightness_slider_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    lv_obj_t *slider =
        static_cast<lv_obj_t *>(
            lv_event_get_target(event)
        );

    current_brightness =
        lv_slider_get_value(slider);

    char buffer[16];

    snprintf(
        buffer,
        sizeof(buffer),
        "%d%%",
        current_brightness
    );

    lv_label_set_text(
        brightness_label,
        buffer
    );

    /*
     * Desktop simulator only updates the UI.
     * Hardware brightness will be connected later.
     */

     if (brightness_callback != NULL) {
        brightness_callback(current_brightness);
    }
}

static void screen_timeout_dropdown_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    lv_obj_t *dropdown =
        static_cast<lv_obj_t *>(
            lv_event_get_target(event)
        );

    uint32_t selected =
        lv_dropdown_get_selected(dropdown);

    switch (selected) {
        case 0:
            display_timeout_ms = 5000;
            break;

        case 1:
            display_timeout_ms = 10000;
            break;

        case 2:
            display_timeout_ms = 30000;
            break;

        case 3:
            display_timeout_ms = 60000;
            break;

        case 4:
            display_timeout_ms = 0;
            break;

        default:
            display_timeout_ms = 10000;
            break;
    }

    printf(
        "Screen timeout: %u ms\n",
        display_timeout_ms
    );

    if (timeout_callback != NULL) {
        timeout_callback(display_timeout_ms);
    }
}

static void brightness_slider_released_cb(lv_event_t *event)
{
    if (
        lv_event_get_code(event) == LV_EVENT_RELEASED &&
        brightness_committed_callback != NULL
    ) {
        brightness_committed_callback(current_brightness);
    }
}

/* ---------------- HOME ---------------- */

static void create_home_screen(void)
{
    home_screen = lv_screen_active();

    lv_obj_set_style_bg_color(
        home_screen,
        lv_color_hex(0x000000),
        LV_PART_MAIN
    );

    lv_obj_add_flag(
        home_screen,
        LV_OBJ_FLAG_CLICKABLE
    );

    lv_obj_add_event_cb(
        home_screen,
        navigation_gesture_cb,
        LV_EVENT_GESTURE,
        NULL
    );

    /* Time */

    time_label =
        lv_label_create(home_screen);

    lv_label_set_text(
        time_label,
        "--:--:--"
    );

    lv_obj_set_style_text_color(
        time_label,
        lv_color_hex(0xFFFFFF),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_font(
        time_label,
        &lv_font_montserrat_26,
        LV_PART_MAIN
    );

    lv_obj_align(
        time_label,
        LV_ALIGN_TOP_MID,
        0,
        20
    );

    /* Date */

    date_label =
        lv_label_create(home_screen);

    lv_label_set_text(
        date_label,
        "--/--/----"
    );

    lv_obj_set_style_text_color(
        date_label,
        lv_color_hex(0xAAAAAA),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_font(
        date_label,
        &lv_font_montserrat_26,
        LV_PART_MAIN
    );

    lv_obj_align(
        date_label,
        LV_ALIGN_TOP_MID,
        0,
        60
    );

    /* Wi-Fi */

    wifi_status_label =
        lv_label_create(home_screen);

    lv_label_set_text(
        wifi_status_label,
        "WiFi Status: Connecting..."
    );

    lv_obj_set_style_text_color(
        wifi_status_label,
        lv_color_hex(0xAAAAAA),
        LV_PART_MAIN
    );

    lv_obj_align(
        wifi_status_label,
        LV_ALIGN_TOP_MID,
        0,
        105
    );

    /* Battery body */

    battery_icon_body =
        lv_obj_create(home_screen);

    lv_obj_remove_style_all(
        battery_icon_body
    );

    lv_obj_set_size(
        battery_icon_body,
        56,
        30
    );

    lv_obj_set_style_border_width(
        battery_icon_body,
        3,
        LV_PART_MAIN
    );

    lv_obj_set_style_border_color(
        battery_icon_body,
        lv_color_hex(0xFFFFFF),
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        battery_icon_body,
        5,
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        battery_icon_body,
        LV_OPA_TRANSP,
        LV_PART_MAIN
    );

    lv_obj_align(
        battery_icon_body,
        LV_ALIGN_CENTER,
        -45,
        -10
    );

    /* Battery level */

    battery_icon_level =
        lv_obj_create(battery_icon_body);

    lv_obj_remove_style_all(
        battery_icon_level
    );

    lv_obj_set_height(
        battery_icon_level,
        20
    );

    lv_obj_set_width(
        battery_icon_level,
        0
    );

    lv_obj_set_style_bg_opa(
        battery_icon_level,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        battery_icon_level,
        2,
        LV_PART_MAIN
    );

    lv_obj_align(
        battery_icon_level,
        LV_ALIGN_LEFT_MID,
        5,
        0
    );

    /* Battery tip */

    battery_icon_tip =
        lv_obj_create(home_screen);

    lv_obj_remove_style_all(
        battery_icon_tip
    );

    lv_obj_set_size(
        battery_icon_tip,
        5,
        14
    );

    lv_obj_set_style_bg_color(
        battery_icon_tip,
        lv_color_hex(0xFFFFFF),
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        battery_icon_tip,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        battery_icon_tip,
        2,
        LV_PART_MAIN
    );

    lv_obj_align_to(
        battery_icon_tip,
        battery_icon_body,
        LV_ALIGN_OUT_RIGHT_MID,
        2,
        0
    );

    /* Battery percentage */

    battery_label =
        lv_label_create(home_screen);

    lv_label_set_text(
        battery_label,
        "--%"
    );

    lv_obj_set_style_text_color(
        battery_label,
        lv_color_hex(0xFFFFFF),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_font(
        battery_label,
        &lv_font_montserrat_26,
        LV_PART_MAIN
    );

    lv_obj_align(
        battery_label,
        LV_ALIGN_CENTER,
        40,
        -10
    );

    /* Charging status */

    charging_label =
        lv_label_create(home_screen);

    lv_label_set_text(
        charging_label,
        ""
    );

    lv_obj_set_style_text_color(
        charging_label,
        lv_color_hex(0xAAAAAA),
        LV_PART_MAIN
    );

    lv_obj_align(
        charging_label,
        LV_ALIGN_CENTER,
        0,
        30
    );

    /* Settings button */

    lv_obj_t *settings_button =
        lv_button_create(home_screen);

    lv_obj_set_size(
        settings_button,
        150,
        50
    );

    lv_obj_align(
        settings_button,
        LV_ALIGN_BOTTOM_MID,
        0,
        -70
    );

    lv_obj_add_event_cb(
        settings_button,
        open_settings_cb,
        LV_EVENT_CLICKED,
        NULL
    );

    lv_obj_t *settings_button_label =
        lv_label_create(settings_button);

    lv_label_set_text(
        settings_button_label,
        "Settings"
    );

    lv_obj_center(
        settings_button_label
    );
}

/* ---------------- SETTINGS SCREEN ---------------- */

static void create_settings_screen(void)
{
    settings_screen =
        lv_obj_create(NULL);

    lv_obj_set_style_bg_color(
        settings_screen,
        lv_color_hex(0x000000),
        LV_PART_MAIN
    );

    lv_obj_add_flag(
        settings_screen,
        LV_OBJ_FLAG_CLICKABLE
    );

    lv_obj_add_event_cb(
        settings_screen,
        navigation_gesture_cb,
        LV_EVENT_GESTURE,
        NULL
    );

    /* Settings title */

    lv_obj_t *settings_title =
        lv_label_create(settings_screen);

    lv_label_set_text(
        settings_title,
        "Settings"
    );

    lv_obj_set_style_text_color(
        settings_title,
        lv_color_hex(0xFFFFFF),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_font(
        settings_title,
        &lv_font_montserrat_26,
        LV_PART_MAIN
    );

    lv_obj_align(
        settings_title,
        LV_ALIGN_TOP_MID,
        0,
        30
    );

    /* Brightness title */

    lv_obj_t *brightness_title =
        lv_label_create(settings_screen);

    lv_label_set_text(
        brightness_title,
        "Brightness control"
    );

    lv_obj_set_style_text_color(
        brightness_title,
        lv_color_hex(0xFFFFFF),
        LV_PART_MAIN
    );

    lv_obj_align(
        brightness_title,
        LV_ALIGN_CENTER,
        0,
        -70
    );

    /* Brightness value */

    brightness_label =
        lv_label_create(settings_screen);

    char brightness_buffer[16];

    snprintf(
        brightness_buffer,
        sizeof(brightness_buffer),
        "%d%%",
        current_brightness
    );

    lv_label_set_text(
        brightness_label,
        brightness_buffer
    );

    lv_obj_set_style_text_color(
        brightness_label,
        lv_color_hex(0xFFFFFF),
        LV_PART_MAIN
    );

    lv_obj_align(
        brightness_label,
        LV_ALIGN_CENTER,
        0,
        -30
    );

    /* Brightness slider */

    brightness_slider =
        lv_slider_create(settings_screen);

    lv_slider_set_range(
        brightness_slider,
        10,
        100
    );

    lv_slider_set_value(
        brightness_slider,
        current_brightness,
        LV_ANIM_OFF
    );

    lv_obj_set_width(
        brightness_slider,
        220
    );

    lv_obj_align(
        brightness_slider,
        LV_ALIGN_CENTER,
        0,
        20
    );

    lv_obj_add_event_cb(
        brightness_slider,
        brightness_slider_cb,
        LV_EVENT_VALUE_CHANGED,
        NULL
    );

    lv_obj_add_event_cb(
        brightness_slider,
        brightness_slider_released_cb,
        LV_EVENT_RELEASED,
        NULL
    );

    /* Screen timeout title */

    lv_obj_t *screen_timeout_title =
        lv_label_create(settings_screen);

    lv_label_set_text(
        screen_timeout_title,
        "Screen timeout"
    );

    lv_obj_set_style_text_color(
        screen_timeout_title,
        lv_color_hex(0xFFFFFF),
        LV_PART_MAIN
    );

    lv_obj_align(
        screen_timeout_title,
        LV_ALIGN_CENTER,
        0,
        80
    );

    /* Screen timeout dropdown */

    screen_timeout_dropdown =
        lv_dropdown_create(settings_screen);

    lv_dropdown_set_options(
        screen_timeout_dropdown,
        "5 seconds\n"
        "10 seconds\n"
        "30 seconds\n"
        "60 seconds\n"
        "Never"
    );

    lv_dropdown_set_selected(
        screen_timeout_dropdown,
        1
    );

    lv_obj_set_width(
        screen_timeout_dropdown,
        180
    );

    lv_obj_align(
        screen_timeout_dropdown,
        LV_ALIGN_CENTER,
        0,
        125
    );

    lv_obj_add_event_cb(
        screen_timeout_dropdown,
        screen_timeout_dropdown_cb,
        LV_EVENT_VALUE_CHANGED,
        NULL
    );

    /* Back button */

    lv_obj_t *back_button =
        lv_button_create(settings_screen);

    lv_obj_set_size(
        back_button,
        130,
        50
    );

    lv_obj_align(
        back_button,
        LV_ALIGN_BOTTOM_MID,
        0,
        -55
    );

    lv_obj_add_event_cb(
        back_button,
        back_home_cb,
        LV_EVENT_CLICKED,
        NULL
    );

    lv_obj_t *back_button_label =
        lv_label_create(back_button);

    lv_label_set_text(
        back_button_label,
        "Back"
    );

    lv_obj_center(
        back_button_label
    );
}

/* ---------------- PUBLIC API ---------------- */

void watch_ui_create(void)
{
    create_home_screen();
    create_settings_screen();

    current_screen =
        WatchScreen::Home;
}

void watch_ui_set_settings(
    uint8_t brightness,
    uint32_t timeout_ms
)
{
    uint16_t timeout_selection = 1;

    current_brightness = brightness;

    switch (timeout_ms) {
        case 5000: timeout_selection = 0; break;
        case 10000: timeout_selection = 1; break;
        case 30000: timeout_selection = 2; break;
        case 60000: timeout_selection = 3; break;
        case 0: timeout_selection = 4; break;
        default: timeout_ms = 10000; break;
    }

    display_timeout_ms = timeout_ms;

    if (brightness_slider != NULL) {
        lv_slider_set_value(brightness_slider, current_brightness, LV_ANIM_OFF);
    }

    if (brightness_label != NULL) {
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%u%%", current_brightness);
        lv_label_set_text(brightness_label, buffer);
    }

    if (screen_timeout_dropdown != NULL) {
        lv_dropdown_set_selected(screen_timeout_dropdown, timeout_selection);
    }
}

void watch_ui_set_time(const char *time)
{
    if (time_label != NULL) {
        lv_label_set_text(
            time_label,
            time
        );
    }
}

void watch_ui_set_date(const char *date)
{
    if (date_label != NULL) {
        lv_label_set_text(
            date_label,
            date
        );
    }
}

void watch_ui_set_wifi(
    bool connected,
    int rssi
)
{
    if (wifi_status_label == NULL) {
        return;
    }

    if (!connected) {
        lv_label_set_text(
            wifi_status_label,
            "WiFi Status: Disconnected"
        );

        return;
    }

    if (rssi == 0) {
        lv_label_set_text(
            wifi_status_label,
            "WiFi Status: Connected"
        );

        return;
    }

    char buffer[64];

    snprintf(
        buffer,
        sizeof(buffer),
        "WiFi Status: Connected (%d dBm)",
        rssi
    );

    lv_label_set_text(
        wifi_status_label,
        buffer
    );
}

void watch_ui_set_battery(
    int percentage,
    bool charging
)
{
    if (
        battery_label == NULL ||
        battery_icon_level == NULL
    ) {
        return;
    }

    /* Battery not available */
    if (percentage < 0) {
        lv_label_set_text(
            battery_label,
            "--%"
        );

        lv_obj_set_width(
            battery_icon_level,
            0
        );

        lv_obj_set_style_text_color(
            battery_label,
            lv_color_hex(0xAAAAAA),
            LV_PART_MAIN
        );

        lv_label_set_text(
            charging_label,
            "No battery"
        );

        return;
    }

    if (percentage > 100) {
        percentage = 100;
    }

    char buffer[16];

    snprintf(
        buffer,
        sizeof(buffer),
        "%d%%",
        percentage
    );

    lv_label_set_text(
        battery_label,
        buffer
    );

    int fill_width =
        (46 * percentage) / 100;

    lv_obj_set_width(
        battery_icon_level,
        fill_width
    );

    lv_color_t battery_color;

    if (charging) {
        battery_color =
            lv_color_hex(0x00C853);
    }
    else if (percentage <= 20) {
        battery_color =
            lv_color_hex(0xF44336);
    }
    else if (percentage <= 50) {
        battery_color =
            lv_color_hex(0xFF9800);
    }
    else if (percentage <= 70) {
        battery_color =
            lv_color_hex(0xFFD600);
    }
    else {
        battery_color =
            lv_color_hex(0x00C853);
    }

    lv_obj_set_style_bg_color(
        battery_icon_level,
        battery_color,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        battery_label,
        battery_color,
        LV_PART_MAIN
    );

    lv_label_set_text(
        charging_label,
        charging
            ? "Charging"
            : ""
    );
}

void watch_ui_set_brightness_callback(
    watch_ui_brightness_cb_t callback
)
{
    brightness_callback = callback;
}

void watch_ui_set_timeout_callback(
    watch_ui_timeout_cb_t callback
)
{
    timeout_callback = callback;
}

void watch_ui_set_brightness_committed_callback(
    watch_ui_brightness_committed_cb_t callback
)
{
    brightness_committed_callback = callback;
}
