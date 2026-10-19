#pragma once
#include "lvgl.h"

#if LV_USE_SYSMON && LV_USE_PERF_MONITOR
// Isolate the LVGL-private adapter here: both supported LVGL builds expose
// the same sysmon subject. No edits to dependency-managed LVGL are needed.
#include "src/display/lv_display_private.h"
#include "src/debugging/sysmon/lv_sysmon_private.h"

static void watch_perf_observer(lv_observer_t *observer,lv_subject_t *subject)
{
    auto *info=static_cast<const lv_sysmon_perf_info_t *>(lv_subject_get_pointer(subject));
    auto *text=static_cast<lv_obj_t *>(lv_observer_get_target(observer));
    lv_label_set_text_fmt(text,"%lu FPS | %lu%% CPU | %lu ms",
        static_cast<unsigned long>(info->calculated.fps),
        // The FreeRTOS OS-idle metric needs task-switch hooks and otherwise
        // reports a misleading permanent 100%. Use LVGL handler busy time on
        // both platforms: UI workload only, not whole-device CPU utilization.
        static_cast<unsigned long>(100-lv_timer_get_idle()),
        static_cast<unsigned long>(info->calculated.render_avg_time+info->calculated.flush_avg_time));
}

static inline lv_subject_t *watch_perf_subject(lv_subject_t &subject) { return &subject; }
static inline lv_subject_t *watch_perf_subject(lv_subject_t *subject) { return subject; }

static void watch_perf_monitor_create()
{
    auto *display=lv_display_get_default();
    if (!display) return;
    lv_sysmon_show_performance(display);
    lv_sysmon_hide_performance(display);
    auto *text=lv_label_create(lv_display_get_layer_sys(display));
    lv_obj_set_style_text_font(text,&lv_font_montserrat_14,0);
    lv_obj_set_style_text_color(text,lv_color_white(),0);
    lv_obj_set_style_bg_color(text,lv_color_black(),0);
    lv_obj_set_style_bg_opa(text,LV_OPA_50,0);
    lv_obj_set_style_pad_all(text,0,0);
    lv_obj_remove_flag(text,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(text,LV_ALIGN_BOTTOM_MID,0,0);
    lv_subject_add_observer_obj(watch_perf_subject(display->perf_sysmon_backend.subject),watch_perf_observer,text,nullptr);
}
#else
static void watch_perf_monitor_create() {}
#endif
