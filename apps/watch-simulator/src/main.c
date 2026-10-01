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
    char date_buffer[40];

    strftime(
        time_buffer,
        sizeof(time_buffer),
        "%H:%M:%S",
        &timeinfo
    );

    static const char *weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    snprintf(date_buffer, sizeof(date_buffer), "%s  %02d/%02d/%04d", weekdays[timeinfo.tm_wday],
             timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);

    watch_ui_set_time(time_buffer);
    watch_ui_set_date(date_buffer);
}

/* Alternate mock connectivity so both status states can be inspected without hardware. */
static watch_ui_wifi_state_t mock_wifi = WATCH_UI_WIFI_CONNECTED;
static unsigned mock_connect_ticks = 0;

static bool mock_wifi_control(bool enabled)
{
    mock_wifi = enabled ? WATCH_UI_WIFI_CONNECTING : WATCH_UI_WIFI_DISCONNECTED;
    mock_connect_ticks = 0;
    watch_ui_set_wifi_state(mock_wifi, 0);
    return true;
}

static void update_mock_wifi_cb(lv_timer_t *timer)
{
    (void)timer;
    if (mock_wifi == WATCH_UI_WIFI_CONNECTING && ++mock_connect_ticks >= 3) mock_wifi = WATCH_UI_WIFI_CONNECTED;
    watch_ui_set_wifi_state(mock_wifi, mock_wifi == WATCH_UI_WIFI_CONNECTED ? -65 : 0);
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
  watch_ui_set_wifi_control_callback(mock_wifi_control);
  watch_ui_set_wifi_state(WATCH_UI_WIFI_CONNECTED, -65);
  lv_timer_create(update_mock_wifi_cb, 1000, NULL);
  watch_ui_set_weather(true, 24, WATCH_UI_WEATHER_SUNNY);
  {
    const uint32_t counts[7] = {1234, 8140, 0, 7283, 4612, 0, 9050};
    watch_ui_activity_day_t days[7];
    time_t now = time(NULL);
    struct tm local;
#ifdef _MSC_VER
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    local.tm_hour = 12;
    for (unsigned i = 0; i < 7; ++i) {
        local.tm_isdst = -1;
        mktime(&local);
        days[i].steps = counts[i];
        days[i].weekday = (uint8_t)local.tm_wday;
        --local.tm_mday;
    }
    watch_ui_set_history(days);
  }
  watch_ui_set_battery(100, false);
  watch_ui_set_steps(1234); /* Shared Home and Activity daily-count mock. */

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

