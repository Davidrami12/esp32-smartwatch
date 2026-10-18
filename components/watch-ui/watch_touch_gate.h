#pragma once

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
