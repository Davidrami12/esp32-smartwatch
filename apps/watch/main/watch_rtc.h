#pragma once

#include <stdbool.h>
#include <time.h>
#include "esp_err.h"

// The PCF85063 stores UTC; the system TZ converts UTC for local display.
esp_err_t watch_rtc_init(void);
bool watch_rtc_read_utc(time_t *timestamp);
esp_err_t watch_rtc_write_utc(time_t timestamp);
