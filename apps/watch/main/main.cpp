#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
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

static lv_obj_t *home_screen;
static lv_obj_t *settings_screen;

static EventGroupHandle_t wifi_event_group;

static bool display_on = true;
static volatile bool wifi_connected = false;

static uint32_t last_activity;
static uint32_t last_tap = 0;

static uint8_t current_brightness = 60;

static lv_obj_t *time_label;
static lv_obj_t *date_label;

static lv_obj_t *wifi_status_label;

static lv_obj_t *battery_label;
static lv_obj_t *charging_label;

static lv_obj_t *battery_icon_body;
static lv_obj_t *battery_icon_level;
static lv_obj_t *battery_icon_tip;

static lv_obj_t *brightness_label;
static lv_obj_t *brightness_slider;

static uint32_t display_timeout_ms = 10000;

static XPowersPMU PMU;
static i2c_master_dev_handle_t pmu_dev_handle = NULL;

enum class WatchScreen {
    Home,
    Settings
};

static WatchScreen current_screen =
    WatchScreen::Home;

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
    if (!wifi_connected) {
        lv_label_set_text(
            wifi_status_label,
            "WiFi Status: Disconnected"
        );

        return;
    }

    wifi_ap_record_t ap_info = {};

    if (
        esp_wifi_sta_get_ap_info(
            &ap_info
        ) == ESP_OK
    ) {
        char buffer[64];

        snprintf(
            buffer,
            sizeof(buffer),
            "WiFi Status: Connected (%d dBm)",
            ap_info.rssi
        );

        lv_label_set_text(
            wifi_status_label,
            buffer
        );
    }
    else {
        lv_label_set_text(
            wifi_status_label,
            "WiFi Status: Connected"
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

    lv_label_set_text(
        time_label,
        time_buffer
    );

    lv_label_set_text(
        date_label,
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
        lv_label_set_text(
            battery_label,
            "--%"
        );

        lv_label_set_text(
            charging_label,
            "No battery"
        );

        lv_obj_set_width(
            battery_icon_level,
            0
        );

        lv_obj_set_style_text_color(
            battery_label,
            lv_color_hex(0xAAAAAA),
            LV_PART_MAIN
        );

        return;
    }

    int battery_percent =
        PMU.getBatteryPercent();

    int battery_voltage =
        PMU.getBattVoltage();

    bool charging =
        PMU.isCharging();

    if (battery_percent < 0) {
        battery_percent = 0;
    }

    if (battery_percent > 100) {
        battery_percent = 100;
    }

    char battery_buffer[16];

    snprintf(
        battery_buffer,
        sizeof(battery_buffer),
        "%d%%",
        battery_percent
    );

    lv_label_set_text(
        battery_label,
        battery_buffer
    );

    /* Calculate battery fill */

    int fill_width =
        (46 * battery_percent) / 100;

    lv_obj_set_width(
        battery_icon_level,
        fill_width
    );

    /* Select battery color */

    lv_color_t battery_color;

    if (charging) {
        battery_color =
            lv_color_hex(0x00C853);
    }
    else if (battery_percent <= 20) {
        battery_color =
            lv_color_hex(0xF44336);
    }
    else if (battery_percent <= 50) {
        battery_color =
            lv_color_hex(0xFF9800);
    }
    else if (battery_percent <= 70) {
        battery_color =
            lv_color_hex(0xFFD600);
    }
    else {
        battery_color =
            lv_color_hex(0x00C853);
    }

    /* Apply battery color */

    lv_obj_set_style_bg_color(
        battery_icon_level,
        battery_color,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        battery_label,
        battery_color,
        LV_PART_MAIN
    );

    /* Update charging status */

    if (charging) {
        lv_label_set_text(
            charging_label,
            "Charging"
        );
    }

    printf(
        "Battery: %d%% | %d mV | Charging: %s\n",
        battery_percent,
        battery_voltage,
        charging ? "YES" : "NO"
    );
}

/* ---------------- BRIGHTNESS ---------------- */

static void brightness_slider_cb(
    lv_event_t *event
)
{
    lv_obj_t *slider =
        static_cast<lv_obj_t *>(
            lv_event_get_target(
                event
            )
        );

    current_brightness =
        lv_slider_get_value(
            slider
        );

    bsp_display_brightness_set(
        current_brightness
    );

    char buffer[16];

    snprintf(
        buffer,
        sizeof(buffer),
        "%d%%",
        current_brightness
    );

    lv_label_set_text(
        brightness_label,
        buffer
    );

    last_activity =
        lv_tick_get();
}

/* ---------------- SCREEN TIMEOUT ---------------- */

static void screen_timeout_dropdown_cb(
    lv_event_t *event
)
{
    if (
        lv_event_get_code(event) !=
        LV_EVENT_VALUE_CHANGED
    ) {
        return;
    }

    lv_obj_t *dropdown =
        static_cast<lv_obj_t *>(
            lv_event_get_target(event)
        );

    uint32_t selected =
        lv_dropdown_get_selected(dropdown);

    switch (selected) {
        case 0:
            display_timeout_ms = 5000;
            break;

        case 1:
            display_timeout_ms = 10000;
            break;

        case 2:
            display_timeout_ms = 30000;
            break;

        case 3:
            display_timeout_ms = 60000;
            break;

        case 4:
            display_timeout_ms = 0;
            break;

        default:
            display_timeout_ms = 10000;
            break;
    }

    last_activity =
        lv_tick_get();
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
        lv_tick_elaps(last_tap) <=
            DOUBLE_TAP_MS
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

/* ---------------- NAVIGATION ---------------- */

static void navigate_to(
    WatchScreen screen
)
{
    if (screen == current_screen) {
        return;
    }

    last_activity =
        lv_tick_get();

    if (screen == WatchScreen::Settings) {
        lv_screen_load_anim(
            settings_screen,
            LV_SCR_LOAD_ANIM_MOVE_LEFT,
            250,
            0,
            false
        );
    }
    else {
        lv_screen_load_anim(
            home_screen,
            LV_SCR_LOAD_ANIM_MOVE_RIGHT,
            250,
            0,
            false
        );
    }

    current_screen = screen;
}

static void open_settings_cb(
    lv_event_t *event
)
{
    if (
        lv_event_get_code(event) !=
        LV_EVENT_CLICKED
    ) {
        return;
    }

    navigate_to(
        WatchScreen::Settings
    );
}

static void back_home_cb(
    lv_event_t *event
)
{
    if (
        lv_event_get_code(event) !=
        LV_EVENT_CLICKED
    ) {
        return;
    }

    navigate_to(
        WatchScreen::Home
    );
}

static void navigation_gesture_cb(
    lv_event_t *event
)
{
    if (
        lv_event_get_code(event) !=
        LV_EVENT_GESTURE
    ) {
        return;
    }

    lv_indev_t *indev =
        lv_indev_active();

    if (indev == NULL) {
        return;
    }

    lv_dir_t direction =
        lv_indev_get_gesture_dir(
            indev
        );

    if (
        current_screen == WatchScreen::Home &&
        direction == LV_DIR_LEFT
    ) {
        navigate_to(
            WatchScreen::Settings
        );
    }
    else if (
        current_screen == WatchScreen::Settings &&
        direction == LV_DIR_RIGHT
    ) {
        navigate_to(
            WatchScreen::Home
        );
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

    /* =========================================================
    * HOME SCREEN
    * ========================================================= */

    home_screen =
        lv_screen_active();

    lv_obj_set_style_bg_color(
        home_screen,
        lv_color_hex(0x000000),
        LV_PART_MAIN
    );

    lv_obj_add_flag(
        home_screen,
        LV_OBJ_FLAG_CLICKABLE
    );

    lv_obj_add_event_cb(
        home_screen,
        navigation_gesture_cb,
        LV_EVENT_GESTURE,
        NULL
    );

    /* ---------------- TIME ---------------- */

    time_label =
        lv_label_create(home_screen);

    lv_obj_set_style_text_color(
        time_label,
        lv_color_hex(0xFFFFFF),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_font(
        time_label,
        &lv_font_montserrat_26,
        LV_PART_MAIN
    );

    lv_obj_align(
        time_label,
        LV_ALIGN_TOP_MID,
        0,
        20
    );

    /* ---------------- DATE ---------------- */

    date_label =
        lv_label_create(home_screen);

    lv_obj_set_style_text_color(
        date_label,
        lv_color_hex(0xAAAAAA),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_font(
        date_label,
        &lv_font_montserrat_26,
        LV_PART_MAIN
    );

    lv_obj_align(
        date_label,
        LV_ALIGN_TOP_MID,
        0,
        60
    );

    /* ---------------- WIFI STATUS ---------------- */

    wifi_status_label =
        lv_label_create(home_screen);

    lv_label_set_text(
        wifi_status_label,
        "WiFi Status: Connecting..."
    );

    lv_obj_set_style_text_color(
        wifi_status_label,
        lv_color_hex(0xAAAAAA),
        LV_PART_MAIN
    );

    lv_obj_align(
        wifi_status_label,
        LV_ALIGN_TOP_MID,
        0,
        105
    );

    /* ---------------- BATTERY ---------------- */

    battery_icon_body =
        lv_obj_create(home_screen);

    lv_obj_remove_style_all(
        battery_icon_body
    );

    lv_obj_set_size(
        battery_icon_body,
        56,
        30
    );

    lv_obj_set_style_border_width(
        battery_icon_body,
        3,
        LV_PART_MAIN
    );

    lv_obj_set_style_border_color(
        battery_icon_body,
        lv_color_hex(0xFFFFFF),
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        battery_icon_body,
        5,
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        battery_icon_body,
        LV_OPA_TRANSP,
        LV_PART_MAIN
    );

    lv_obj_align(
        battery_icon_body,
        LV_ALIGN_CENTER,
        -45,
        -10
    );

    /* Battery level */

    battery_icon_level =
        lv_obj_create(battery_icon_body);

    lv_obj_remove_style_all(
        battery_icon_level
    );

    lv_obj_set_height(
        battery_icon_level,
        20
    );

    lv_obj_set_width(
        battery_icon_level,
        0
    );

    lv_obj_set_style_bg_color(
        battery_icon_level,
        lv_color_hex(0xFFFFFF),
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        battery_icon_level,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        battery_icon_level,
        2,
        LV_PART_MAIN
    );

    lv_obj_align(
        battery_icon_level,
        LV_ALIGN_LEFT_MID,
        5,
        0
    );

    /* Battery tip */

    battery_icon_tip =
        lv_obj_create(home_screen);

    lv_obj_remove_style_all(
        battery_icon_tip
    );

    lv_obj_set_size(
        battery_icon_tip,
        5,
        14
    );

    lv_obj_set_style_bg_color(
        battery_icon_tip,
        lv_color_hex(0xFFFFFF),
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        battery_icon_tip,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        battery_icon_tip,
        2,
        LV_PART_MAIN
    );

    lv_obj_align_to(
        battery_icon_tip,
        battery_icon_body,
        LV_ALIGN_OUT_RIGHT_MID,
        2,
        0
    );

    /* Battery percentage */

    battery_label =
        lv_label_create(home_screen);

    lv_label_set_text(
        battery_label,
        "--%"
    );

    lv_obj_set_style_text_color(
        battery_label,
        lv_color_hex(0xFFFFFF),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_font(
        battery_label,
        &lv_font_montserrat_26,
        LV_PART_MAIN
    );

    lv_obj_align(
        battery_label,
        LV_ALIGN_CENTER,
        40,
        -10
    );

    /* Charging status */

    charging_label =
        lv_label_create(home_screen);

    lv_label_set_text(
        charging_label,
        ""
    );

    lv_obj_set_style_text_color(
        charging_label,
        lv_color_hex(0xAAAAAA),
        LV_PART_MAIN
    );

    lv_obj_align(
        charging_label,
        LV_ALIGN_CENTER,
        0,
        30
    );

    /* ---------------- SETTINGS BUTTON ---------------- */

    lv_obj_t *settings_button =
        lv_button_create(home_screen);

    lv_obj_set_size(
        settings_button,
        150,
        50
    );

    lv_obj_align(
        settings_button,
        LV_ALIGN_BOTTOM_MID,
        0,
        -70
    );

    lv_obj_add_event_cb(
        settings_button,
        open_settings_cb,
        LV_EVENT_CLICKED,
        NULL
    );

    lv_obj_t *settings_button_label =
        lv_label_create(settings_button);

    lv_label_set_text(
        settings_button_label,
        "Settings"
    );

    lv_obj_center(
        settings_button_label
    );

    /* =========================================================
    * SETTINGS SCREEN
    * ========================================================= */

    settings_screen =
        lv_obj_create(NULL);

    lv_obj_set_style_bg_color(
        settings_screen,
        lv_color_hex(0x000000),
        LV_PART_MAIN
    );

    lv_obj_add_flag(
        settings_screen,
        LV_OBJ_FLAG_CLICKABLE
    );

    lv_obj_add_event_cb(
        settings_screen,
        navigation_gesture_cb,
        LV_EVENT_GESTURE,
        NULL
    );

    /* ---------------- SETTINGS TITLE ---------------- */

    lv_obj_t *settings_title =
        lv_label_create(settings_screen);

    lv_label_set_text(
        settings_title,
        "Settings"
    );

    lv_obj_set_style_text_color(
        settings_title,
        lv_color_hex(0xFFFFFF),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_font(
        settings_title,
        &lv_font_montserrat_26,
        LV_PART_MAIN
    );

    lv_obj_align(
        settings_title,
        LV_ALIGN_TOP_MID,
        0,
        30
    );

    /* ---------------- BRIGHTNESS TITLE ---------------- */

    lv_obj_t *brightness_title =
        lv_label_create(settings_screen);

    lv_label_set_text(
        brightness_title,
        "Brightness control"
    );

    lv_obj_set_style_text_color(
        brightness_title,
        lv_color_hex(0xFFFFFF),
        LV_PART_MAIN
    );

    lv_obj_align(
        brightness_title,
        LV_ALIGN_CENTER,
        0,
        -70
    );

    /* ---------------- BRIGHTNESS VALUE ---------------- */

    brightness_label =
        lv_label_create(settings_screen);

    char brightness_buffer[16];

    snprintf(
        brightness_buffer,
        sizeof(brightness_buffer),
        "%d%%",
        current_brightness
    );

    lv_label_set_text(
        brightness_label,
        brightness_buffer
    );

    lv_obj_set_style_text_color(
        brightness_label,
        lv_color_hex(0xFFFFFF),
        LV_PART_MAIN
    );

    lv_obj_align(
        brightness_label,
        LV_ALIGN_CENTER,
        0,
        -30
    );

    /* ---------------- BRIGHTNESS SLIDER ---------------- */

    brightness_slider =
        lv_slider_create(settings_screen);

    lv_slider_set_range(
        brightness_slider,
        10,
        100
    );

    lv_slider_set_value(
        brightness_slider,
        current_brightness,
        LV_ANIM_OFF
    );

    lv_obj_set_width(
        brightness_slider,
        220
    );

    lv_obj_align(
        brightness_slider,
        LV_ALIGN_CENTER,
        0,
        20
    );

    lv_obj_add_event_cb(
        brightness_slider,
        brightness_slider_cb,
        LV_EVENT_VALUE_CHANGED,
        NULL
    );

    /* ---------------- SCREEN TIMEOUT TITLE ---------------- */

    lv_obj_t *screen_timeout_title =
        lv_label_create(settings_screen);

    lv_label_set_text(
        screen_timeout_title,
        "Screen timeout"
    );

    lv_obj_set_style_text_color(
        screen_timeout_title,
        lv_color_hex(0xFFFFFF),
        LV_PART_MAIN
    );

    lv_obj_align(
        screen_timeout_title,
        LV_ALIGN_CENTER,
        0,
        80
    );

    /* ---------------- SCREEN TIMEOUT DROPDOWN ---------------- */

    lv_obj_t *screen_timeout_dropdown =
        lv_dropdown_create(settings_screen);

    lv_dropdown_set_options(
        screen_timeout_dropdown,
        "5 seconds\n"
        "10 seconds\n"
        "30 seconds\n"
        "60 seconds\n"
        "Never"
    );

    lv_dropdown_set_selected(
        screen_timeout_dropdown,
        1
    );

    lv_obj_set_width(
        screen_timeout_dropdown,
        180
    );

    lv_obj_align(
        screen_timeout_dropdown,
        LV_ALIGN_CENTER,
        0,
        125
    );

    lv_obj_add_event_cb(
        screen_timeout_dropdown,
        screen_timeout_dropdown_cb,
        LV_EVENT_VALUE_CHANGED,
        NULL
    );

    /* ---------------- BACK BUTTON ---------------- */

    lv_obj_t *back_button =
        lv_button_create(settings_screen);

    lv_obj_set_size(
        back_button,
        130,
        50
    );

    lv_obj_align(
        back_button,
        LV_ALIGN_BOTTOM_MID,
        0,
        -55
    );

    lv_obj_add_event_cb(
        back_button,
        back_home_cb,
        LV_EVENT_CLICKED,
        NULL
    );

    lv_obj_t *back_button_label =
        lv_label_create(back_button);

    lv_label_set_text(
        back_button_label,
        "Back"
    );

    lv_obj_center(
        back_button_label
    );

    /* ---------------- TIMERS ---------------- */

    update_clock_cb(NULL);
    update_wifi_status_cb(NULL);
    update_battery_cb(NULL);

    last_activity =
        lv_tick_get();

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