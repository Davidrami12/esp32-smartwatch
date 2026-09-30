#include "watch_settings.h"

#include <inttypes.h>

#include "esp_log.h"
#include "nvs.h"

namespace {

constexpr char kNamespace[] = "watch_settings";
constexpr char kBrightnessKey[] = "brightness";
constexpr char kScreenTimeoutKey[] = "screen_timeout";
constexpr uint8_t kDefaultBrightness = 60;
constexpr uint32_t kDefaultScreenTimeoutMs = 10000;

nvs_handle_t settings_handle = 0;
watch_settings_t current_settings = {
    kDefaultBrightness,
    kDefaultScreenTimeoutMs
};
bool settings_open = false;
const char *TAG = "watch_settings";

bool valid_screen_timeout(uint32_t timeout_ms)
{
    return timeout_ms == 5000 ||
           timeout_ms == 10000 ||
           timeout_ms == 30000 ||
           timeout_ms == 60000 ||
           timeout_ms == 0;
}

void log_read_error(const char *key, esp_err_t error)
{
    if (error != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Could not read %s: %s", key, esp_err_to_name(error));
    }
}

} // namespace

void watch_settings_load(watch_settings_t *settings)
{
    if (settings == nullptr) {
        return;
    }

    current_settings = {kDefaultBrightness, kDefaultScreenTimeoutMs};

    esp_err_t error = nvs_open(kNamespace, NVS_READWRITE, &settings_handle);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Could not open NVS namespace: %s", esp_err_to_name(error));
        *settings = current_settings;
        return;
    }

    settings_open = true;

    uint8_t brightness = 0;
    error = nvs_get_u8(settings_handle, kBrightnessKey, &brightness);
    if (error == ESP_OK) {
        if (brightness >= 10 && brightness <= 100) {
            current_settings.brightness = brightness;
        } else {
            ESP_LOGW(TAG, "Ignoring invalid brightness value: %u", brightness);
        }
    } else {
        log_read_error(kBrightnessKey, error);
    }

    uint32_t timeout_ms = 0;
    error = nvs_get_u32(settings_handle, kScreenTimeoutKey, &timeout_ms);
    if (error == ESP_OK) {
        if (valid_screen_timeout(timeout_ms)) {
            current_settings.screen_timeout_ms = timeout_ms;
        } else {
            ESP_LOGW(TAG, "Ignoring invalid screen timeout: %" PRIu32, timeout_ms);
        }
    } else {
        log_read_error(kScreenTimeoutKey, error);
    }

    *settings = current_settings;
    ESP_LOGI(TAG, "Loaded brightness=%u timeout_ms=%" PRIu32,
             current_settings.brightness, current_settings.screen_timeout_ms);
}

esp_err_t watch_settings_save_brightness(uint8_t brightness)
{
    if (brightness < 10 || brightness > 100) {
        return ESP_ERR_INVALID_ARG;
    }
    if (brightness == current_settings.brightness) {
        return ESP_OK;
    }
    if (!settings_open) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t error = nvs_set_u8(settings_handle, kBrightnessKey, brightness);
    if (error == ESP_OK) {
        error = nvs_commit(settings_handle);
    }
    if (error == ESP_OK) {
        current_settings.brightness = brightness;
        ESP_LOGI(TAG, "Saved brightness=%u", brightness);
    } else {
        ESP_LOGE(TAG, "Could not save brightness: %s", esp_err_to_name(error));
    }
    return error;
}

esp_err_t watch_settings_save_screen_timeout(uint32_t timeout_ms)
{
    if (!valid_screen_timeout(timeout_ms)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (timeout_ms == current_settings.screen_timeout_ms) {
        return ESP_OK;
    }
    if (!settings_open) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t error = nvs_set_u32(settings_handle, kScreenTimeoutKey, timeout_ms);
    if (error == ESP_OK) {
        error = nvs_commit(settings_handle);
    }
    if (error == ESP_OK) {
        current_settings.screen_timeout_ms = timeout_ms;
        ESP_LOGI(TAG, "Saved screen timeout=%" PRIu32, timeout_ms);
    } else {
        ESP_LOGE(TAG, "Could not save screen timeout: %s", esp_err_to_name(error));
    }
    return error;
}
