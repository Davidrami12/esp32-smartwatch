#include <stdio.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"

#define DISPLAY_BRIGHTNESS 60
#define DISPLAY_TIMEOUT_MS 10000
#define DOUBLE_TAP_MS 500

static bool display_on = true;
static uint32_t last_activity;
static uint32_t last_tap = 0;

static lv_obj_t *time_label;
static lv_obj_t *date_label;

static void update_clock_cb(lv_timer_t *timer)
{
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    char time_buffer[16];
    char date_buffer[32];

    strftime(time_buffer, sizeof(time_buffer), "%H:%M:%S", &timeinfo);
    strftime(date_buffer, sizeof(date_buffer), "%d/%m/%Y", &timeinfo);

    lv_label_set_text(time_label, time_buffer);
    lv_label_set_text(date_label, date_buffer);
}

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
        bsp_display_brightness_set(DISPLAY_BRIGHTNESS);

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

void app_main(void)
{
    bsp_display_start();
    bsp_display_brightness_set(DISPLAY_BRIGHTNESS);

    bsp_display_lock(0);

    lv_obj_t *screen = lv_screen_active();

    lv_obj_set_style_bg_color(
        screen,
        lv_color_hex(0x000000),
        LV_PART_MAIN
    );

    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_event_cb(
        screen,
        touch_event_cb,
        LV_EVENT_PRESSED,
        NULL
    );

    // Hora
    time_label = lv_label_create(screen);

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

    // Fecha
    date_label = lv_label_create(screen);

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

    last_activity = lv_tick_get();

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