#include "watch_2048.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned occupied(const Watch2048 &game)
{
    unsigned count = 0; for (auto tile : game.board) if (tile) ++count; return count;
}
int main()
{
    uint32_t gain;
    uint32_t left[16] = {2,2,2,2, 2,2,4,0, 4,0,4,8, 0,0,0,0};
    assert(Watch2048::slide(left, Watch2048Direction::Left, gain));
    const uint32_t expected[16] = {4,4,0,0, 4,4,0,0, 8,8,0,0, 0,0,0,0};
    assert(!memcmp(left, expected, sizeof(left)) && gain == 20);
    assert(Watch2048::slide(left, Watch2048Direction::Left, gain));
    assert(gain == 32 && left[0] == 8 && left[4] == 8 && left[8] == 16);
    uint32_t right[16] = {2,2,2,2};
    assert(Watch2048::slide(right, Watch2048Direction::Right, gain));
    assert(right[0] == 0 && right[1] == 0 && right[2] == 4 && right[3] == 4 && gain == 8);
    uint32_t up[16] = {2,0,0,0, 2,0,0,0, 4,0,0,0, 0,0,0,0};
    assert(Watch2048::slide(up, Watch2048Direction::Up, gain));
    assert(up[0] == 4 && up[4] == 4 && up[8] == 0 && gain == 4);
    uint32_t down[16] = {2,0,0,0, 2,0,0,0, 4,0,0,0, 0,0,0,0};
    assert(Watch2048::slide(down, Watch2048Direction::Down, gain));
    assert(down[12] == 4 && down[8] == 4 && down[4] == 0 && gain == 4);
    uint32_t triple[16] = {2,2,2,0};
    assert(Watch2048::slide(triple, Watch2048Direction::Left, gain));
    assert(triple[0] == 4 && triple[1] == 2 && gain == 4);

    Watch2048 game; game.new_game(); assert(occupied(game) == 2 && game.score == 0);
    memset(game.board, 0, sizeof(game.board)); game.board[0] = 2;
    const uint32_t seed = game.random_state;
    assert(!game.move(Watch2048Direction::Left));
    assert(game.random_state == seed && occupied(game) == 1 && game.moves == 0);
    assert(game.move(Watch2048Direction::Right));
    assert(occupied(game) == 2 && game.board[3] == 2 && game.moves == 1);
    memset(game.board, 0, sizeof(game.board)); game.board[0] = game.board[1] = 2;
    assert(game.move(Watch2048Direction::Left)); assert(game.score == 4 && game.best == 4 && occupied(game) == 2);
    game.new_game(); assert(game.best == 4 && game.score == 0 && occupied(game) == 2);
    memset(game.board, 0, sizeof(game.board)); game.board[0] = game.board[1] = 1024;
    assert(game.move(Watch2048Direction::Left)); assert(game.won && game.win_pending && game.score == 2048 && game.best == 2048);
    assert(!game.move(Watch2048Direction::Down)); game.continue_game(); assert(game.move(Watch2048Direction::Down));
    assert(game.won && !game.win_pending);

    const uint32_t stuck[16] = {2,4,2,4, 4,2,4,2, 2,4,2,4, 4,2,4,2};
    memcpy(game.board, stuck, sizeof(stuck)); game.game_over = false;
    assert(!game.can_move()); assert(!game.move(Watch2048Direction::Left)); assert(game.game_over);
    game.board[15] = 0; assert(game.can_move()); game.board[15] = 4; assert(game.can_move());
    game.new_game(); game.score = UINT32_MAX - 1; game.board[0] = game.board[1] = 2;
    assert(game.move(Watch2048Direction::Left)); assert(game.score == UINT32_MAX && game.best == UINT32_MAX);
    game.new_game(); memset(game.board, 0, sizeof(game.board)); game.board[0] = game.board[1] = 0x80000000u;
    assert(!Watch2048::slide(game.board, Watch2048Direction::Left, gain)); // No tile overflow.
    Watch2048 a, b; a.random_state = b.random_state = 42; a.new_game(); b.new_game();
    assert(!memcmp(a.board, b.board, sizeof(a.board)));
    a.move(Watch2048Direction::Down); b.move(Watch2048Direction::Down);
    assert(!memcmp(a.board, b.board, sizeof(a.board)) && a.score == b.score);
    puts("PASS: 2048 four directions, single merges, scoring, spawn/no-op, win/continue, game over, best/reset, overflow and deterministic RNG");
}
