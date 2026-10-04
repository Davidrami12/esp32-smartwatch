#include "watch_ui.h"
#include "watch_settings.h"
#include "watch_rtc.h"
#include "watch_power.h"
#include "watch_motion.h"
#include "watch_steps.h"
#include "watch_activity.h"
#include "watch_wifi.h"
#include "watch_weather.h"
#include "esp_timer.h"
#include "watch_ble.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "nvs_flash.h"

#include "driver/i2c_master.h"

#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"

#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"

#define DOUBLE_TAP_MS 500

#define AXP2101_I2C_ADDRESS 0x34
#define I2C_TIMEOUT_MS 1000

static constexpr time_t MIN_VALID_EPOCH = 946684800; // 2000-01-01 UTC


static uint32_t last_activity;
static uint32_t last_tap = 0;

static watch_settings_t user_settings = {60, 10000, 8000};
static uint8_t current_brightness = 60;
static uint32_t display_timeout_ms = 10000;

static XPowersPMU PMU;
static i2c_master_dev_handle_t pmu_dev_handle = NULL;

/* ---------------- AXP2101 ---------------- */

static int pmu_register_read(
    uint8_t dev_addr,
    uint8_t reg_addr,
    uint8_t *data,
    uint8_t len
)
{
    (void)dev_addr;

    esp_err_t ret =
        i2c_master_transmit_receive(
            pmu_dev_handle,
            &reg_addr,
            1,
            data,
            len,
            I2C_TIMEOUT_MS
        );

    return ret == ESP_OK ? 0 : -1;
}

static int pmu_register_write_byte(
    uint8_t dev_addr,
    uint8_t reg_addr,
    uint8_t *data,
    uint8_t len
)
{
    (void)dev_addr;

    uint8_t *buffer =
        static_cast<uint8_t *>(
            malloc(len + 1)
        );

    if (buffer == NULL) {
        return -1;
    }

    buffer[0] = reg_addr;

    memcpy(
        &buffer[1],
        data,
        len
    );

    esp_err_t ret =
        i2c_master_transmit(
            pmu_dev_handle,
            buffer,
            len + 1,
            I2C_TIMEOUT_MS
        );

    free(buffer);

    return ret == ESP_OK ? 0 : -1;
}

static esp_err_t battery_init(void)
{
    /*
     * The board uses a shared I2C bus.
     * The display/touch initialization normally creates it.
     */
    i2c_master_bus_handle_t i2c_bus =
        bsp_i2c_get_handle();

    if (i2c_bus == NULL) {
        ESP_ERROR_CHECK(
            bsp_i2c_init()
        );

        i2c_bus =
            bsp_i2c_get_handle();
    }

    if (i2c_bus == NULL) {
        return ESP_FAIL;
    }

    i2c_device_config_t dev_config = {};

    dev_config.dev_addr_length =
        I2C_ADDR_BIT_LEN_7;

    dev_config.device_address =
        AXP2101_I2C_ADDRESS;

    dev_config.scl_speed_hz =
        400000;

    ESP_ERROR_CHECK(
        i2c_master_bus_add_device(
            i2c_bus,
            &dev_config,
            &pmu_dev_handle
        )
    );

    if (
        !PMU.begin(
            AXP2101_SLAVE_ADDRESS,
            pmu_register_read,
            pmu_register_write_byte
        )
    ) {
        printf(
            "Failed to initialize AXP2101\n"
        );

        return ESP_FAIL;
    }

    PMU.enableBattVoltageMeasure();
    PMU.enableVbusVoltageMeasure();
    PMU.enableSystemVoltageMeasure();

    PMU.disableTSPinMeasure();

    printf(
        "AXP2101 initialized\n"
    );

    return ESP_OK;
}

/* ---------------- WIFI ---------------- */

static bool ui_wifi_request(bool enabled)
{
    return watch_wifi_request_enabled(enabled);
}

static bool ui_bluetooth_request(bool enabled)
{
    return watch_ble_set_enabled(enabled);
}

static void update_wifi_status_cb(
    lv_timer_t *timer
)
{
    (void)timer;

    if (!watch_power_is_active()) {
        return;
    }

    const auto state = watch_wifi_get_state();
    const auto ble_state = watch_ble_get_state();
    watch_ui_set_bluetooth_state(ble_state == watch_ble_state_t::Connected ? WATCH_UI_BLUETOOTH_CONNECTED :
        ble_state == watch_ble_state_t::Advertising ? WATCH_UI_BLUETOOTH_ADVERTISING : WATCH_UI_BLUETOOTH_DISABLED);
    watch_ui_set_wifi_state(state == watch_wifi_state_t::Connected ? WATCH_UI_WIFI_CONNECTED :
        state == watch_wifi_state_t::Connecting ? WATCH_UI_WIFI_CONNECTING : WATCH_UI_WIFI_DISCONNECTED,
        watch_wifi_get_rssi());
    watch_ui_set_wifi_name(state == watch_wifi_state_t::Connected ? WIFI_SSID : "");
    const auto weather = watch_weather_get();
    const auto forecast = watch_weather_forecast_get();
    watch_ui_forecast_day_t forecast_rows[7] = {};
    const time_t forecast_now = time(nullptr);
    if (forecast_now >= 1577836800) {
        struct tm day = {};
        localtime_r(&forecast_now, &day);
        day.tm_hour = 12; day.tm_min = day.tm_sec = 0;
        for (unsigned i = 0; i < 7; ++i) {
            day.tm_isdst = -1;
            mktime(&day);
            const uint32_t key = (day.tm_year + 1900) * 10000 + (day.tm_mon + 1) * 100 + day.tm_mday;
            forecast_rows[i].date = key;
            forecast_rows[i].weekday = day.tm_wday;
            for (const auto &source : forecast.days) if (forecast.available && source.date == key) {
                forecast_rows[i].available = true;
                forecast_rows[i].low_c = source.low_c; forecast_rows[i].high_c = source.high_c;
                forecast_rows[i].condition = static_cast<watch_ui_weather_condition_t>(source.condition);
            }
            ++day.tm_mday;
        }
    }
    const uint32_t forecast_age = forecast.available ? static_cast<uint32_t>(
        (esp_timer_get_time() - forecast.fetched_monotonic_us) / 60000000) : 0;
    watch_ui_set_forecast(forecast_rows, state == watch_wifi_state_t::Connected, forecast_age);
    watch_ui_set_weather(weather.available, weather.temperature_c,
                         static_cast<watch_ui_weather_condition_t>(weather.condition));
}

/* ---------------- TIME ---------------- */

static void time_sync_notification_cb(struct timeval *tv);

static void sync_time(void)
{
    esp_sntp_config_t config =
        ESP_NETIF_SNTP_DEFAULT_CONFIG(
            "pool.ntp.org"
        );
    config.sync_cb = time_sync_notification_cb;

    ESP_ERROR_CHECK(esp_netif_sntp_init(&config));

    ESP_LOGI("watch_time", "SNTP initialized");
    setenv(
        "TZ",
        "CET-1CEST,M3.5.0/2,M10.5.0/3",
        1
    );

    tzset();
}

static void time_sync_notification_cb(struct timeval *tv)
{
    (void)tv;
    time_t now;
    time(&now);
    if (now < MIN_VALID_EPOCH) {
        ESP_LOGW("watch_time", "SNTP callback received invalid system time");
        return;
    }
    esp_err_t error = watch_rtc_write_utc(now);
    if (error == ESP_OK) {
        ESP_LOGI("watch_time", "SNTP UTC time synchronized to PCF85063");
    } else {
        ESP_LOGE("watch_time", "Could not synchronize SNTP time to RTC: %s",
                 esp_err_to_name(error));
    }
}

static void seed_clock_from_rtc(void)
{
    time_t timestamp;
    if (!watch_rtc_read_utc(&timestamp)) {
        ESP_LOGW("watch_rtc", "No valid RTC time; awaiting SNTP");
        return;
    }
    struct timeval now = {.tv_sec = timestamp, .tv_usec = 0};
    settimeofday(&now, NULL);
    ESP_LOGI("watch_rtc", "Seeded system clock from UTC RTC");
}

static void start_sntp_when_wifi_connected(void *arg)
{
    (void)arg;
    sync_time();
    vTaskDelete(NULL);
}

static void update_clock_cb(
    lv_timer_t *timer
)
{
    (void)timer;

    if (!watch_power_is_active()) {
        return;
    }

    time_t now;
    struct tm timeinfo;

    time(&now);

    if (now < MIN_VALID_EPOCH) {
        watch_ui_set_time("--:--");
        watch_ui_set_date("Waiting for local date");
        return;
    }

    localtime_r(
        &now,
        &timeinfo
    );

    char time_buffer[16];
    char date_buffer[32];

    strftime(
        time_buffer,
        sizeof(time_buffer),
        "%H:%M:%S",
        &timeinfo
    );

    static const char *weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    snprintf(date_buffer, sizeof(date_buffer), "%s  %02d/%02d/%04d", weekdays[timeinfo.tm_wday],
             timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);

    watch_ui_set_time(
        time_buffer
    );

    watch_ui_set_date(
        date_buffer
    );
}

/* ---------------- BATTERY ---------------- */

static void update_battery_cb(
    lv_timer_t *timer
)
{
    (void)timer;

    // Reuse the existing PMIC acquisition; only sample in IDLE for a connected BLE peer.
    static uint32_t last_battery_sample = 0;
    const bool active = watch_power_is_active();
    if (!active && (watch_ble_get_state() != watch_ble_state_t::Connected ||
                   lv_tick_elaps(last_battery_sample) < 30000)) return;
    last_battery_sample = lv_tick_get();
    if (!PMU.isBatteryConnect()) {
        watch_ble_update_battery(-1);
        if (active) watch_ui_set_battery(
            -1,
            false
        );

        return;
    }

    int battery_percent =
        PMU.getBatteryPercent();

    int battery_voltage =
        PMU.getBattVoltage();

    bool charging =
        PMU.isCharging();

    watch_ble_update_battery(battery_percent);
    if (!active) return;
    watch_ui_set_battery(
        battery_percent,
        charging
    );

    printf(
        "Battery: %d%% | %d mV | Charging: %s\n",
        battery_percent,
        battery_voltage,
        charging ? "YES" : "NO"
    );
}

/* ---------------- DISPLAY ---------------- */

static void update_steps_cb(lv_timer_t *timer);

static void touch_event_cb(
    lv_event_t *event
)
{
    if (
        lv_event_get_code(event) !=
        LV_EVENT_PRESSED
    ) {
        return;
    }

    uint32_t now =
        lv_tick_get();

    printf(
        "Touch | display_on=%d\n",
        watch_power_is_active()
    );

    if (watch_power_is_active()) {
        last_activity = now;
        return;
    }

    if (
        last_tap != 0 &&
        lv_tick_elaps(last_tap) <= DOUBLE_TAP_MS
    ) {
        watch_power_set_active(true, current_brightness);
        watch_ui_set_display_active(true);
        last_activity = now;
        last_tap = 0;

        update_clock_cb(NULL);
        update_steps_cb(NULL);
        update_wifi_status_cb(NULL);
        update_battery_cb(NULL);

        printf("Display wake\n");
    }
    else {
        last_tap = now;
    }
}

static void register_touch_handler(void)
{
    lv_indev_t *indev =
        lv_indev_get_next(NULL);

    while (indev != NULL) {
        if (
            lv_indev_get_type(indev) ==
            LV_INDEV_TYPE_POINTER
        ) {
            lv_indev_add_event_cb(
                indev,
                touch_event_cb,
                LV_EVENT_PRESSED,
                NULL
            );

            printf("Touch handler registered\n");
            return;
        }

        indev =
            lv_indev_get_next(indev);
    }

    printf("Touch input device not found\n");
}

static void display_timeout_cb(
    lv_timer_t *timer
)
{
    (void)timer;
    watch_ui_set_display_active(watch_power_is_active());

    if (display_timeout_ms == 0) {
        return;
    }

    if (
        watch_power_is_active() &&
        lv_tick_elaps(
            last_activity
        ) >= display_timeout_ms
    ) {
        watch_power_set_active(false, current_brightness);
        watch_ui_set_display_active(false);
        last_tap = 0;
    }
}

static void ui_brightness_changed(
    uint8_t brightness
)
{
    current_brightness =
        brightness;

    if (watch_power_is_active()) {
        bsp_display_brightness_set(current_brightness);
    }

    last_activity =
        lv_tick_get();
}

static void ui_timeout_changed(
    uint32_t timeout_ms
)
{
    display_timeout_ms =
        timeout_ms;

    last_activity =
        lv_tick_get();

    esp_err_t error = watch_settings_save_screen_timeout(timeout_ms);
    if (error != ESP_OK) {
        ESP_LOGW("watch_settings", "Timeout persistence failed: %s",
                 esp_err_to_name(error));
    }
}

static void update_steps_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!watch_power_is_active()) {
        return;
    }
    watch_ui_set_steps(watch_steps_get_count());
    watch_activity_day_t history[7];
    watch_ui_activity_day_t rows[7];
    const bool valid = watch_activity_get_history(history);
    if (valid) {
        for (unsigned i = 0; i < 7; ++i) rows[i] = {history[i].steps, history[i].weekday};
    }
    watch_ui_set_history(valid ? rows : nullptr);
}

static void ui_brightness_committed(uint8_t brightness)
{
    esp_err_t error = watch_settings_save_brightness(brightness);
    if (error != ESP_OK) {
        ESP_LOGW("watch_settings", "Brightness persistence failed: %s",
                 esp_err_to_name(error));
    }
}

static bool ui_step_goal_changed(uint32_t goal)
{
    return watch_settings_save_step_goal(goal) == ESP_OK;
}


/* ---------------- MAIN ---------------- */

extern "C" void app_main(void)
{
    esp_err_t ret =
        nvs_flash_init();

    if (
        ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND
    ) {
        ESP_ERROR_CHECK(
            nvs_flash_erase()
        );

        ret =
            nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(watch_power_init());

    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
    tzset();

    watch_settings_load(&user_settings);
    current_brightness = user_settings.brightness;
    display_timeout_ms = user_settings.screen_timeout_ms;

    /* Initialize display */

    lv_display_t *display =
        bsp_display_start();

    if (display == NULL) {
        printf(
            "Failed to initialize display\n"
        );

        return;
    }

    /* Give LVGL task time to finish display initialization */

    vTaskDelay(
        pdMS_TO_TICKS(100)
    );

    bsp_display_lock(0);

    ESP_ERROR_CHECK(
        bsp_display_brightness_set(
            current_brightness
        )
    );

    bsp_display_unlock();

    if (bsp_i2c_get_handle() == NULL) {
        ESP_ERROR_CHECK(bsp_i2c_init());
    }
    if (watch_rtc_init() == ESP_OK) {
        seed_clock_from_rtc();
    }

    watch_steps_init();
    esp_err_t activity_error = watch_activity_init();
    if (activity_error != ESP_OK) {
        ESP_LOGW("watch_activity", "Daily activity persistence unavailable: %s",
                 esp_err_to_name(activity_error));
    }
    esp_err_t motion_error = watch_motion_init();
    if (motion_error != ESP_OK) {
        ESP_LOGW("watch_motion", "Motion sensor unavailable; continuing without IMU");
    }

    /* Initialize battery monitoring */

    if (
        battery_init() != ESP_OK
    ) {
        printf(
            "Battery monitoring unavailable\n"
        );
    }

    /* Create LVGL UI */

    bsp_display_lock(0);

    watch_ui_create();
    watch_ui_set_wifi_control_callback(ui_wifi_request);
    watch_ui_set_bluetooth_control_callback(ui_bluetooth_request);

    watch_ui_set_brightness_callback(
        ui_brightness_changed
    );

    watch_ui_set_timeout_callback(
        ui_timeout_changed
    );

    watch_ui_set_brightness_committed_callback(
        ui_brightness_committed
    );

    watch_ui_set_settings(
        current_brightness,
        display_timeout_ms
    );

    /* Initial UI state */
    watch_ui_set_step_goal(user_settings.step_goal);
    watch_ui_set_step_goal_callback(ui_step_goal_changed);

    update_clock_cb(NULL);
    update_steps_cb(NULL);
    update_wifi_status_cb(NULL);
    update_battery_cb(NULL);

    last_activity =
        lv_tick_get();

    /* Timers */

    lv_timer_create(
        update_clock_cb,
        1000,
        NULL
    );

    lv_timer_create(
        update_steps_cb,
        1000,
        NULL
    );

    lv_timer_create(
        update_wifi_status_cb,
        2000,
        NULL
    );

    lv_timer_create(
        update_battery_cb,
        5000,
        NULL
    );

    lv_timer_create(
        display_timeout_cb,
        200,
        NULL
    );

    register_touch_handler();

    bsp_display_unlock();

    ESP_ERROR_CHECK(watch_wifi_init());
    ESP_ERROR_CHECK(watch_weather_init());
    watch_ble_update_steps(watch_steps_get_count());
    const auto wifi_state = watch_wifi_get_state();
    watch_ble_update_wifi(!watch_wifi_is_enabled() ? 3 : wifi_state == watch_wifi_state_t::Connected ? 2 :
                          wifi_state == watch_wifi_state_t::Connecting ? 1 : 0);
    const esp_err_t ble_error = watch_ble_init();
    if (ble_error != ESP_OK) ESP_LOGW("watch_ble", "BLE unavailable: %s", esp_err_to_name(ble_error));
    xTaskCreate(start_sntp_when_wifi_connected, "sntp_start", 4096, NULL, 5, NULL);
}
