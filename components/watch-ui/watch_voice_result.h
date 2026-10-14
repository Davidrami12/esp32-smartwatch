#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
// Bounded, contiguous UTF-8 result assembly. No allocation and no logs.
struct WatchVoiceResult {
    char transcription[513] = {}, answer[3073] = {};
    uint16_t offset = 0, total = 0;
    uint8_t kind = 0;
    static bool utf8(const char *text, size_t length) {
        for (size_t i = 0; i < length;) {
            const uint8_t first = uint8_t(text[i++]);
            if (first < 0x80) { if (first < 0x20 && first != '\n' && first != '\t') return false; continue; }
            unsigned rest = first >= 0xc2 && first <= 0xdf ? 1 : first >= 0xe0 && first <= 0xef ? 2 : first >= 0xf0 && first <= 0xf4 ? 3 : 0;
            if (!rest || i + rest > length) return false;
            uint32_t value = first & (rest == 1 ? 0x1f : rest == 2 ? 0xf : 7);
            const unsigned width = rest;
            while (rest--) { const uint8_t byte = uint8_t(text[i++]); if ((byte & 0xc0) != 0x80) return false; value = (value << 6) | (byte & 0x3f); }
            if (value < (width == 1 ? 0x80u : width == 2 ? 0x800u : 0x10000u) || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return false;
        }
        return true;
    }
    void clear() { memset(this, 0, sizeof(*this)); }
    bool accept(uint8_t type, uint16_t at, uint16_t length, const uint8_t *data, size_t count) {
        const size_t maximum = type == 1 ? 512 : (type == 2 ? 3072 : 160);
        if (type < 1 || type > 3 || !length || length > maximum || !count || at + count > length) return false;
        if (at == 0) { kind = type; offset = 0; total = length; }
        if (kind != type || total != length || at != offset) return false;
        char *target = type == 1 ? transcription : answer;
        for (size_t i = 0; i < count; ++i) if (!data[i]) return false;
        memcpy(target + at, data, count); offset += count; target[offset] = 0;
        if (offset == total && !utf8(target,total)) { target[0] = 0; offset = total = 0; return false; }
        return true;
    }
    bool complete() const { return total && offset == total; }
};
