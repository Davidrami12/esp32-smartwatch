#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool available;
    uint32_t date; // Madrid YYYYMMDD.
    uint8_t hour, condition; // 08/14/20; same condition order as watch_ui.
    float temperature_c, precipitation_mm; // Precipitation in the hour ending at this time.
} watch_weather_slot_t;

typedef struct {
    bool available, rain_available;
    uint8_t condition;
    uint32_t observation_date;
    float temperature_c, wind_kmh, wind_degrees, humidity_percent, rain_mm;
    int64_t fetched_monotonic_us;
    watch_weather_slot_t slots[9];
} watch_weather_dashboard_t;
