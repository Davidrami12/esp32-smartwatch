#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
struct watch_media_t {
    uint32_t session = 0;
    uint8_t state = 0, actions = 0; // prev=1 toggle=2 next=4 volume-down=8 volume-up=16.
    char title[145] = {}, artist[97] = {};
};
class WatchMediaAssembler {
public:
    void reset() { used = total = 0; }
    int accept(const uint8_t *frame, size_t length, uint32_t now, watch_media_t &out) {
        if (used && static_cast<uint32_t>(now - last) > 5000) reset();
        if (!frame || length < 9 || length > 20 || frame[0] != 1 || frame[1] != 2) return reject();
        const uint16_t next_id = le16(frame+2), offset = le16(frame+4), size = le16(frame+6);
        if (size < 12 || size > 252) return reject();
        if (offset == 0) { reset(); total = size; id = next_id; }
        if (!total || size != total || id != next_id || offset != used || used + length - 8 > total) return reject();
        memcpy(buffer + used, frame+8, length-8); used += length-8; last = now;
        if (used != total) return 0;
        const size_t title = le16(buffer+8), artist = le16(buffer+10);
        if ((buffer[0] != 1 && buffer[0] != 2) || buffer[1] > 2 || buffer[2] > (buffer[0] == 1 ? 7 : 31) || buffer[3] || title > 144 || artist > 96 ||
            12+title+artist != total || !text_valid(buffer+12, title+artist)) return reject();
        watch_media_t parsed;
        parsed.state = buffer[1]; parsed.actions = buffer[2];
        for (unsigned i=0; i<4; ++i) parsed.session |= static_cast<uint32_t>(buffer[4+i]) << (8*i);
        if ((parsed.state == 0 && (parsed.session || parsed.actions || title || artist)) || (parsed.state && !parsed.session) ||
            !text_valid(buffer+12, title) || !text_valid(buffer+12+title, artist)) return reject();
        memcpy(parsed.title, buffer+12, title); memcpy(parsed.artist, buffer+12+title, artist);
        out = parsed; reset(); return 1;
    }
private:
    uint8_t buffer[252] = {};
    uint16_t id = 0, total = 0;
    size_t used = 0;
    uint32_t last = 0;
    int reject() { reset(); return -1; }
    static uint16_t le16(const uint8_t *p) { return p[0] | static_cast<uint16_t>(p[1]) << 8; }
    static bool text_valid(const uint8_t *p, size_t size) {
        for (size_t i=0; i<size; ++i) {
            if (p[i] >= 32 && p[i] <= 126) continue;
            if ((p[i] == 0xc2 || p[i] == 0xc3) && i+1 < size && p[i+1] >= 0x80 && p[i+1] <= 0xbf) {
                if (p[i] == 0xc2 && p[i+1] < 0xa0) return false;
                ++i; continue;
            }
            return false;
        }
        return true;
    }
};
