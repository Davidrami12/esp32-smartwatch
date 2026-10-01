#pragma once

#include "esp_err.h"
#include <stdint.h>

struct watch_activity_day_t {
    uint32_t date; // local YYYYMMDD
    uint32_t steps;
    uint8_t weekday; // Sunday=0; derived from the local calendar, not elapsed 24h periods
};
// Newest first; false while the system date is not trustworthy. Missing days are zero.
bool watch_activity_get_history(watch_activity_day_t days[7]);

// Restores today's count when system time is valid, then starts rollover/save monitoring.
// An initialization/storage error does not stop live step counting.
esp_err_t watch_activity_init(void);
