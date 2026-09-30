#pragma once

#include <stdint.h>
#include "esp_err.h"

typedef struct {
    uint8_t brightness;
    uint32_t screen_timeout_ms;
} watch_settings_t;

void watch_settings_load(watch_settings_t *settings);
esp_err_t watch_settings_save_brightness(uint8_t brightness);
esp_err_t watch_settings_save_screen_timeout(uint32_t timeout_ms);
