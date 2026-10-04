#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "watch_ble_control_protocol.h"

enum class watch_ble_state_t { Disabled, Advertising, Connected };
esp_err_t watch_ble_init(void);
watch_ble_state_t watch_ble_get_state(void);
// Runtime radio intent (defaults enabled each boot); disabling disconnects the peer.
bool watch_ble_set_enabled(bool enabled);
// Register before init. Called on NimBLE host: platform serializes settings/UI access.
// ESP_OK means persistence/application completed, NOT merely queued.
typedef esp_err_t (*watch_ble_control_cb_t)(watch_ble_control_t control, uint32_t value);
void watch_ble_set_control_callback(watch_ble_control_cb_t callback);
void watch_ble_update_settings(uint8_t brightness, uint32_t timeout_ms);

// Thread-safe data inputs only. PMIC/activity/network acquisition remains elsewhere.
// Safe before init; notifications are dispatched on the NimBLE host event queue.
void watch_ble_update_battery(int percentage); // Negative means unavailable.
void watch_ble_update_steps(uint32_t steps);
void watch_ble_update_wifi(uint8_t state); // 0 disconnected, 1 connecting, 2 connected, 3 disabled.
