#include "watch_activity.h"

#include <inttypes.h>
#include <limits.h>
#include <time.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "watch_steps.h"

namespace {

constexpr char kTag[] = "watch_activity";
constexpr char kNamespace[] = "watch_activity";
constexpr char kDayKey[] = "day";
constexpr char kStepsKey[] = "steps";
constexpr time_t kEarliestValidUtc = 946684800; // 2000-01-01 UTC, RTC supported range
constexpr uint32_t kFirstSupportedYear = 2000;
constexpr uint32_t kLastSupportedYear = 2099;
constexpr uint32_t kSaveIntervalMs = 60'000;
constexpr uint32_t kMonitorIntervalMs = 1'000;

nvs_handle_t activity_handle = 0;
bool nvs_available = false;
bool stored_record_valid = false;
uint32_t stored_day = 0;
uint32_t stored_steps = 0;

bool current_day_valid = false;
uint32_t current_day = 0;
uint32_t last_observed_steps = 0;
bool dirty = false;
int64_t last_save_attempt_us = 0;

bool valid_calendar_date(uint32_t year, uint32_t month, uint32_t day)
{
    if (year < kFirstSupportedYear || year > kLastSupportedYear || month < 1 || month > 12) {
        return false;
    }
    constexpr uint8_t days_per_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint32_t max_day = days_per_month[month - 1];
    const bool leap_year = (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
    if (month == 2 && leap_year) {
        ++max_day;
    }
    return day >= 1 && day <= max_day;
}

bool valid_encoded_day(uint32_t encoded)
{
    const uint32_t year = encoded / 10000;
    const uint32_t month = (encoded / 100) % 100;
    const uint32_t day = encoded % 100;
    return valid_calendar_date(year, month, day);
}

bool get_local_day(uint32_t *encoded_day)
{
    if (encoded_day == nullptr) {
        return false;
    }

    time_t now;
    time(&now);
    if (now < kEarliestValidUtc) {
        return false;
    }

    struct tm local = {};
    if (localtime_r(&now, &local) == nullptr) {
        return false;
    }

    const uint32_t year = static_cast<uint32_t>(local.tm_year + 1900);
    const uint32_t month = static_cast<uint32_t>(local.tm_mon + 1);
    const uint32_t day = static_cast<uint32_t>(local.tm_mday);
    if (!valid_calendar_date(year, month, day)) {
        return false;
    }

    *encoded_day = year * 10000 + month * 100 + day;
    return true;
}

void load_record(void)
{
    esp_err_t error = nvs_open(kNamespace, NVS_READWRITE, &activity_handle);
    if (error != ESP_OK) {
        ESP_LOGW(kTag, "NVS unavailable; daily count will remain in RAM: %s", esp_err_to_name(error));
        return;
    }
    nvs_available = true;

    uint32_t day = 0;
    uint32_t steps = 0;
    const esp_err_t day_error = nvs_get_u32(activity_handle, kDayKey, &day);
    const esp_err_t steps_error = nvs_get_u32(activity_handle, kStepsKey, &steps);

    if (day_error == ESP_OK && steps_error == ESP_OK && valid_encoded_day(day)) {
        stored_record_valid = true;
        stored_day = day;
        stored_steps = steps;
        ESP_LOGI(kTag, "Loaded record day=%" PRIu32 " steps=%" PRIu32, stored_day, stored_steps);
        return;
    }

    if (day_error != ESP_ERR_NVS_NOT_FOUND && day_error != ESP_OK) {
        ESP_LOGW(kTag, "Could not read activity day: %s", esp_err_to_name(day_error));
    }
    if (steps_error != ESP_ERR_NVS_NOT_FOUND && steps_error != ESP_OK) {
        ESP_LOGW(kTag, "Could not read activity steps: %s", esp_err_to_name(steps_error));
    }
    if (day_error == ESP_OK && !valid_encoded_day(day)) {
        ESP_LOGW(kTag, "Ignoring invalid stored activity date: %" PRIu32, day);
    }
}

bool persist_record(uint32_t day, uint32_t steps)
{
    if (!nvs_available) {
        return false;
    }

    // Both values are staged before commit; the periodic batching avoids per-step flash writes.
    esp_err_t error = nvs_set_u32(activity_handle, kStepsKey, steps);
    if (error == ESP_OK) {
        error = nvs_set_u32(activity_handle, kDayKey, day);
    }
    if (error == ESP_OK) {
        error = nvs_commit(activity_handle);
    }
    last_save_attempt_us = esp_timer_get_time();
    if (error != ESP_OK) {
        ESP_LOGW(kTag, "Could not persist day=%" PRIu32 " steps=%" PRIu32 ": %s",
                 day, steps, esp_err_to_name(error));
        return false;
    }

    stored_record_valid = true;
    stored_day = day;
    stored_steps = steps;
    dirty = false;
    ESP_LOGI(kTag, "Saved day=%" PRIu32 " steps=%" PRIu32, day, steps);
    return true;
}

uint32_t add_without_overflow(uint32_t first, uint32_t second)
{
    return second > UINT32_MAX - first ? UINT32_MAX : first + second;
}

void reconcile_day(uint32_t today)
{
    const uint32_t live_steps = watch_steps_get_count();
    if (!current_day_valid) {
        if (stored_record_valid && stored_day == today) {
            // If time was invalid at boot, preserve the stored count and add any
            // steps collected in RAM while waiting for RTC/SNTP time.
            const uint32_t restored = add_without_overflow(stored_steps, live_steps);
            watch_steps_set_count(restored);
            current_day = today;
            current_day_valid = true;
            last_observed_steps = restored;
            dirty = restored != stored_steps;
            ESP_LOGI(kTag, "Restored today's activity: %" PRIu32 " steps", restored);
        } else {
            // A missing/different record starts the valid current local day at zero.
            watch_steps_set_count(0);
            current_day = today;
            current_day_valid = true;
            last_observed_steps = 0;
            dirty = true;
            ESP_LOGI(kTag, "Started new activity day %" PRIu32, today);
            persist_record(current_day, 0);
        }
        return;
    }

    if (today != current_day) {
        current_day = today;
        watch_steps_set_count(0);
        last_observed_steps = 0;
        dirty = true;
        ESP_LOGI(kTag, "Local day changed to %" PRIu32 "; daily steps reset", today);
        // Day rollover is an important lifecycle transition: save it immediately.
        persist_record(current_day, 0);
    }
}

void activity_task(void *)
{
    while (true) {
        uint32_t today = 0;
        if (get_local_day(&today)) {
            reconcile_day(today);

            const uint32_t steps = watch_steps_get_count();
            if (steps != last_observed_steps) {
                last_observed_steps = steps;
                dirty = true;
            }

            const int64_t now_us = esp_timer_get_time();
            const int64_t save_interval_us = static_cast<int64_t>(kSaveIntervalMs) * 1000;
            if (dirty && now_us - last_save_attempt_us >= save_interval_us) {
                persist_record(current_day, steps);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(kMonitorIntervalMs));
    }
}

} // namespace

esp_err_t watch_activity_init(void)
{
    load_record();

    uint32_t today = 0;
    if (get_local_day(&today)) {
        reconcile_day(today);
    } else {
        // Keep the current live count untouched until RTC/SNTP establishes a valid date.
        ESP_LOGI(kTag, "System date not trusted yet; deferring activity restore/rollover");
    }
    last_save_attempt_us = esp_timer_get_time();

    if (xTaskCreate(activity_task, "activity_day", 3072, nullptr, 3, nullptr) != pdPASS) {
        ESP_LOGE(kTag, "Could not start daily activity monitor");
        return ESP_ERR_NO_MEM;
    }
    return nvs_available ? ESP_OK : ESP_ERR_INVALID_STATE;
}
