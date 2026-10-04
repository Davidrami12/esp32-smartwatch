// Include production UI so this headless integration test can inspect its private screens.
#include "../components/watch-ui/watch_ui.cpp"
#include <assert.h>
#include <initializer_list>

static unsigned brightness_calls, commit_calls;
static uint8_t committed;
static unsigned goal_calls;
static bool goal_save_ok = true;
static bool goal_test(uint32_t) { ++goal_calls; return goal_save_ok; }
static void brightness_test(uint8_t) { ++brightness_calls; }
static void commit_test(uint8_t value) { ++commit_calls; committed = value; }

static lv_obj_t *find_button(lv_obj_t *screen, const char *text)
{
    for (uint32_t i = 0; i < lv_obj_get_child_count(screen); ++i) {
        lv_obj_t *object = lv_obj_get_child(screen, i);
        if (!lv_obj_check_type(object, &lv_button_class) || !lv_obj_get_child_count(object)) continue;
        lv_obj_t *child = lv_obj_get_child(object, 0);
        if (lv_obj_check_type(child, &lv_label_class) && strcmp(lv_label_get_text(child), text) == 0) return object;
    }
    assert(false && "Button not found");
    return nullptr;
}

int main()
{
    lv_init();
    lv_display_create(410, 502);
    watch_ui_create();
    assert(navigation_animation(WatchScreen::Home, WatchScreen::Settings) == LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    assert(navigation_animation(WatchScreen::Settings, WatchScreen::Home) == LV_SCR_LOAD_ANIM_MOVE_LEFT);
    assert(navigation_animation(WatchScreen::Home, WatchScreen::Launcher) == LV_SCR_LOAD_ANIM_MOVE_LEFT);
    assert(navigation_animation(WatchScreen::Launcher, WatchScreen::Home) == LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    assert(navigation_animation(WatchScreen::Settings, WatchScreen::Wifi) == LV_SCR_LOAD_ANIM_MOVE_LEFT);
    assert(navigation_animation(WatchScreen::Wifi, WatchScreen::Settings) == LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    assert(navigation_animation(WatchScreen::Launcher, WatchScreen::Timer) == LV_SCR_LOAD_ANIM_MOVE_LEFT);
    assert(navigation_animation(WatchScreen::Timer, WatchScreen::Launcher) == LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    lv_obj_t *apps = find_button(screen_for(WatchScreen::Home), LV_SYMBOL_LIST "  APPS");
    lv_obj_t *settings = find_button(screen_for(WatchScreen::Home), LV_SYMBOL_SETTINGS "  SETTINGS");
    lv_obj_update_layout(screen_for(WatchScreen::Home));
    assert(lv_obj_get_x(settings) < lv_obj_get_x(apps));
    for (auto *nav : {settings, apps}) {
        assert(lv_obj_get_width(nav) == design::TileWidth && lv_obj_get_height(nav) == design::TouchHeight);
        assert(lv_obj_get_style_border_width(nav, LV_PART_MAIN) == design::BorderWidth);
        assert(lv_obj_get_style_bg_opa(nav, LV_PART_MAIN) == LV_OPA_COVER);
        assert(lv_color_eq(lv_obj_get_style_text_color(lv_obj_get_child(nav, 0), LV_PART_MAIN), lv_color_hex(kWhite)));
        lv_obj_add_state(nav, LV_STATE_PRESSED);
        assert(lv_obj_get_style_bg_opa(nav, LV_PART_MAIN) == LV_OPA_COVER);
        assert(lv_obj_get_style_border_width(nav, LV_PART_MAIN) == design::BorderWidth);
        lv_obj_remove_state(nav, LV_STATE_PRESSED);
    }
    lv_obj_send_event(settings, LV_EVENT_CLICKED, nullptr);
    assert(current_screen == WatchScreen::Settings);
    back_cb(nullptr);
    lv_obj_send_event(apps, LV_EVENT_CLICKED, nullptr);
    assert(current_screen == WatchScreen::Launcher);
    back_cb(nullptr);
    assert(weather_icon_color(WATCH_UI_WEATHER_SUNNY) == design::Weather);
    assert(weather_icon_color(WATCH_UI_WEATHER_RAIN) == design::WeatherRain);
    assert(weather_icon_color(WATCH_UI_WEATHER_CLOUDY) == design::WeatherCloudy);
    assert(weather_icon_color(WATCH_UI_WEATHER_THUNDERSTORM) == design::WeatherStorm);
    assert(weather_icon_color(WATCH_UI_WEATHER_WINDY) == design::WeatherWind);
    for (const auto parent : {WatchScreen::Launcher, WatchScreen::Settings}) {
        navigate_to(parent);
        lv_obj_send_event(find_button(screen_for(parent), LV_SYMBOL_HOME "  HOME"), LV_EVENT_CLICKED, nullptr);
        assert(current_screen == WatchScreen::Home);
    }
    for (const auto child : {WatchScreen::Activity, WatchScreen::Timer, WatchScreen::Stopwatch, WatchScreen::Weather,
                             WatchScreen::Wifi, WatchScreen::Bluetooth, WatchScreen::Brightness, WatchScreen::Timeout}) {
        navigate_to(child);
        lv_obj_send_event(find_button(screen_for(child), LV_SYMBOL_LEFT "  BACK"), LV_EVENT_CLICKED, nullptr);
        const bool settings_child = child == WatchScreen::Wifi || child == WatchScreen::Bluetooth ||
            child == WatchScreen::Brightness || child == WatchScreen::Timeout;
        assert(current_screen == (settings_child ? WatchScreen::Settings : WatchScreen::Launcher));
    }
    back_cb(nullptr);
    watch_ui_set_brightness_callback(brightness_test);
    watch_ui_set_brightness_committed_callback(commit_test);
    watch_ui_set_settings(50, 30000);
    watch_ui_set_step_goal_callback(goal_test);
    watch_ui_set_steps(5421);
    assert(strcmp(lv_label_get_text(steps_goal_label), "Goal 8,000") == 0);
    assert(lv_bar_get_value(steps_progress) == 5421);
    lv_obj_send_event(goal_plus, LV_EVENT_CLICKED, nullptr);
    assert(daily_step_goal == 9000 && goal_calls == 1);
    assert(strcmp(lv_label_get_text(activity_goal_label), "Goal 9,000") == 0);
    goal_save_ok = false;
    lv_obj_send_event(goal_plus, LV_EVENT_CLICKED, nullptr);
    assert(daily_step_goal == 9000 && goal_calls == 2);
    assert(strcmp(lv_label_get_text(activity_goal_label), "Save failed / retry") == 0);
    goal_save_ok = true;
    watch_ui_set_step_goal(1000);
    assert(lv_obj_has_state(goal_minus, LV_STATE_DISABLED));
    assert(lv_bar_get_value(steps_progress) == 1000);
    lv_obj_send_event(goal_minus, LV_EVENT_CLICKED, nullptr);
    assert(daily_step_goal == 1000 && goal_calls == 2);
    watch_ui_set_step_goal(30000);
    assert(lv_obj_has_state(goal_plus, LV_STATE_DISABLED));
    lv_obj_send_event(goal_plus, LV_EVENT_CLICKED, nullptr);
    assert(daily_step_goal == 30000 && goal_calls == 2);
    watch_ui_set_step_goal(0); watch_ui_set_step_goal(3500);
    assert(daily_step_goal == 30000);
    watch_ui_set_steps(UINT32_MAX);
    assert(lv_bar_get_value(steps_progress) == 30000);
    watch_ui_set_steps(0);
    assert(lv_bar_get_value(steps_progress) == 0);
    watch_ui_set_step_goal(8000);
    lv_obj_send_event(goal_minus, LV_EVENT_CLICKED, nullptr);
    assert(daily_step_goal == 7000 && goal_calls == 3);
    watch_ui_set_step_goal(8000);
    assert(strcmp(lv_label_get_text(settings_brightness_value), "50%") == 0);
    assert(strcmp(lv_label_get_text(settings_timeout_value), "30 sec") == 0);
    lv_obj_t *plus = find_button(screen_for(WatchScreen::Brightness), "+");
    lv_obj_t *minus = find_button(screen_for(WatchScreen::Brightness), "-");
    lv_obj_send_event(plus, LV_EVENT_CLICKED, nullptr);
    assert(current_brightness == 55 && committed == 55 && brightness_calls == 1 && commit_calls == 1);
    assert(strcmp(lv_label_get_text(settings_brightness_value), "55%") == 0);
    lv_obj_send_event(minus, LV_EVENT_CLICKED, nullptr);
    assert(current_brightness == 50 && committed == 50);
    watch_ui_set_settings(100, 30000);
    unsigned before = commit_calls;
    lv_obj_send_event(plus, LV_EVENT_CLICKED, nullptr);
    assert(current_brightness == 100 && commit_calls == before);
    watch_ui_set_settings(10, 30000);
    lv_obj_send_event(minus, LV_EVENT_CLICKED, nullptr);
    assert(current_brightness == 10 && commit_calls == before);
    lv_dropdown_set_selected(screen_timeout_dropdown, 4);
    lv_obj_send_event(screen_timeout_dropdown, LV_EVENT_VALUE_CHANGED, nullptr);
    assert(strcmp(lv_label_get_text(settings_timeout_value), "Never") == 0);

    open_settings_cb(nullptr); open_wifi_cb(nullptr); back_cb(nullptr);
    assert(current_screen == WatchScreen::Settings);
    back_cb(nullptr); assert(current_screen == WatchScreen::Home);
    open_launcher_cb(nullptr); open_timer_cb(nullptr);
    timer_minutes = 1;
    timer_control_cb(nullptr);
    lv_tick_inc(1000); time_tools_tick(nullptr);
    assert(strcmp(lv_label_get_text(timer_value), "00:00:59") == 0);
    timer_control_cb(nullptr);
    lv_tick_inc(5000); time_tools_tick(nullptr);
    assert(countdown.elapsed_ms == 1000 && !countdown.running);
    timer_control_cb(nullptr); stopwatch_control_cb(nullptr);
    watch_ui_set_display_active(false);
    lv_tick_inc(59000); time_tools_tick(nullptr);
    assert(!countdown.running && countdown.elapsed_ms == 60000);
    assert(stopwatch.running && stopwatch.elapsed_ms == 59000);
    // Expiry while IDLE must not redraw until wake.
    assert(strcmp(lv_label_get_text(timer_value), "00:00:59") == 0);
    watch_ui_set_display_active(true);
    assert(strcmp(lv_label_get_text(timer_value), "00:00:00") == 0);
    assert(strcmp(lv_label_get_text(timer_status), "TIME'S UP") == 0);
    back_cb(nullptr); open_stopwatch_cb(nullptr);
    assert(strcmp(lv_label_get_text(stopwatch_value), "00:00:59.0") == 0);
    stopwatch_control_cb(nullptr); stopwatch_reset_cb(nullptr);
    assert(stopwatch.elapsed_ms == 0 && !stopwatch.running);
    back_cb(nullptr); assert(current_screen == WatchScreen::Launcher);
    watch_ui_forecast_day_t forecast[7] = {};
    forecast[0] = {true, 20261003, 6, 12, 24, WATCH_UI_WEATHER_SUNNY};
    watch_ui_set_forecast(forecast, false, 45);
    assert(strcmp(lv_label_get_text(weather_range_label), "H 24\xC2\xB0 / L 12\xC2\xB0") == 0);
    open_weather_cb(nullptr);
    assert(strcmp(lv_label_get_text(forecast_names[0]), "TODAY 03/10") == 0);
    assert(strcmp(lv_label_get_text(forecast_values[0]), "24 / 12") == 0);
    assert(strstr(lv_label_get_text(forecast_status), "OFFLINE / CACHED"));
    assert(strcmp(lv_label_get_text(forecast_values[1]), "-- / --") == 0);
    watch_ui_set_forecast(nullptr, true, 0);
    assert(strcmp(lv_label_get_text(weather_range_label), "H -- / L --") == 0);
    assert(strcmp(lv_label_get_text(forecast_values[0]), "-- / --") == 0);
    back_cb(nullptr); assert(current_screen == WatchScreen::Launcher);
    puts("PASS: UI +/- callbacks/bounds, navigation, countdown expiry, simultaneous stopwatch, IDLE/wake refresh");
}
