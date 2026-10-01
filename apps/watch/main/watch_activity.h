#pragma once

#include "esp_err.h"

// Restores today's count when system time is valid, then starts rollover/save monitoring.
// An initialization/storage error does not stop live step counting.
esp_err_t watch_activity_init(void);
