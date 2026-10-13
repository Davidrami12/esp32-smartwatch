#pragma once
#include "watch_media_protocol.h"
int watch_media_receive(const uint8_t *bytes, size_t length);
void watch_media_reset();
bool watch_media_snapshot(watch_media_t &out, uint32_t &revision);
bool watch_media_queue_command(uint8_t command, uint32_t session);
bool watch_media_take_command(uint8_t out[8]);
