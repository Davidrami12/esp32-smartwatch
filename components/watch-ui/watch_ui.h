#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*watch_ui_brightness_cb_t)(uint8_t brightness);
typedef void (*watch_ui_brightness_committed_cb_t)(uint8_t brightness);
typedef void (*watch_ui_timeout_cb_t)(uint32_t timeout_ms);

void watch_ui_set_brightness_callback(
    watch_ui_brightness_cb_t callback
);

void watch_ui_set_timeout_callback(
    watch_ui_timeout_cb_t callback
);

void watch_ui_set_brightness_committed_callback(
    watch_ui_brightness_committed_cb_t callback
);

void watch_ui_create(void);
void watch_ui_set_settings(
    uint8_t brightness,
    uint32_t timeout_ms
);

void watch_ui_set_time(const char *time);
void watch_ui_set_date(const char *date);
void watch_ui_set_steps(uint32_t steps);

typedef enum {
    WATCH_UI_WIFI_DISCONNECTED, WATCH_UI_WIFI_CONNECTING, WATCH_UI_WIFI_CONNECTED
} watch_ui_wifi_state_t;
typedef bool (*watch_ui_wifi_control_cb_t)(bool enabled);
void watch_ui_set_wifi_state(watch_ui_wifi_state_t state, int rssi);
void watch_ui_set_wifi_control_callback(watch_ui_wifi_control_cb_t callback);

typedef enum {
    WATCH_UI_WEATHER_SUNNY, WATCH_UI_WEATHER_CLOUDY, WATCH_UI_WEATHER_RAIN,
    WATCH_UI_WEATHER_THUNDERSTORM, WATCH_UI_WEATHER_WINDY
} watch_ui_weather_condition_t;
void watch_ui_set_weather(bool available, float temperature_c, watch_ui_weather_condition_t condition);
typedef struct { uint32_t steps; uint8_t weekday; } watch_ui_activity_day_t;
// Seven newest-first calendar rows supplied by firmware; NULL means date unavailable.
void watch_ui_set_history(const watch_ui_activity_day_t days[7]);

void watch_ui_set_wifi(
    bool connected,
    int rssi
);

void watch_ui_set_battery(
    int percentage,
    bool charging
);

#ifdef __cplusplus
}
#endif
