#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

struct watch_notification_t {
    uint64_t timestamp_ms;
    char package[97], app[49], title[97], body[241];
};
// 20-byte ATT writes: version/type/id/offset/total (8 bytes), up to 12 data bytes.
class WatchNotificationAssembler {
public:
    void reset() { used = total = 0; }
    // -1 invalid, 0 partial, 1 complete. Completion requires exact total and field validation.
    int accept(const uint8_t *frame, size_t length, uint32_t now_ms, watch_notification_t &result) {
        if (used && static_cast<uint32_t>(now_ms - last_ms) > 5000) reset();
        if (!frame || length < 9 || length > 20 || frame[0] != 1 || frame[1] != 1) return reject();
        const uint16_t next_id = le16(frame + 2), offset = le16(frame + 4), next_total = le16(frame + 6);
        if (next_total < 16 || next_total > sizeof(buffer)) return reject();
        if (!offset) { reset(); id = next_id; total = next_total; }
        if (!total || id != next_id || total != next_total || offset != used || used + length - 8 > total) return reject();
        memcpy(buffer + used, frame + 8, length - 8);
        used += length - 8; last_ms = now_ms;
        if (used != total) return 0;
        watch_notification_t parsed = {};
        for (unsigned i = 0; i < 8; ++i) parsed.timestamp_ms |= static_cast<uint64_t>(buffer[i]) << (8 * i);
        size_t at = 16;
        const size_t capacities[] = {96, 48, 96, 240};
        char *destinations[] = {parsed.package, parsed.app, parsed.title, parsed.body};
        for (unsigned field = 0; field < 4; ++field) {
            const size_t count = le16(buffer + 8 + field * 2);
            if (count > capacities[field] || at + count > total || !valid_text(buffer + at, count)) return reject();
            memcpy(destinations[field], buffer + at, count); at += count;
        }
        if (at != total || !parsed.package[0] || (!parsed.title[0] && !parsed.body[0])) return reject();
        if (!parsed.app[0]) memcpy(parsed.app, "Phone", 6);
        result = parsed; reset(); return 1;
    }
private:
    uint8_t buffer[512] = {};
    uint16_t id = 0, total = 0;
    size_t used = 0;
    uint32_t last_ms = 0;
    int reject() { reset(); return -1; }
    static uint16_t le16(const uint8_t *p) { return p[0] | static_cast<uint16_t>(p[1]) << 8; }
    static bool valid_text(const uint8_t *text, size_t size) {
        // First-version font coverage: printable basic Latin and Latin-1 only. No emoji/control bytes.
        for (size_t i = 0; i < size; ++i) {
            if (text[i] >= 32 && text[i] <= 126) continue;
            if ((text[i] == 0xc2 || text[i] == 0xc3) && i + 1 < size && text[i + 1] >= 0x80 && text[i + 1] <= 0xbf) {
                if (text[i] == 0xc2 && text[i + 1] < 0xa0) return false;
                ++i; continue;
            }
            return false;
        }
        return true;
    }
};

class WatchNotificationQueue {
public:
    void push(const watch_notification_t &item) {
        if (count == 5) { first = (first + 1) % 5; --count; }
        items[(first + count) % 5] = item; ++count;
    }
    bool pop(watch_notification_t &item) {
        if (!count) return false;
        item = items[first]; first = (first + 1) % 5; --count; return true;
    }
    unsigned size() const { return count; }
private:
    watch_notification_t items[5] = {};
    unsigned first = 0, count = 0;
};
