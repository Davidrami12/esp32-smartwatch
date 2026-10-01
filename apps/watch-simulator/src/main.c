/**
 * @file main.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#ifndef _DEFAULT_SOURCE
  #define _DEFAULT_SOURCE /* needed for usleep() */
#endif

#include <stdlib.h>
#include <stdio.h>
#ifdef _MSC_VER
  #include <Windows.h>
#else
  #include <unistd.h>
  #include <pthread.h>
#endif
#include "lvgl/lvgl.h"
#include "lvgl/examples/lv_examples.h"
#include "lvgl/demos/lv_demos.h"
#include "watch_ui.h"
#include <time.h>
#include <SDL.h>

#include "hal/hal.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

#if LV_USE_OS != LV_OS_FREERTOS

static void brightness_changed(uint8_t brightness)
{
    printf(
        "Brightness changed: %u%%\n",
        brightness
    );
}

static void timeout_changed(uint32_t timeout_ms)
{
    printf(
        "Timeout changed: %u ms\n",
        timeout_ms
    );
}

static void update_clock_cb(lv_timer_t *timer)
{
    (void)timer;

    time_t now = time(NULL);
    struct tm timeinfo;

#ifdef _MSC_VER
    localtime_s(&timeinfo, &now);
#else
    localtime_r(&now, &timeinfo);
#endif

    char time_buffer[16];
    char date_buffer[16];

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

    watch_ui_set_time(time_buffer);
    watch_ui_set_date(date_buffer);
}

int main(int argc, char **argv)
{
  (void)argc; /*Unused*/
  (void)argv; /*Unused*/

  /* Initialize LVGL */
  lv_init();

  /* Initialize display, input devices and tick */
  sdl_hal_init(410, 502);

  watch_ui_create();

  watch_ui_set_brightness_callback(
      brightness_changed
  );

  watch_ui_set_timeout_callback(
      timeout_changed
  );

  /* Initialize clock and update it every second */
  update_clock_cb(NULL);
  lv_timer_create(update_clock_cb, 1000, NULL);
  watch_ui_set_wifi(true, -65);
  watch_ui_set_battery(100, false);
  watch_ui_set_steps(1234); /* Mock value for validating the shared Home screen UI. */

  while(1) {
    /* Periodically call the lv_task handler.
     * It could be done in a timer interrupt or an OS task too.*/
    uint32_t sleep_time_ms = lv_timer_handler();
    if(sleep_time_ms == LV_NO_TIMER_READY){
	sleep_time_ms =  LV_DEF_REFR_PERIOD;
    }
#ifdef _MSC_VER
    Sleep(sleep_time_ms);
#else
    usleep(sleep_time_ms * 1000);
#endif
  }

  return 0;
}


#endif

/**********************
 *   STATIC FUNCTIONS
 **********************/

