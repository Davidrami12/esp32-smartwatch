#include "watch_weather.h"
#include "watch_wifi.h"

#include <math.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {
constexpr char kTag[] = "watch_weather";
constexpr char kUrl[] = "https://api.open-meteo.com/v1/forecast?latitude=40.4168&longitude=-3.7038"
    "&current=temperature_2m,weather_code,wind_speed_10m"
    "&daily=weather_code,temperature_2m_max,temperature_2m_min,wind_speed_10m_max"
    "&temperature_unit=celsius&wind_speed_unit=kmh&timezone=Europe%2FMadrid&forecast_days=7";
constexpr int64_t kRefreshUs = 30LL * 60 * 1000000;
constexpr int64_t kRetryUs = 5LL * 60 * 1000000;
constexpr float kWindyKmh = 30.0f; // Sustained 10m wind, km/h. Rain/thunder take priority.
portMUX_TYPE cache_lock = portMUX_INITIALIZER_UNLOCKED;
watch_weather_t cached = {};
watch_weather_forecast_t forecast_cached = {};
struct Response { char text[4096]; size_t length; bool overflow; };

esp_err_t response_event(esp_http_client_event_t *event)
{
    auto *response = static_cast<Response *>(event->user_data);
    if (event->event_id == HTTP_EVENT_ON_DATA && event->data_len > 0) {
        if (response->length + event->data_len >= sizeof(response->text)) {
            response->overflow = true;
            return ESP_FAIL;
        }
        memcpy(response->text + response->length, event->data, event->data_len);
        response->length += event->data_len;
        response->text[response->length] = '\0';
    }
    return ESP_OK;
}

watch_weather_condition_t classify(int code, float wind)
{
    if (code >= 95 && code <= 99) return watch_weather_condition_t::Thunderstorm;
    // Snow is folded into the precipitation category in this deliberately small icon set.
    if ((code >= 51 && code <= 67) || (code >= 71 && code <= 86)) return watch_weather_condition_t::Rain;
    if (wind >= kWindyKmh) return watch_weather_condition_t::Windy;
    return code <= 1 ? watch_weather_condition_t::Sunny : watch_weather_condition_t::Cloudy;
}

bool valid_number(const cJSON *value, double low, double high)
{
    return cJSON_IsNumber(value) && isfinite(value->valuedouble) && value->valuedouble >= low && value->valuedouble <= high;
}

bool parse_forecast(cJSON *root, watch_weather_forecast_t &result)
{
    cJSON *daily = cJSON_GetObjectItemCaseSensitive(root, "daily");
    const char *names[] = {"time", "temperature_2m_min", "temperature_2m_max", "weather_code", "wind_speed_10m_max"};
    cJSON *arrays[5];
    for (unsigned i = 0; i < 5; ++i) {
        arrays[i] = cJSON_GetObjectItemCaseSensitive(daily, names[i]);
        if (!cJSON_IsArray(arrays[i]) || cJSON_GetArraySize(arrays[i]) != 7) return false;
    }
    uint32_t expected = 0;
    for (unsigned i = 0; i < 7; ++i) {
        cJSON *date = cJSON_GetArrayItem(arrays[0], i);
        cJSON *low = cJSON_GetArrayItem(arrays[1], i), *high = cJSON_GetArrayItem(arrays[2], i);
        cJSON *code = cJSON_GetArrayItem(arrays[3], i), *wind = cJSON_GetArrayItem(arrays[4], i);
        if (!cJSON_IsString(date) || !date->valuestring || strlen(date->valuestring) != 10 ||
            !valid_number(low, -100, 70) || !valid_number(high, -100, 70) || low->valuedouble > high->valuedouble ||
            !valid_number(code, 0, 99) || code->valuedouble != code->valueint || !valid_number(wind, 0, 500)) return false;
        int year, month, day;
        for (unsigned j = 0; j < 10; ++j) {
            if (j != 4 && j != 7 && !isdigit(static_cast<unsigned char>(date->valuestring[j]))) return false;
        }
        if (sscanf(date->valuestring, "%4d-%2d-%2d", &year, &month, &day) != 3 ||
            date->valuestring[4] != '-' || date->valuestring[7] != '-' ||
            year < 2020 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31) return false;
        tm local = {};
        local.tm_year = year - 1900; local.tm_mon = month - 1; local.tm_mday = day;
        local.tm_hour = 12; local.tm_isdst = -1;
        if (mktime(&local) == static_cast<time_t>(-1) || local.tm_year != year - 1900 ||
            local.tm_mon != month - 1 || local.tm_mday != day) return false;
        const uint32_t key = year * 10000 + month * 100 + day;
        if (i && key != expected) return false;
        ++local.tm_mday; local.tm_isdst = -1;
        if (mktime(&local) == static_cast<time_t>(-1)) return false;
        expected = (local.tm_year + 1900) * 10000 + (local.tm_mon + 1) * 100 + local.tm_mday;
        result.days[i] = {key, static_cast<float>(low->valuedouble), static_cast<float>(high->valuedouble),
                          classify(code->valueint, static_cast<float>(wind->valuedouble))};
    }
    result.available = true;
    result.fetched_epoch = time(nullptr);
    result.fetched_monotonic_us = esp_timer_get_time();
    return true;
}

bool fetch()
{
    Response response = {};
    esp_http_client_config_t config = {};
    config.url = kUrl;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = 10000;
    config.event_handler = response_event;
    config.user_data = &response;
    auto client = esp_http_client_init(&config);
    if (!client) return false;
    esp_err_t error = esp_http_client_perform(client);
    const int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (error != ESP_OK || status != 200 || response.overflow) {
        ESP_LOGW(kTag, "Fetch failed: %s HTTP=%d overflow=%d", esp_err_to_name(error), status, response.overflow);
        return false;
    }
    cJSON *root = cJSON_Parse(response.text);
    if (!root) { ESP_LOGW(kTag, "Invalid JSON; retaining cache"); return false; }
    cJSON *current = cJSON_GetObjectItemCaseSensitive(root, "current");
    cJSON *temperature = cJSON_GetObjectItemCaseSensitive(current, "temperature_2m");
    cJSON *code = cJSON_GetObjectItemCaseSensitive(current, "weather_code");
    cJSON *wind = cJSON_GetObjectItemCaseSensitive(current, "wind_speed_10m");
    const bool valid = cJSON_IsNumber(temperature) && cJSON_IsNumber(code) && cJSON_IsNumber(wind) &&
        isfinite(temperature->valuedouble) && temperature->valuedouble >= -100 && temperature->valuedouble <= 70 &&
        isfinite(code->valuedouble) && code->valuedouble >= 0 && code->valuedouble <= 99 &&
        code->valuedouble == code->valueint && isfinite(wind->valuedouble) && wind->valuedouble >= 0;
    watch_weather_forecast_t forecast = {};
    const bool forecast_valid = parse_forecast(root, forecast);
    if (valid) {
        watch_weather_t result = {true, static_cast<float>(temperature->valuedouble),
            classify(code->valueint, static_cast<float>(wind->valuedouble))};
        portENTER_CRITICAL(&cache_lock);
        cached = result;
        if (forecast_valid) forecast_cached = forecast;
        portEXIT_CRITICAL(&cache_lock);
        ESP_LOGI(kTag, "Madrid: %.1f C, WMO=%d, wind=%.1f km/h", result.temperature_c, code->valueint, wind->valuedouble);
    } else ESP_LOGW(kTag, "Invalid current-weather JSON; retaining cache");
    if (forecast_valid && !valid) {
        portENTER_CRITICAL(&cache_lock);
        forecast_cached = forecast;
        portEXIT_CRITICAL(&cache_lock);
    }
    if (forecast_valid) ESP_LOGI(kTag, "Seven-day Madrid forecast cached: %lu through %lu",
        static_cast<unsigned long>(forecast.days[0].date), static_cast<unsigned long>(forecast.days[6].date));
    else ESP_LOGW(kTag, "Invalid daily forecast; retaining cache");
    cJSON_Delete(root);
    return valid && forecast_valid;
}

void worker(void *)
{
    bool was_connected = false;
    int64_t due = 0;
    int64_t last_attempt = -kRetryUs;
    while (true) {
        const bool online = watch_wifi_get_state() == watch_wifi_state_t::Connected;
        const int64_t now = esp_timer_get_time();
        // Reconnect refresh is rate-limited too, so a flapping AP cannot hammer the API.
        if (online && !was_connected) due = now;
        was_connected = online;
        if (online && time(nullptr) >= 946684800 && now >= due && now - last_attempt >= kRetryUs) {
            last_attempt = now;
            const bool success = fetch();
            due = esp_timer_get_time() + (success ? kRefreshUs : kRetryUs);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
} // namespace

esp_err_t watch_weather_init(void)
{
    return xTaskCreate(worker, "madrid_weather", 12288, nullptr, 3, nullptr) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

watch_weather_t watch_weather_get(void)
{
    portENTER_CRITICAL(&cache_lock);
    const watch_weather_t result = cached;
    portEXIT_CRITICAL(&cache_lock);
    return result;
}

watch_weather_forecast_t watch_weather_forecast_get(void)
{
    portENTER_CRITICAL(&cache_lock);
    const auto result = forecast_cached;
    portEXIT_CRITICAL(&cache_lock);
    return result;
}
