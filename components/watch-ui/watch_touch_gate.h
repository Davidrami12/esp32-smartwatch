#pragma once
#include <stdint.h>

// Filter before LVGL hit-testing: wake touches never reach widgets, including
// the second tap and its release after the panel has become active.
struct WatchTouchGate {
    bool previous_pressed = false;
    bool blocked_until_release = false;
    struct Result { bool allow; bool wake_press; };
    Result sample(bool active, bool pressed) {
        const bool rising = pressed && !previous_pressed;
        previous_pressed = pressed;
        if (!active) {
            blocked_until_release = pressed;
            return {false,rising};
        }
        if (blocked_until_release) {
            if (!pressed) blocked_until_release = false;
            return {false,false};
        }
        return {true,false};
    }
    void sleep() { blocked_until_release = previous_pressed; }
};

// A separate raw-touch gesture, never a widget event. A press that began awake
// cannot become a shortcut when the panel turns off. Movement cancels until up.
struct WatchOffscreenHold {
    bool previous_pressed = false, tracking = false;
    uint32_t started = 0;
    int32_t origin_x = 0, origin_y = 0;
    bool sample(bool active, bool pressed, int32_t x, int32_t y, uint32_t now) {
        const bool rising = pressed && !previous_pressed;
        previous_pressed = pressed;
        if (active || !pressed) { tracking = false; return false; }
        if (rising) { tracking = true; started = now; origin_x = x; origin_y = y; }
        if (!tracking) return false;
        const int64_t dx = int64_t(x)-origin_x, dy = int64_t(y)-origin_y;
        if (dx*dx+dy*dy > 12*12) { tracking = false; return false; }
        if (uint32_t(now-started) < 1000) return false;
        tracking = false;
        return true;
    }
};
