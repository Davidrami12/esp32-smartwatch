#pragma once
#include <stddef.h>
#include <stdint.h>

// Pure protocol state; caller serializes on the audio mutex / NimBLE host queue.
struct WatchVoiceWindow {
    static constexpr uint8_t MaxCredits = 4, Header = 15;
    uint32_t session = 0, epoch = 0, offset = 0;
    uint8_t credits = 0, sequence = 0;
    void clear() { session = epoch = offset = 0; credits = sequence = 0; }
    bool grant(uint32_t clip, uint32_t generation, uint32_t at, uint8_t count, uint32_t size) {
        if (!clip || !generation || !count || count > MaxCredits || at >= size || (at & 1)) return false;
        if (session && (session != clip || generation <= epoch)) return false;
        session = clip; epoch = generation; offset = at; credits = count; sequence = 0;
        return true;
    }
    size_t payload(size_t capacity, uint32_t size) const {
        if (!credits || offset >= size || capacity <= Header) return 0;
        size_t count = (capacity-Header) & ~size_t(1);
        return count < size-offset ? count : size-offset;
    }
    bool last(size_t count, uint32_t size) const { return credits == 1 || offset+count == size; }
    void sent(size_t count, uint32_t size) {
        offset += static_cast<uint32_t>(count); ++sequence;
        if (credits) --credits;
        if (offset >= size) credits = 0;
    }
};
