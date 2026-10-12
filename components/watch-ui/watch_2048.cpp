#include "watch_2048.h"
#include <string.h>

namespace {
uint32_t add_score(uint32_t a, uint32_t b) { return UINT32_MAX - a < b ? UINT32_MAX : a + b; }
unsigned index_for(Watch2048Direction direction, unsigned line, unsigned position)
{
    switch (direction) {
    case Watch2048Direction::Left: return line * 4 + position;
    case Watch2048Direction::Right: return line * 4 + 3 - position;
    case Watch2048Direction::Up: return position * 4 + line;
    case Watch2048Direction::Down: return (3 - position) * 4 + line;
    }
    return 0;
}
}

bool Watch2048::slide(uint32_t cells[16], Watch2048Direction direction, uint32_t &gain)
{
    gain = 0;
    if (static_cast<unsigned>(direction) > static_cast<unsigned>(Watch2048Direction::Down)) return false;
    bool changed = false;
    for (unsigned line = 0; line < 4; ++line) {
        uint32_t packed[4] = {}, output[4] = {};
        unsigned count = 0, written = 0;
        for (unsigned p = 0; p < 4; ++p) {
            const uint32_t tile = cells[index_for(direction, line, p)];
            if (tile) packed[count++] = tile;
        }
        for (unsigned p = 0; p < count; ++p) {
            uint32_t tile = packed[p];
            if (p + 1 < count && tile == packed[p + 1] && tile <= UINT32_MAX / 2) {
                tile *= 2; ++p; // Consume both originals; never merge the output again.
                gain = add_score(gain, tile);
            }
            output[written++] = tile;
        }
        for (unsigned p = 0; p < 4; ++p) {
            const unsigned index = index_for(direction, line, p);
            changed |= cells[index] != output[p];
            cells[index] = output[p];
        }
    }
    return changed;
}

uint32_t Watch2048::random()
{
    if (!random_state) random_state = 0x20482048;
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}
bool Watch2048::spawn()
{
    unsigned empty[16], count = 0;
    for (unsigned i = 0; i < 16; ++i) if (!board[i]) empty[count++] = i;
    if (!count) return false;
    const unsigned index = empty[random() % count];
    board[index] = random() % 10 == 0 ? 4 : 2; // Approximately 90% twos / 10% fours.
    return true;
}
void Watch2048::new_game()
{
    memset(board, 0, sizeof(board)); score = moves = 0;
    started = true; won = win_pending = game_over = false;
    spawn(); spawn();
}
bool Watch2048::can_move() const
{
    for (unsigned i = 0; i < 16; ++i) {
        if (!board[i]) return true;
        if (board[i] <= UINT32_MAX / 2 &&
            ((i % 4 < 3 && board[i] == board[i + 1]) || (i < 12 && board[i] == board[i + 4]))) return true;
    }
    return false;
}
bool Watch2048::move(Watch2048Direction direction)
{
    if (!started || win_pending || game_over) return false;
    uint32_t gain;
    if (!slide(board, direction, gain)) { game_over = !can_move(); return false; }
    score = add_score(score, gain);
    if (score > best) best = score;
    if (moves != UINT32_MAX) ++moves;
    spawn();
    if (!won) for (uint32_t tile : board) if (tile >= 2048) { won = win_pending = true; break; }
    game_over = !can_move();
    return true;
}
