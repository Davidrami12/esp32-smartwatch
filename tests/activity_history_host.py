"""Run the production activity module with RAM NVS/time/task fakes.

Windows: run from a Visual Studio developer prompt: python tests/activity_history_host.py
No device, clock changes, or real NVS writes are involved.
"""
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
STUBS = {
    "esp_err.h": """#pragma once
using esp_err_t = int;
constexpr int ESP_OK=0, ESP_ERR_NO_MEM=1, ESP_ERR_NVS_NOT_FOUND=2, ESP_FAIL=3;
inline const char *esp_err_to_name(int) { return "mock"; }
""",
    "esp_log.h": """#pragma once
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
""",
    "esp_timer.h": """#pragma once
#include <stdint.h>
inline int64_t mock_us=1000000;
inline int64_t esp_timer_get_time() { return mock_us; }
""",
    "freertos/FreeRTOS.h": """#pragma once
constexpr int portMAX_DELAY=-1, pdPASS=1;
#define pdMS_TO_TICKS(x) (x)
""",
    "freertos/task.h": """#pragma once
inline int xTaskCreate(void (*)(void *),const char *,int,void *,int,void *) { return 1; }
inline void vTaskDelay(int) {}
""",
    "freertos/semphr.h": """#pragma once
using SemaphoreHandle_t = void *;
inline void *xSemaphoreCreateMutex() { return reinterpret_cast<void *>(1); }
inline void xSemaphoreTake(void *,int) {}
inline void xSemaphoreGive(void *) {}
""",
    "nvs.h": """#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstring>
#include <stdint.h>
#include "esp_err.h"
using nvs_handle_t = int;
constexpr int NVS_READWRITE=0;
inline std::map<std::string,uint32_t> legacy;
inline std::vector<char> blob;
inline int writes=0;
inline bool fail_save=false;
inline int nvs_open(const char *,int,int *h) { *h=1; return 0; }
inline int nvs_get_u32(int,const char *key,uint32_t *out) {
    if (!legacy.count(key)) return 2; *out=legacy[key]; return 0;
}
inline int nvs_get_blob(int,const char *,void *out,size_t *len) {
    if (blob.empty()) return 2;
    if (*len < blob.size()) return 3;
    *len=blob.size(); memcpy(out,blob.data(),blob.size()); return 0;
}
inline int nvs_set_blob(int,const char *,const void *data,size_t len) {
    ++writes; if (fail_save) return 3;
    blob.assign(static_cast<const char *>(data),static_cast<const char *>(data)+len); return 0;
}
inline int nvs_commit(int) { return 0; }
""",
}
HARNESS = r'''
#include <cassert>
#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include "nvs.h"
static time_t clock_now;
static time_t fake_time(time_t *out) { if(out) *out=clock_now; return clock_now; }
static tm *fake_localtime(const time_t *in,tm *out) {
    return localtime_s(out,in)==0 ? out : nullptr;
}
#define time fake_time
#define localtime_r fake_localtime
#include "watch_activity.cpp"
#undef time
#undef localtime_r
static uint32_t live;
void watch_ble_update_steps(uint32_t) {}
uint32_t watch_steps_get_count() { return live; }
void watch_steps_add_count(uint32_t n) { live += n; }
uint32_t watch_steps_exchange_count(uint32_t n) { auto old=live; live=n; return old; }
static void day(int year,int month,int date) {
    tm value={}; value.tm_year=year-1900; value.tm_mon=month-1; value.tm_mday=date;
    value.tm_hour=12; value.tm_isdst=-1; clock_now=mktime(&value);
}
static void reboot() {
    history={kVersion,{}}; current_day=0; observed_steps=0; dirty=false; last_save=0;
    live=0; storage_ready=false;
    assert(watch_activity_init()==ESP_OK);
}
int main() {
    _putenv_s("TZ","CET-1CEST"); _tzset();
    legacy["day"]=20261001; legacy["steps"]=5421;
    day(2026,10,1); reboot(); assert(live==5421); assert(blob.size()==60);
    watch_activity_day_t days[7]; assert(watch_activity_get_history(days));
    assert(days[0].date==20261001 && days[0].weekday==4 && days[0].steps==5421);
    for(int i=1;i<7;i++) assert(days[i].steps==0);
    // Finalize unsaved live steps on rollover, then reboot preserves both days.
    live=5500; day(2026,10,2); reconcile(); save_if_due(); assert(live==0);
    assert(watch_activity_get_history(days) && days[1].steps==5500);
    live=23; mock_us+=60000000; reconcile(); save_if_due(); reboot(); assert(live==23);
    // Off for several days: holes are zero, older records remain within the window.
    day(2026,10,5); reboot(); assert(live==0); assert(watch_activity_get_history(days));
    assert(days[1].date==20261004 && days[1].steps==0);
    assert(days[3].date==20261002 && days[3].steps==23 && days[4].steps==5500);
    // Invalid time does not overwrite NVS, and pending steps merge on valid same-day recovery.
    live=42; reconcile(); mock_us+=60000000; save_if_due();
    clock_now=0; int before=writes; reboot(); assert(writes==before);
    live=3; assert(!watch_activity_get_history(days)); day(2026,10,5); reconcile(); assert(live==45);
    // Stable same-day corrections must not reset; a previously tracked day can be restored.
    reconcile(); assert(live==45);
    day(2026,10,4); reconcile(); assert(live==0);
    live=7; day(2026,10,5); reconcile(); assert(live==45);
    assert(watch_activity_get_history(days) && days[1].steps==7);
    // Calendar boundary cases, including both European DST transition weekends.
    for(auto date : {20260330u,20261026u,20260101u,20280301u}) {
        calendar_window(date,days);
        for(int i=1;i<7;i++) assert(days[i].date<days[i-1].date);
    }
    calendar_window(20260101,days); assert(days[1].date==20251231);
    calendar_window(20280301,days); assert(days[1].date==20280229);
    assert(!valid_day(20260229) && valid_day(20280229));
    // Storage failure preserves dirty RAM state and retry batching.
    live=46; reconcile(); fail_save=true; mock_us+=60000000; save_if_due(); assert(dirty);
    before=writes; save_if_due(); assert(writes==before);
    fail_save=false; mock_us+=60000000; save_if_due(); assert(!dirty);
    day(2026,10,20); reconcile(); assert(watch_activity_get_history(days));
    for(auto value : days) assert(value.steps==0);
    puts("PASS: migration, restore, rollover, gaps, invalid-time recovery, date corrections, calendar boundaries, batching, NVS errors, pruning");
}
'''

with tempfile.TemporaryDirectory(prefix="watch-activity-") as directory:
    work = pathlib.Path(directory)
    for name, text in STUBS.items():
        path = work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
    (work / "test.cpp").write_text(HARNESS)
    subprocess.run([
        "cl", "/nologo", "/std:c++17", "/EHsc", "/D_CRT_SECURE_NO_WARNINGS",
        f"/I{work}", f"/I{ROOT / 'apps/watch/main'}", "test.cpp", "/Fe:test.exe",
    ], cwd=work, check=True)
    subprocess.run([str(work / "test.exe")], cwd=work, check=True)
