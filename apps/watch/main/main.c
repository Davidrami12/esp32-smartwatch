#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "nvs_flash.h"

#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"

#define DISPLAY_BRIGHTNESS 60
#define DISPLAY_TIMEOUT_MS 10000
#define DOUBLE_TAP_MS 500

#define WIFI_CONNECTED_BIT BIT0

static EventGroupHandle_t wifi_event_group;

static bool display_on = true;
static uint32_t last_activity;
static uint32_t last_tap = 0;

static lv_obj_t *time_label;
static lv_obj_t *date_label;


/* ---------------- WIFI ---------------- */

static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
)
{
    if (event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_START) {

        esp_wifi_connect();
    }

    else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {

        esp_wifi_connect();
    }

    else if (event_base == IP_EVENT &&
             event_id == IP_EVENT_STA_GOT_IP) {

        xEventGroupSetBits(
            wifi_event_group,
            WIFI_CONNECTED_BIT
        );
    }
}


static void wifi_init(void)
{
    wifi_event_group = xEventGroupCreate();

    esp_netif_init();
    esp_event_loop_create_default();

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        wifi_event_handler,
        NULL
    );

    esp_event_handler_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        wifi_event_handler,
        NULL
    );

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_BIT,
        pdFALSE,
        pdTRUE,
        portMAX_DELAY
    );
}


/* ---------------- TIME ---------------- */

static void sync_time(void)
{
    esp_sntp_config_t config =
        ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");

    ESP_ERROR_CHECK(esp_netif_sntp_init(&config));

    esp_err_t ret = esp_netif_sntp_sync_wait(
        pdMS_TO_TICKS(10000)
    );

    if (ret != ESP_OK) {
        printf("NTP sync failed\n");
    }

    setenv(
        "TZ",
        "CET-1CEST,M3.5.0/2,M10.5.0/3",
        1
    );

    tzset();
}


static void update_clock_cb(lv_timer_t *timer)
{
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

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


/* ---------------- DISPLAY ---------------- */

static void touch_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_PRESSED) {
        return;
    }

    uint32_t now = lv_tick_get();

    if (display_on) {
        last_activity = now;
        return;
    }

    if (now - last_tap <= DOUBLE_TAP_MS) {

        bsp_display_brightness_set(
            DISPLAY_BRIGHTNESS
        );

        display_on = true;
        last_activity = now;
        last_tap = 0;

    } else {

        last_tap = now;
    }
}


static void display_timeout_cb(lv_timer_t *timer)
{
    if (
        display_on &&
        lv_tick_elaps(last_activity) >= DISPLAY_TIMEOUT_MS
    ) {

        bsp_display_brightness_set(0);

        display_on = false;
        last_tap = 0;
    }
}


/* ---------------- MAIN ---------------- */

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();

    if (
        ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND
    ) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    bsp_display_start();

    bsp_display_brightness_set(
        DISPLAY_BRIGHTNESS
    );

    wifi_init();

    sync_time();

    bsp_display_lock(0);

    lv_obj_t *screen =
        lv_screen_active();

    lv_obj_set_style_bg_color(
        screen,
        lv_color_hex(0x000000),
        LV_PART_MAIN
    );

    lv_obj_add_flag(
        screen,
        LV_OBJ_FLAG_CLICKABLE
    );

    lv_obj_add_event_cb(
        screen,
        touch_event_cb,
        LV_EVENT_PRESSED,
        NULL
    );

    time_label =
        lv_label_create(screen);

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
        LV_ALIGN_CENTER,
        0,
        -35
    );

    date_label =
        lv_label_create(screen);

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
        LV_ALIGN_CENTER,
        0,
        35
    );

    update_clock_cb(NULL);

    last_activity =
        lv_tick_get();

    lv_timer_create(
        update_clock_cb,
        1000,
        NULL
    );

    lv_timer_create(
        display_timeout_cb,
        200,
        NULL
    );

    bsp_display_unlock();
}