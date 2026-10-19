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
constexpr uint32_t kDefaultStepGoal = 8000;
constexpr char kStepGoalKey[] = "step_goal";

nvs_handle_t settings_handle = 0;
watch_settings_t current_settings = {
    kDefaultBrightness,
    kDefaultScreenTimeoutMs,
    kDefaultStepGoal, 24, 60
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

    current_settings = {kDefaultBrightness, kDefaultScreenTimeoutMs, kDefaultStepGoal, 24, 60};

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

    uint32_t goal = 0;
    error = nvs_get_u32(settings_handle, kStepGoalKey, &goal);
    if (error == ESP_OK) {
        if (goal >= 1000 && goal <= 30000 && goal % 1000 == 0) current_settings.step_goal = goal;
        else ESP_LOGW(TAG, "Ignoring invalid step goal: %" PRIu32, goal);
    } else log_read_error(kStepGoalKey, error);

    uint8_t gain=24;
    error=nvs_get_u8(settings_handle,"mic_gain",&gain);
    if (error==ESP_OK) {
        if (gain<=42 && gain%6==0) current_settings.microphone_gain_db=gain;
        else ESP_LOGW(TAG,"Ignoring invalid microphone gain: %u",gain);
    } else log_read_error("mic_gain",error);
    uint8_t volume=60;
    error=nvs_get_u8(settings_handle,"output_volume",&volume);
    if (error==ESP_OK) {
        if (volume<=100) current_settings.output_volume=volume;
        else ESP_LOGW(TAG,"Ignoring invalid output volume: %u",volume);
    } else log_read_error("output_volume",error);
    *settings = current_settings;
    ESP_LOGI(TAG, "Loaded brightness=%u timeout_ms=%" PRIu32 " step_goal=%" PRIu32,
             current_settings.brightness, current_settings.screen_timeout_ms, current_settings.step_goal);
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

esp_err_t watch_settings_save_microphone_gain(uint8_t gain_db)
{
    if (gain_db>42 || gain_db%6) return ESP_ERR_INVALID_ARG;
    if (gain_db==current_settings.microphone_gain_db) return ESP_OK;
    if (!settings_open) return ESP_ERR_INVALID_STATE;
    esp_err_t error=nvs_set_u8(settings_handle,"mic_gain",gain_db);
    if (error==ESP_OK) error=nvs_commit(settings_handle);
    if (error==ESP_OK) current_settings.microphone_gain_db=gain_db;
    else ESP_LOGE(TAG,"Could not save microphone gain: %s",esp_err_to_name(error));
    return error;
}

esp_err_t watch_settings_save_output_volume(uint8_t percent)
{
    if (percent>100) return ESP_ERR_INVALID_ARG;
    if (percent==current_settings.output_volume) return ESP_OK;
    if (!settings_open) return ESP_ERR_INVALID_STATE;
    esp_err_t error=nvs_set_u8(settings_handle,"output_volume",percent);
    if (error==ESP_OK) error=nvs_commit(settings_handle);
    if (error==ESP_OK) current_settings.output_volume=percent;
    else ESP_LOGE(TAG,"Could not save output volume: %s",esp_err_to_name(error));
    return error;
}


esp_err_t watch_settings_save_step_goal(uint32_t goal)
{
    if (goal < 1000 || goal > 30000 || goal % 1000) return ESP_ERR_INVALID_ARG;
    if (goal == current_settings.step_goal) return ESP_OK;
    if (!settings_open) return ESP_ERR_INVALID_STATE;
    esp_err_t error = nvs_set_u32(settings_handle, kStepGoalKey, goal);
    if (error == ESP_OK) error = nvs_commit(settings_handle);
    if (error == ESP_OK) {
        current_settings.step_goal = goal;
        ESP_LOGI(TAG, "Saved step goal=%" PRIu32, goal);
    } else ESP_LOGE(TAG, "Could not save step goal: %s", esp_err_to_name(error));
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
