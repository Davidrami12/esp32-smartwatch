#pragma once
#include "esp_err.h"

enum class watch_weather_condition_t { Sunny, Cloudy, Rain, Thunderstorm, Windy };
struct watch_weather_t {
    bool available;
    float temperature_c;
    watch_weather_condition_t condition;
};
esp_err_t watch_weather_init(void);
watch_weather_t watch_weather_get(void);
