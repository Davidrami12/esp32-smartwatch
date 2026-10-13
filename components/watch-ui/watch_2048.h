#pragma once
#include <stdint.h>

enum class Watch2048Direction { Left, Right, Up, Down };

// Plain fixed-size state, independent of LVGL, ESP-IDF and storage.
struct Watch2048 {
    uint32_t board[16] = {};
    uint32_t score = 0, best = 0;
    uint32_t random_state = 0x20482048;
    uint32_t moves = 0;
    bool started = false, won = false, win_pending = false, game_over = false;

    void new_game(); // Retains best and RNG state; spawns exactly two tiles.
    bool move(Watch2048Direction direction); // Invalid moves neither score nor spawn.
    bool can_move() const;
    void continue_game() { win_pending = false; }
    // Pure board transform used by production moves and deterministic tests.
    static bool slide(uint32_t board[16], Watch2048Direction direction, uint32_t &gain);
private:
    uint32_t random();
    bool spawn();
};
