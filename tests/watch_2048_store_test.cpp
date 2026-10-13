// Compile the actual firmware storage adapter against narrowly scoped NVS mocks.
#include "watch_2048_store.h"
#include "nvs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t disk, staged;
static unsigned writes, commits, closes;
static int64_t clock_us;
static esp_err_t open_error, read_error = ESP_ERR_NVS_NOT_FOUND, write_error, commit_error;
const char *esp_err_to_name(esp_err_t) { return "mock error"; }
int64_t esp_timer_get_time() { return clock_us; }
esp_err_t nvs_open(const char *name, nvs_open_mode_t, nvs_handle_t *out)
{
    assert(!strcmp(name, "watch_2048")); *out = 1; return open_error;
}
esp_err_t nvs_get_u32(nvs_handle_t, const char *key, uint32_t *out)
{
    assert(!strcmp(key, "best_score")); if (!read_error) *out = disk; return read_error;
}
esp_err_t nvs_set_u32(nvs_handle_t, const char *key, uint32_t value)
{
    assert(!strcmp(key, "best_score")); ++writes; staged = value; return write_error;
}
esp_err_t nvs_commit(nvs_handle_t)
{
    ++commits; if (!commit_error) { disk = staged; read_error = ESP_OK; } return commit_error;
}
void nvs_close(nvs_handle_t) { ++closes; }
int main()
{
    assert(watch_2048_store_load() == 0);
    watch_2048_store_best(4, false); watch_2048_store_best(8, false);
    clock_us = 14'999'999; watch_2048_store_tick(); assert(writes == 0);
    clock_us = 15'000'000; watch_2048_store_tick(); assert(writes == 1 && commits == 1 && disk == 8);
    watch_2048_store_best(8, true); watch_2048_store_best(4, true); assert(writes == 1);
    assert(watch_2048_store_load() == 8); // Simulated reboot/load.
    watch_2048_store_best(16, true); assert(disk == 16 && writes == 2);
    commit_error = ESP_FAIL;
    assert(!watch_2048_store_best(32, true)); assert(disk == 16);
    commit_error = ESP_OK; clock_us += 15'000'000; watch_2048_store_tick(); assert(disk == 32);
    write_error = ESP_FAIL;
    assert(!watch_2048_store_best(64, true)); assert(disk == 32);
    write_error = ESP_OK; clock_us += 15'000'000; watch_2048_store_tick(); assert(disk == 64);
    open_error = ESP_FAIL;
    assert(!watch_2048_store_best(128, true)); assert(disk == 64);
    open_error = ESP_OK; clock_us += 15'000'000; watch_2048_store_tick(); assert(disk == 128);
    read_error = ESP_ERR_NVS_TYPE_MISMATCH; assert(watch_2048_store_load() == 0);
    read_error = ESP_FAIL; assert(watch_2048_store_load() == 0);
    read_error = ESP_OK; disk = 3; assert(watch_2048_store_load() == 0);
    disk = UINT32_MAX; assert(watch_2048_store_load() == UINT32_MAX);
    assert(closes > 0);
    puts("PASS: actual 2048 NVS adapter defaults/corruption, batching, unchanged/no-lower writes, reload and failed-save retry");
}
