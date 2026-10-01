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
