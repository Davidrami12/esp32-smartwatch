#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
// Value 4 was local playback; keep subsequent UI state values stable.
enum class WatchVoiceState : uint8_t { Idle, Starting, Recording, Ready, Error = 5, Transferring, Processing, Complete };
struct WatchVoiceStatus {
    WatchVoiceState state; uint32_t duration_ms; uint8_t level; const char *error;
    uint32_t confirmed_bytes = 0, total_bytes = 0;
};
esp_err_t watch_voice_init(); // Task only; no microphone activation.
void watch_voice_set_microphone_gain(uint8_t gain_db); // Applied at next capture.
bool watch_voice_preview_volume(uint8_t percent); // Explicit local 180 ms tone; 0 is silent.
const char *watch_voice_take_preview_error();
bool watch_voice_start();
void watch_voice_release();
void watch_voice_cancel();
WatchVoiceStatus watch_voice_status();
// Secure GATT pull transfer; only a completed capture can be claimed.
size_t watch_voice_metadata(uint8_t bytes[18]);
bool watch_voice_select(uint32_t session, uint32_t offset, uint8_t operation);
size_t watch_voice_chunk(uint8_t *bytes, size_t capacity);
bool watch_voice_grant(uint32_t session, uint32_t epoch, uint32_t offset, uint8_t credits);
size_t watch_voice_stream_chunk(uint8_t *bytes, size_t capacity);
void watch_voice_stream_sent(uint32_t session, uint32_t epoch, size_t payload);
void watch_voice_stop_stream();
void watch_voice_disconnected();
bool watch_voice_result(const uint8_t *bytes, size_t count);
bool watch_voice_copy_text(uint32_t *revision, char *transcription, char *answer);
