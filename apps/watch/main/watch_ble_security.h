#pragma once
#include <stdint.h>
#include "esp_err.h"
struct ble_gap_event;
enum class watch_pairing_state_t { Unpaired, Comparing, Securing, Trusted, Rejected, Failed, Forgetting, Forgotten, Locked };
struct watch_pairing_status_t {
    watch_pairing_state_t state;
    uint32_t token;
    uint32_t code;
    bool has_phone;
};
esp_err_t watch_ble_security_init();
void watch_ble_security_gap(ble_gap_event *event);
bool watch_ble_security_ready(uint16_t handle);
watch_pairing_status_t watch_ble_security_status();
bool watch_ble_security_confirm(uint32_t token, bool accept);
bool watch_ble_security_forget();
void watch_ble_security_reset();
