#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t watch_power_init(void);
bool watch_power_is_active(void);
void watch_power_set_active(bool active, uint8_t brightness);
