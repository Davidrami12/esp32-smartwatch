#pragma once
#include "esp_err.h"
#include <stdint.h>

enum class watch_weather_condition_t { Sunny, Cloudy, Rain, Thunderstorm, Windy };
struct watch_weather_t {
    bool available;
    float temperature_c;
    watch_weather_condition_t condition;
};
esp_err_t watch_weather_init(void);
watch_weather_t watch_weather_get(void);
struct watch_weather_day_t {
    uint32_t date; // Madrid-local YYYYMMDD, oldest first.
    float low_c, high_c;
    watch_weather_condition_t condition;
};
struct watch_weather_forecast_t {
    bool available;
    int64_t fetched_epoch;
    int64_t fetched_monotonic_us;
    watch_weather_day_t days[7];
};
watch_weather_forecast_t watch_weather_forecast_get(void);
