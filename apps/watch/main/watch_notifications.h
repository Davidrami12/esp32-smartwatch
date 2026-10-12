#pragma once
#include "watch_notification_protocol.h"
// receive/reset on BLE host; pop on platform LVGL task. No shared UI dependencies.
int watch_notifications_receive(const uint8_t *frame, size_t length);
void watch_notifications_reset_transport();
bool watch_notifications_pop(watch_notification_t &item);
bool watch_notifications_take_arrival(void);
