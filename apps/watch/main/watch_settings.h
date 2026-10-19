#pragma once

#include <stdint.h>
#include "esp_err.h"

typedef struct {
    uint8_t brightness;
    uint32_t screen_timeout_ms;
    uint32_t step_goal;
    uint8_t microphone_gain_db;
    uint8_t output_volume;
} watch_settings_t;

void watch_settings_load(watch_settings_t *settings);
esp_err_t watch_settings_save_brightness(uint8_t brightness);
esp_err_t watch_settings_save_screen_timeout(uint32_t timeout_ms);
esp_err_t watch_settings_save_step_goal(uint32_t goal);
esp_err_t watch_settings_save_microphone_gain(uint8_t gain_db);
esp_err_t watch_settings_save_output_volume(uint8_t percent);
