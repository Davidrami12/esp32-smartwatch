#include "watch_2048_store.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_timer.h"

namespace {
constexpr const char *kTag = "watch_2048";
uint32_t saved = 0, pending = 0;
int64_t last_attempt = 0;
bool flush()
{
    if (pending <= saved) return true;
    last_attempt = esp_timer_get_time();
    nvs_handle_t handle;
    esp_err_t err = nvs_open("watch_2048", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_u32(handle, "best_score", pending);
        if (err == ESP_OK) err = nvs_commit(handle);
        nvs_close(handle);
    }
    if (err != ESP_OK) { ESP_LOGW(kTag, "Best score save failed: %s; retained for retry", esp_err_to_name(err)); return false; }
    saved = pending;
    ESP_LOGI(kTag, "Best score saved: %lu", static_cast<unsigned long>(saved));
    return true;
}
}
uint32_t watch_2048_store_load()
{
    saved = pending = 0;
    last_attempt = esp_timer_get_time();
    nvs_handle_t handle;
    esp_err_t err = nvs_open("watch_2048", NVS_READONLY, &handle);
    if (err == ESP_OK) {
        err = nvs_get_u32(handle, "best_score", &saved);
        nvs_close(handle);
    }
    if (err != ESP_OK) {
        saved = 0;
        if (err != ESP_ERR_NVS_NOT_FOUND)
            ESP_LOGW(kTag, "Best score missing/invalid: %s; using zero", esp_err_to_name(err));
    }
    if (saved != UINT32_MAX && saved % 4) {
        ESP_LOGW(kTag, "Invalid best score value; using zero");
        saved = 0;
    }
    pending = saved;
    ESP_LOGI(kTag, "Best score loaded: %lu", static_cast<unsigned long>(saved));
    return saved;
}
bool watch_2048_store_best(uint32_t best, bool force)
{
    if (best > pending) pending = best;
    return force ? flush() : true;
}
void watch_2048_store_tick()
{
    if (pending > saved && esp_timer_get_time() - last_attempt >= 15'000'000) flush();
}
