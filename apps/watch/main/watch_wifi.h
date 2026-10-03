#pragma once
#include "esp_err.h"

enum class watch_wifi_state_t { Disconnected, Connecting, Connected };
esp_err_t watch_wifi_init(void);
// Non-blocking UI request; networking and persistence run on the connectivity worker.
bool watch_wifi_request_enabled(bool enabled);
watch_wifi_state_t watch_wifi_get_state(void);
int watch_wifi_get_rssi(void);
bool watch_wifi_is_enabled(void);
