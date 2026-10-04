#pragma once
#include <stdint.h>

// Portable, monotonic elapsed-time arithmetic shared with host tests. No wall clock.
struct WatchTimeTool {
    uint64_t elapsed_ms = 0;
    uint32_t last_tick = 0;
    bool running = false;
    void advance(uint32_t tick) {
        if (running) elapsed_ms += static_cast<uint32_t>(tick - last_tick);
        last_tick = tick;
    }
    void toggle(uint32_t tick) { advance(tick); running = !running; }
    void reset(uint32_t tick) { elapsed_ms = 0; last_tick = tick; running = false; }
};
