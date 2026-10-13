#include "watch_media.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
namespace {
WatchMediaAssembler assembler;
watch_media_t media;
uint32_t version = 1;
uint16_t sequence = 0;
uint8_t pending = 0;
uint32_t pending_session = 0;
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
}
int watch_media_receive(const uint8_t *bytes, size_t length) {
    watch_media_t parsed;
    const int result = assembler.accept(bytes, length, static_cast<uint32_t>(esp_timer_get_time()/1000), parsed);
    if (result == 1) {
        portENTER_CRITICAL(&lock); media = parsed; ++version; portEXIT_CRITICAL(&lock);
        ESP_LOGI("watch_media", "Media snapshot applied state=%u actions=%u", parsed.state, parsed.actions);
    } else if (result < 0) { watch_media_reset(); ESP_LOGW("watch_media", "Invalid media frame rejected"); }
    return result;
}
void watch_media_reset() {
    assembler.reset();
    portENTER_CRITICAL(&lock); media = {}; pending = 0; ++version; portEXIT_CRITICAL(&lock);
}
bool watch_media_snapshot(watch_media_t &out, uint32_t &revision) {
    portENTER_CRITICAL(&lock);
    const bool changed = revision != version;
    if (changed) { out = media; revision = version; }
    portEXIT_CRITICAL(&lock); return changed;
}
bool watch_media_queue_command(uint8_t command, uint32_t session) {
    if (command < 1 || command > 5) return false;
    portENTER_CRITICAL(&lock);
    const bool allowed = !pending && media.state && media.session == session && (media.actions & (1 << (command-1)));
    if (allowed) { pending = command; pending_session = session; }
    portEXIT_CRITICAL(&lock); return allowed;
}
bool watch_media_take_command(uint8_t out[8]) {
    portENTER_CRITICAL(&lock);
    const bool allowed = pending && media.state && media.session == pending_session && (media.actions & (1 << (pending-1)));
    if (allowed) {
        out[0] = 1; out[1] = pending; ++sequence;
        for (unsigned i=0; i<4; ++i) out[2+i] = pending_session >> (8*i);
        out[6] = sequence; out[7] = sequence >> 8;
    }
    pending = 0;
    portEXIT_CRITICAL(&lock); return allowed;
}
