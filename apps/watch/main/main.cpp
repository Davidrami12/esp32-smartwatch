#include "watch_ui.h"
#include "watch_settings.h"
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
#define WIFI_CONNECTED_BIT BIT0

#define AXP2101_I2C_ADDRESS 0x34
#define I2C_TIMEOUT_MS 1000

static EventGroupHandle_t wifi_event_group;

static bool display_on = true;
static volatile bool wifi_connected = false;

static uint32_t last_activity;
static uint32_t last_tap = 0;

static watch_settings_t user_settings = {60, 10000};
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

static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
)
{
    if (
        event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_START
    ) {
        esp_wifi_connect();
    }
    else if (
        event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_DISCONNECTED
    ) {
        wifi_connected = false;

        esp_wifi_connect();
    }
    else if (
        event_base == IP_EVENT &&
        event_id == IP_EVENT_STA_GOT_IP
    ) {
        wifi_connected = true;

        xEventGroupSetBits(
            wifi_event_group,
            WIFI_CONNECTED_BIT
        );
    }
}

static void wifi_init(void)
{
    wifi_event_group =
        xEventGroupCreate();

    ESP_ERROR_CHECK(
        esp_netif_init()
    );

    ESP_ERROR_CHECK(
        esp_event_loop_create_default()
    );

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg =
        WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(
        esp_wifi_init(&cfg)
    );

    ESP_ERROR_CHECK(
        esp_event_handler_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            wifi_event_handler,
            NULL
        )
    );

    ESP_ERROR_CHECK(
        esp_event_handler_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            wifi_event_handler,
            NULL
        )
    );

    wifi_config_t wifi_config = {};

    snprintf(
        reinterpret_cast<char *>(
            wifi_config.sta.ssid
        ),
        sizeof(
            wifi_config.sta.ssid
        ),
        "%s",
        WIFI_SSID
    );

    snprintf(
        reinterpret_cast<char *>(
            wifi_config.sta.password
        ),
        sizeof(
            wifi_config.sta.password
        ),
        "%s",
        WIFI_PASSWORD
    );

    ESP_ERROR_CHECK(
        esp_wifi_set_mode(
            WIFI_MODE_STA
        )
    );

    ESP_ERROR_CHECK(
        esp_wifi_set_config(
            WIFI_IF_STA,
            &wifi_config
        )
    );

    ESP_ERROR_CHECK(
        esp_wifi_start()
    );

    xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_BIT,
        pdFALSE,
        pdTRUE,
        portMAX_DELAY
    );
}

static void update_wifi_status_cb(
    lv_timer_t *timer
)
{
    (void)timer;

    if (!wifi_connected) {
        watch_ui_set_wifi(
            false,
            0
        );

        return;
    }

    wifi_ap_record_t ap_info = {};

    if (
        esp_wifi_sta_get_ap_info(
            &ap_info
        ) == ESP_OK
    ) {
        watch_ui_set_wifi(
            true,
            ap_info.rssi
        );
    }
    else {
        watch_ui_set_wifi(
            true,
            0
        );
    }
}

/* ---------------- TIME ---------------- */

static void sync_time(void)
{
    esp_sntp_config_t config =
        ESP_NETIF_SNTP_DEFAULT_CONFIG(
            "pool.ntp.org"
        );

    ESP_ERROR_CHECK(
        esp_netif_sntp_init(
            &config
        )
    );

    esp_err_t ret =
        esp_netif_sntp_sync_wait(
            pdMS_TO_TICKS(10000)
        );

    if (ret != ESP_OK) {
        printf(
            "NTP sync failed\n"
        );
    }

    setenv(
        "TZ",
        "CET-1CEST,M3.5.0/2,M10.5.0/3",
        1
    );

    tzset();
}

static void update_clock_cb(
    lv_timer_t *timer
)
{
    (void)timer;

    time_t now;
    struct tm timeinfo;

    time(&now);

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

    strftime(
        date_buffer,
        sizeof(date_buffer),
        "%d/%m/%Y",
        &timeinfo
    );

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

    if (!PMU.isBatteryConnect()) {
        watch_ui_set_battery(
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
        display_on
    );

    if (display_on) {
        last_activity = now;
        return;
    }

    if (
        last_tap != 0 &&
        lv_tick_elaps(last_tap) <= DOUBLE_TAP_MS
    ) {
        bsp_display_brightness_set(
            current_brightness
        );

        display_on = true;
        last_activity = now;
        last_tap = 0;

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

    if (display_timeout_ms == 0) {
        return;
    }

    if (
        display_on &&
        lv_tick_elaps(
            last_activity
        ) >= display_timeout_ms
    ) {
        bsp_display_brightness_set(0);

        display_on = false;
        last_tap = 0;
    }
}

static void ui_brightness_changed(
    uint8_t brightness
)
{
    current_brightness =
        brightness;

    bsp_display_brightness_set(
        current_brightness
    );

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

static void ui_brightness_committed(uint8_t brightness)
{
    esp_err_t error = watch_settings_save_brightness(brightness);
    if (error != ESP_OK) {
        ESP_LOGW("watch_settings", "Brightness persistence failed: %s",
                 esp_err_to_name(error));
    }
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

    /* Initialize battery monitoring */

    if (
        battery_init() != ESP_OK
    ) {
        printf(
            "Battery monitoring unavailable\n"
        );
    }

    /* Initialize Wi-Fi */

    wifi_init();

    /* Synchronize time */

    sync_time();

    /* Create LVGL UI */

    bsp_display_lock(0);

    watch_ui_create();

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

    update_clock_cb(NULL);
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
}
