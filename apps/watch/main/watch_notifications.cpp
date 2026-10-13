#include "watch_notifications.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include <atomic>

namespace {
WatchNotificationAssembler assembler;
WatchNotificationQueue queue;
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
std::atomic<bool> arrival{false};
}
int watch_notifications_receive(const uint8_t *frame, size_t length)
{
    watch_notification_t item;
    const int result = assembler.accept(frame, length, static_cast<uint32_t>(esp_timer_get_time() / 1000), item);
    if (result < 0) ESP_LOGW("watch_notifications", "Invalid/incomplete frame rejected");
    if (result == 1) {
        portENTER_CRITICAL(&lock); queue.push(item); const unsigned count = queue.size(); portEXIT_CRITICAL(&lock);
        arrival = true;
        ESP_LOGI("watch_notifications", "Notification received; pending=%u", count);
    }
    return result;
}
void watch_notifications_reset_transport() { assembler.reset(); }
bool watch_notifications_pop(watch_notification_t &item)
{
    portENTER_CRITICAL(&lock); const bool result = queue.pop(item); portEXIT_CRITICAL(&lock); return result;
}
bool watch_notifications_take_arrival(void) { return arrival.exchange(false); }
