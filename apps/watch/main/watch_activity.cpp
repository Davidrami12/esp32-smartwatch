#include "watch_activity.h"
#include "watch_steps.h"

#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs.h"

namespace {
constexpr char kTag[] = "watch_activity";
constexpr uint32_t kVersion = 1;
constexpr int64_t kSaveUs = 60'000'000; // Dirty snapshots once/minute, rollover immediately.
struct Record { uint32_t day; uint32_t steps; };
struct Storage { uint32_t version; Record records[7]; };
static_assert(sizeof(Storage) == 60, "NVS history v1 layout must stay stable");
Storage history = {kVersion, {}};
SemaphoreHandle_t lock = nullptr;
nvs_handle_t handle = 0;
bool storage_ready = false;
uint32_t current_day = 0;
uint32_t observed_steps = 0;
bool dirty = false;
int64_t last_save = 0;

uint32_t encode(const tm &date)
{
    return (date.tm_year + 1900) * 10000 + (date.tm_mon + 1) * 100 + date.tm_mday;
}

bool valid_day(uint32_t day)
{
    tm date = {};
    date.tm_year = day / 10000 - 1900;
    date.tm_mon = (day / 100) % 100 - 1;
    date.tm_mday = day % 100;
    date.tm_hour = 12;
    date.tm_isdst = -1;
    return date.tm_year >= 100 && date.tm_year <= 199 && mktime(&date) != -1 && encode(date) == day;
}

bool local_date(tm *date)
{
    const time_t now = time(nullptr);
    return now >= 946684800 && localtime_r(&now, date) && date->tm_year >= 100 && date->tm_year <= 199;
}

void calendar_window(uint32_t today, watch_activity_day_t days[7])
{
    tm date = {};
    date.tm_year = today / 10000 - 1900;
    date.tm_mon = (today / 100) % 100 - 1;
    date.tm_mday = today % 100;
    // Calendar subtraction at local noon avoids midnight DST ambiguities.
    date.tm_hour = 12;
    for (unsigned i = 0; i < 7; ++i) {
        date.tm_isdst = -1;
        mktime(&date);
        days[i] = {encode(date), 0, static_cast<uint8_t>(date.tm_wday)};
        --date.tm_mday;
    }
}

Record *find(uint32_t day)
{
    for (auto &record : history.records) if (record.day == day) return &record;
    return nullptr;
}

void set_record(uint32_t day, uint32_t steps)
{
    Record *record = find(day);
    if (!record) {
        record = &history.records[0];
        for (auto &candidate : history.records) {
            if (candidate.day < record->day) record = &candidate;
        }
    }
    *record = {day, steps};
}

void prune(uint32_t today)
{
    watch_activity_day_t days[7];
    calendar_window(today, days);
    for (auto &record : history.records) {
        // Keep future-dated records during a backward clock correction so a subsequent
        // correction back to that day restores its count rather than repeatedly resetting it.
        // They remain bounded by the same seven slots and are never shown as past history.
        bool keep = record.day > today;
        for (const auto &day : days) if (day.date == record.day) keep = true;
        if (!keep) record = {};
    }
}

void load()
{
    const esp_err_t open_error = nvs_open("watch_activity", NVS_READWRITE, &handle);
    storage_ready = open_error == ESP_OK;
    if (!storage_ready) {
        ESP_LOGW(kTag, "NVS unavailable: %s; using RAM history", esp_err_to_name(open_error));
        return;
    }
    Storage loaded = {};
    size_t length = sizeof(loaded);
    const esp_err_t error = nvs_get_blob(handle, "history", &loaded, &length);
    if (error == ESP_OK && length == sizeof(loaded) && loaded.version == kVersion) {
        bool valid = true;
        for (unsigned i = 0; i < 7; ++i) {
            if (loaded.records[i].day && !valid_day(loaded.records[i].day)) valid = false;
            for (unsigned j = 0; j < i; ++j) {
                if (loaded.records[i].day && loaded.records[i].day == loaded.records[j].day) valid = false;
            }
        }
        if (valid) { history = loaded; return; }
    }
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        // One-time migration: preserve the previous daily count without erasing NVS.
        uint32_t day = 0, steps = 0;
        if (nvs_get_u32(handle, "day", &day) == ESP_OK && valid_day(day) &&
            nvs_get_u32(handle, "steps", &steps) == ESP_OK) {
            set_record(day, steps);
            ESP_LOGI(kTag, "Migrating legacy day=%lu steps=%lu", (unsigned long)day, (unsigned long)steps);
        }
    } else ESP_LOGW(kTag, "Ignoring invalid/unreadable history: %s", esp_err_to_name(error));
    dirty = true;
}

void reconcile()
{
    tm date = {};
    if (!local_date(&date)) return; // Never reset/overwrite persisted dates at invalid epoch.
    const uint32_t today = encode(date);
    xSemaphoreTake(lock, portMAX_DELAY);
    if (today != current_day) {
        const Record *existing = find(today);
        const uint32_t restored = existing ? existing->steps : 0;
        if (current_day) {
            // Atomically capture the final live count and switch to the target local day's count.
            const uint32_t final_steps = watch_steps_exchange_count(restored);
            set_record(current_day, final_steps);
        } else if (existing) {
            watch_steps_add_count(restored); // Keep steps collected while awaiting trustworthy time.
        } else {
            watch_steps_exchange_count(0);
        }
        current_day = today;
        prune(today);
        observed_steps = watch_steps_get_count();
        set_record(today, observed_steps);
        dirty = true;
        last_save = 0; // Important day transition: persist immediately.
        ESP_LOGI(kTag, "Current local day=%lu steps=%lu", (unsigned long)today, (unsigned long)observed_steps);
    }
    const uint32_t steps = watch_steps_get_count();
    if (steps != observed_steps) { observed_steps = steps; dirty = true; }
    set_record(current_day, steps);
    xSemaphoreGive(lock);
}

void save_if_due()
{
    if (!current_day || !dirty || (last_save && esp_timer_get_time() - last_save < kSaveUs)) return;
    last_save = esp_timer_get_time();
    if (!storage_ready) return;
    Storage snapshot;
    xSemaphoreTake(lock, portMAX_DELAY);
    snapshot = history;
    xSemaphoreGive(lock);
    esp_err_t error = nvs_set_blob(handle, "history", &snapshot, sizeof(snapshot));
    if (error == ESP_OK) error = nvs_commit(handle);
    if (error == ESP_OK) {
        dirty = false;
        ESP_LOGI(kTag, "Saved seven-day history");
    } else ESP_LOGW(kTag, "History save failed: %s", esp_err_to_name(error));
}

void worker(void *)
{
    while (true) {
        reconcile();
        save_if_due();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
} // namespace

esp_err_t watch_activity_init(void)
{
    lock = xSemaphoreCreateMutex();
    if (!lock) return ESP_ERR_NO_MEM;
    load();
    reconcile();
    save_if_due();
    return xTaskCreate(worker, "activity_day", 4096, nullptr, 3, nullptr) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

bool watch_activity_get_history(watch_activity_day_t days[7])
{
    if (!days || !lock) return false;
    tm date = {};
    if (!local_date(&date)) return false;
    xSemaphoreTake(lock, portMAX_DELAY);
    if (!current_day) { xSemaphoreGive(lock); return false; }
    // Anchor to the reconciled day to avoid labeling yesterday's live count as today at midnight.
    calendar_window(current_day, days);
    for (auto &record : history.records) {
        for (unsigned i = 0; i < 7; ++i) if (days[i].date == record.day) days[i].steps = record.steps;
    }
    days[0].steps = watch_steps_get_count();
    xSemaphoreGive(lock);
    return true;
}
