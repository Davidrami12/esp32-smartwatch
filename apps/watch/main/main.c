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
        lv_color_hex(0x87CEEB),
        LV_PART_MAIN
    );

    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_event_cb(
        screen,
        touch_event_cb,
        LV_EVENT_PRESSED,
        NULL
    );

    lv_obj_t *label = lv_label_create(screen);

    lv_label_set_text(
        label,
        "Hello World!\nTesting Smartwatch :)"
    );

    lv_obj_center(label);

    last_activity = lv_tick_get();

    lv_timer_create(
        display_timeout_cb,
        200,
        NULL
    );

    bsp_display_unlock();
}