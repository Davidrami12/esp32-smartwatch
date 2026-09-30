#include "watch_power.h"

#include "esp_log.h"
#include "esp_pm.h"
#include "bsp/display.h"
#include "sdkconfig.h"

namespace {
constexpr char kTag[] = "watch_power";
bool active = true;
}

esp_err_t watch_power_init(void)
{
    esp_pm_config_t config = {
        .max_freq_mhz = 240,
        .min_freq_mhz = CONFIG_XTAL_FREQ,
        .light_sleep_enable = false
    };
    esp_err_t error = esp_pm_configure(&config);
    if (error == ESP_OK) {
        ESP_LOGI(kTag, "DFS configured: CPU %d-%d MHz; automatic light sleep disabled",
                 CONFIG_XTAL_FREQ, 240);
    } else {
        ESP_LOGE(kTag, "ESP PM configuration failed: %s", esp_err_to_name(error));
    }
    return error;
}

bool watch_power_is_active(void)
{
    return active;
}

void watch_power_set_active(bool make_active, uint8_t brightness)
{
    if (active == make_active) {
        return;
    }
    // Panel controller sleep is deferred because the current Waveshare BSP does not expose the LCD panel handle through its public API.
    esp_err_t error = bsp_display_brightness_set(make_active ? brightness : 0);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "Display brightness transition failed: %s", esp_err_to_name(error));
        return;
    }
    ESP_LOGI(kTag, "Power state: %s -> %s", active ? "ACTIVE" : "IDLE",
             make_active ? "ACTIVE" : "IDLE");
    active = make_active;
}
