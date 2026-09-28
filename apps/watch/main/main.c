#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_memory_utils.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"

void app_main(void)
{
    bsp_display_start();

    bsp_display_lock(0);

    lv_obj_set_style_bg_color(
        lv_screen_active(),
        lv_color_hex(0x87CEEB),
        LV_PART_MAIN
    );

    lv_obj_t *label = lv_label_create(lv_screen_active());

    lv_label_set_text(
        label,
        "Hello World!\nTesting Smartwatch :)"
    );

    lv_obj_center(label);

    bsp_display_unlock();
}