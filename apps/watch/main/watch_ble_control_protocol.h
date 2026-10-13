#pragma once
#include <stddef.h>
#include <stdint.h>

// Portable wire validation, deliberately outside the shared UI.
enum class watch_ble_control_t { Brightness, ScreenTimeout, WifiEnabled };
inline size_t watch_ble_control_length(watch_ble_control_t control)
{
    return control == watch_ble_control_t::ScreenTimeout ? 4 : 1;
}
inline bool watch_ble_decode_control(watch_ble_control_t control, const uint8_t *bytes, size_t length, uint32_t &value)
{
    if (!bytes || length != watch_ble_control_length(control)) return false;
    value = 0;
    for (size_t i = 0; i < length; ++i) value |= static_cast<uint32_t>(bytes[i]) << (8 * i);
    switch (control) {
        case watch_ble_control_t::Brightness: return value >= 10 && value <= 100;
        case watch_ble_control_t::ScreenTimeout:
            return value == 0 || value == 5000 || value == 10000 || value == 30000 || value == 60000;
        case watch_ble_control_t::WifiEnabled: return value <= 1;
    }
    return false;
}
