#include "watch_voice.h"
#include "bsp/esp-bsp.h"
#include "esp_codec_dev.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_random.h"
#include "esp_log.h"
#include "watch_voice_result.h"
#include "watch_voice_window.h"
#include <atomic>
#include <string.h>

namespace {
constexpr unsigned kRate = 16000, kFrames = 320, kMaxSamples = kRate * 8;
constexpr uint32_t kStart = 1, kPlay = 2, kCancel = 4;
constexpr uint32_t kProcess = 8;
std::atomic<uint32_t> request_id{0};
WatchVoiceResult result_text;
uint32_t text_revision = 0;
TaskHandle_t task;
esp_codec_dev_handle_t microphone, speaker;
int16_t *pcm;
size_t samples;
std::atomic<int64_t> expires{0};
SemaphoreHandle_t audio_lock;
uint32_t session_id = 0, selected_offset = 0, checksum = 0;
std::atomic<int64_t> released_us{0};
uint32_t diagnostic_session = 0;
WatchVoiceWindow stream_window;
uint32_t crc32(const uint8_t *bytes, size_t count)
{
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < count; ++i) { crc ^= bytes[i]; for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u-(crc & 1))); }
    return ~crc;
}
void put32(uint8_t *out, uint32_t value) { for (unsigned i = 0; i < 4; ++i) out[i] = value >> (8*i); }
std::atomic<WatchVoiceState> state{WatchVoiceState::Idle};
std::atomic<bool> held{false}, cancelled{false};
portMUX_TYPE status_lock = portMUX_INITIALIZER_UNLOCKED;
WatchVoiceStatus status{WatchVoiceState::Idle,0,0,nullptr};
void publish(WatchVoiceState next, const char *error = nullptr, uint8_t level = 0)
{
    portENTER_CRITICAL(&status_lock);
    status = {next, static_cast<uint32_t>(samples * 1000 / kRate), level, error};
    portEXIT_CRITICAL(&status_lock);
    state = next;
    if (next == WatchVoiceState::Ready) ESP_LOGI("watch_voice", "Capture completed: samples=%u bytes=%u duration_ms=%u", unsigned(samples), unsigned(samples * 2), unsigned(samples * 1000 / kRate));
    if (next == WatchVoiceState::Error) ESP_LOGW("watch_voice", "Audio operation failed: %s", error ? error : "unknown");
}
void clear_audio()
{
    xSemaphoreTake(audio_lock,portMAX_DELAY);
    if (pcm) {
        volatile uint8_t *bytes = reinterpret_cast<volatile uint8_t *>(pcm);
        for (size_t i = 0; i < kMaxSamples * sizeof(int16_t); ++i) bytes[i] = 0;
        heap_caps_free(pcm); pcm = nullptr;
    }
    samples = 0; expires = 0;
    session_id = selected_offset = checksum = 0;
    stream_window.clear();
    xSemaphoreGive(audio_lock);
}
bool codecs()
{
    if (microphone && speaker) return true;
    // Official board pin mapping; only initialized after explicit hold.
    i2s_std_config_t config = {};
    config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(kRate);
    config.slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_STEREO);
    config.gpio_cfg.mclk = BSP_I2S_MCLK; config.gpio_cfg.bclk = BSP_I2S_SCLK;
    config.gpio_cfg.ws = BSP_I2S_LCLK; config.gpio_cfg.dout = BSP_I2S_DOUT; config.gpio_cfg.din = BSP_I2S_DSIN;
    if (bsp_audio_init(&config) != ESP_OK) return false;
    if (!microphone) microphone = bsp_audio_codec_microphone_init();
    if (!speaker) speaker = bsp_audio_codec_speaker_init();
    if (microphone) esp_codec_dev_close(microphone);
    if (speaker) esp_codec_dev_close(speaker);
    return microphone && speaker;
}
void record()
{
    clear_audio();
    if (!codecs()) { publish(WatchVoiceState::Error,"Audio initialization failed"); return; }
    if (!held || cancelled) { publish(WatchVoiceState::Idle); return; }
    pcm = static_cast<int16_t *>(heap_caps_calloc(kMaxSamples,sizeof(int16_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!pcm) { publish(WatchVoiceState::Error,"Audio buffer unavailable"); return; }
    esp_codec_dev_sample_info_t format = {};
    format.sample_rate = kRate; format.channel = 2; format.bits_per_sample = 16;
    if (esp_codec_dev_open(microphone,&format) != ESP_CODEC_DEV_OK) {
        esp_codec_dev_close(microphone); clear_audio(); publish(WatchVoiceState::Error,"Microphone open failed"); return;
    }
    if (esp_codec_dev_set_in_gain(microphone,24.0f) != ESP_CODEC_DEV_OK) {
        esp_codec_dev_close(microphone); clear_audio(); publish(WatchVoiceState::Error,"Microphone gain setup failed"); return;
    }
    int16_t raw[kFrames*2];
    bool failed = false;
    const int64_t started = esp_timer_get_time();
    publish(WatchVoiceState::Recording);
    while (held && !cancelled && samples+kFrames <= kMaxSamples && esp_timer_get_time()-started < 8'000'000) {
        if (esp_codec_dev_read(microphone,raw,sizeof(raw)) != ESP_CODEC_DEV_OK) { failed = true; break; }
        // Release/cancel must not retain samples read after the user stopped.
        if (!held || cancelled) break;
        unsigned peak = 0;
        for (unsigned i = 0; i < kFrames; ++i) {
            const int32_t mono = (static_cast<int32_t>(raw[2*i])+raw[2*i+1])/2;
            pcm[samples++] = static_cast<int16_t>(mono);
            const unsigned absolute = mono < 0 ? -mono : mono;
            if (absolute > peak) peak = absolute;
        }
        publish(WatchVoiceState::Recording,nullptr,static_cast<uint8_t>(peak*100/32768));
    }
    volatile uint8_t *scratch = reinterpret_cast<volatile uint8_t *>(raw);
    for (size_t i = 0; i < sizeof(raw); ++i) scratch[i] = 0;
    const int close_result = esp_codec_dev_close(microphone);
    held = false;
    if (cancelled) { clear_audio(); publish(WatchVoiceState::Idle); }
    else if (failed || close_result != ESP_CODEC_DEV_OK) { clear_audio(); publish(WatchVoiceState::Error,"Microphone capture failed"); }
    else if (samples < kRate/4) { clear_audio(); publish(WatchVoiceState::Error,"Hold at least 0.25 seconds"); }
    else {
        session_id = esp_random(); if (!session_id) session_id = 1;
        diagnostic_session = session_id;
        selected_offset = 0; checksum = crc32(reinterpret_cast<const uint8_t *>(pcm),samples*sizeof(int16_t));
        expires = esp_timer_get_time()+30'000'000; publish(WatchVoiceState::Ready);
        ESP_LOGI("watch_voice","VoiceLatency trace=%08lx stage=finalized mono_us=%lld release_to_ready_us=%lld",static_cast<unsigned long>(session_id),esp_timer_get_time(),released_us.load() ? esp_timer_get_time()-released_us.load() : -1LL);
    }
}
void preview()
{
    esp_codec_dev_sample_info_t format = {};
    format.sample_rate = kRate; format.channel = 1; format.bits_per_sample = 16;
    bool failed = !pcm || esp_codec_dev_open(speaker,&format) != ESP_CODEC_DEV_OK;
    if (!failed) {
        failed = esp_codec_dev_set_out_vol(speaker,35) != ESP_CODEC_DEV_OK;
        for (size_t at = 0; !failed && at < samples && !cancelled; at += kFrames) {
            const size_t count = samples-at < kFrames ? samples-at : kFrames;
            if (esp_codec_dev_write(speaker,pcm+at,count*sizeof(int16_t)) != ESP_CODEC_DEV_OK) { failed = true; break; }
        }
    }
    if (speaker) esp_codec_dev_close(speaker);
    clear_audio();
    publish(failed ? WatchVoiceState::Error : WatchVoiceState::Idle,failed ? "Playback failed" : nullptr);
}
void worker(void *)
{
    while (true) {
        uint32_t command = 0;
        xTaskNotifyWait(0,UINT32_MAX,&command,pdMS_TO_TICKS(100));
        if ((command & kCancel) && cancelled) { clear_audio(); publish(WatchVoiceState::Idle); continue; }
        if (command & kProcess) { clear_audio(); expires = esp_timer_get_time()+90'000'000; }
        if (command & kStart) record();
        else if (command & kPlay) preview();
        if (expires && esp_timer_get_time() >= expires) {
            auto ready = WatchVoiceState::Ready;
            if (state.compare_exchange_strong(ready,WatchVoiceState::Starting)) {
                ESP_LOGW("watch_voice", "Unclaimed capture expired after 30 seconds; no phone transfer claim received");
                clear_audio(); publish(WatchVoiceState::Error,"Phone did not claim audio. Enable companion; hold to retry");
            }
            else if (state == WatchVoiceState::Transferring) { clear_audio(); publish(WatchVoiceState::Error,"Phone transfer timed out"); }
            else if (state == WatchVoiceState::Processing) { request_id = 0; expires = 0; publish(WatchVoiceState::Error,"AI request timed out. Hold to retry"); }
        }
    }
}
}
esp_err_t watch_voice_init()
{
    audio_lock = xSemaphoreCreateMutex();
    if (!audio_lock) return ESP_ERR_NO_MEM;
    return xTaskCreate(worker,"voice_capture",6144,nullptr,4,&task) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
bool watch_voice_start()
{
    if (!task) return false;
    auto old = state.load();
    if (old != WatchVoiceState::Idle && old != WatchVoiceState::Ready && old != WatchVoiceState::Error && old != WatchVoiceState::Complete) return false;
    if (!state.compare_exchange_strong(old,WatchVoiceState::Starting)) return false;
    xSemaphoreTake(audio_lock,portMAX_DELAY); result_text.clear(); ++text_revision; xSemaphoreGive(audio_lock);
    request_id = 0;
    released_us = 0;
    cancelled = false; held = true; xTaskNotify(task,kStart,eSetBits); return true;
}
void watch_voice_release() { released_us = esp_timer_get_time(); held = false; }
void watch_voice_cancel() {
    request_id = 0; cancelled = true; held = false;
    if (audio_lock) { xSemaphoreTake(audio_lock,portMAX_DELAY); stream_window.clear(); result_text.clear(); ++text_revision; xSemaphoreGive(audio_lock); }
    if (task) xTaskNotify(task,kCancel,eSetBits);
}
bool watch_voice_preview()
{
    auto old = WatchVoiceState::Ready;
    if (!task || !state.compare_exchange_strong(old,WatchVoiceState::Playing)) return false;
    cancelled = false; xTaskNotify(task,kPlay,eSetBits); return true;
}
WatchVoiceStatus watch_voice_status()
{
    portENTER_CRITICAL(&status_lock); const auto result = status; portEXIT_CRITICAL(&status_lock);
    return {state.load(),result.duration_ms,result.level,result.error};
}
size_t watch_voice_metadata(uint8_t bytes[18])
{
    memset(bytes,0,18); bytes[0] = 1;
    if (!audio_lock) return 18;
    xSemaphoreTake(audio_lock,portMAX_DELAY);
    if (state == WatchVoiceState::Processing && request_id) { bytes[1] = 3; put32(bytes+2,request_id); }
    if ((state == WatchVoiceState::Ready || state == WatchVoiceState::Transferring) && pcm) {
        bytes[1] = state == WatchVoiceState::Ready ? 1 : 2;
        put32(bytes+2,session_id); put32(bytes+6,samples*sizeof(int16_t)); put32(bytes+10,checksum);
        bytes[14] = 0x80; bytes[15] = 0x3e; bytes[16] = 16; bytes[17] = 1;
    }
    xSemaphoreGive(audio_lock); return 18;
}
bool watch_voice_select(uint32_t session, uint32_t offset, uint8_t operation)
{
    if (!audio_lock) return false;
    xSemaphoreTake(audio_lock,portMAX_DELAY);
    bool valid = pcm && session && session == session_id && offset <= samples*sizeof(int16_t) && offset % 2 == 0 &&
        (operation == 2 || (!cancelled && expires && esp_timer_get_time() < expires));
    if (valid && operation != 1) valid = state == WatchVoiceState::Transferring && (operation == 2 || (operation == 3 && offset == samples * 2));
    else if (valid) {
        auto ready = WatchVoiceState::Ready;
        if (offset == 0 && state.compare_exchange_strong(ready,WatchVoiceState::Transferring)) {
            // Phone deadline is 175s; keep five seconds for cleanup/cancellation.
            expires = esp_timer_get_time()+180'000'000;
            ESP_LOGI("watch_voice", "Secure transfer claimed: bytes=%u", unsigned(samples * 2));
            ESP_LOGI("watch_voice","VoiceLatency trace=%08lx stage=claimed mono_us=%lld",static_cast<unsigned long>(session),esp_timer_get_time());
        }
        valid = state == WatchVoiceState::Transferring && offset < samples*sizeof(int16_t);
        if (valid) { selected_offset = offset; stream_window.clear(); } // Explicit pull fallback stops push.
    }
    if (valid && operation == 3) {
        stream_window.clear();
        ESP_LOGI("watch_voice", "Phone acknowledged complete audio: bytes=%u; processing started", unsigned(offset));
        request_id = session; publish(WatchVoiceState::Processing);
    }
    xSemaphoreGive(audio_lock);
    if (valid && operation == 2) watch_voice_cancel();
    if (valid && operation == 3) xTaskNotify(task,kProcess,eSetBits);
    return valid;
}
size_t watch_voice_chunk(uint8_t *bytes, size_t capacity)
{
    if (!audio_lock || capacity < 11) return 0;
    xSemaphoreTake(audio_lock,portMAX_DELAY);
    size_t length = 0;
    if (state == WatchVoiceState::Transferring && pcm && !cancelled && expires &&
        esp_timer_get_time() < expires && selected_offset < samples*sizeof(int16_t)) {
        size_t count = (capacity-9) & ~size_t(1);
        const size_t remaining = samples*sizeof(int16_t)-selected_offset;
        if (count > remaining) count = remaining;
        bytes[0] = 1; put32(bytes+1,session_id); put32(bytes+5,selected_offset);
        memcpy(bytes+9,reinterpret_cast<uint8_t *>(pcm)+selected_offset,count); length = 9+count;
    }
    xSemaphoreGive(audio_lock); return length;
}
bool watch_voice_grant(uint32_t session, uint32_t epoch, uint32_t offset, uint8_t credits)
{
    if (!audio_lock) return false;
    xSemaphoreTake(audio_lock,portMAX_DELAY);
    bool valid = pcm && !cancelled && session == session_id &&
        (state == WatchVoiceState::Ready || state == WatchVoiceState::Transferring) &&
        expires && esp_timer_get_time() < expires;
    if (valid && state == WatchVoiceState::Ready && offset != 0) valid = false;
    if (valid) valid = stream_window.grant(session,epoch,offset,credits,samples*2);
    if (valid && state == WatchVoiceState::Ready) {
        expires = esp_timer_get_time()+180'000'000;
        publish(WatchVoiceState::Transferring);
        ESP_LOGI("watch_voice","VoiceLatency trace=%08lx stage=notify_claimed mono_us=%lld",static_cast<unsigned long>(session),esp_timer_get_time());
    }
    xSemaphoreGive(audio_lock); return valid;
}
size_t watch_voice_stream_chunk(uint8_t *bytes, size_t capacity)
{
    if (!audio_lock) return 0;
    xSemaphoreTake(audio_lock,portMAX_DELAY);
    const auto count = state == WatchVoiceState::Transferring && pcm && !cancelled &&
        expires && esp_timer_get_time() < expires ? stream_window.payload(capacity,samples*2) : 0;
    if (count) {
        bytes[0] = 2; put32(bytes+1,stream_window.session); put32(bytes+5,stream_window.epoch);
        put32(bytes+9,stream_window.offset); bytes[13] = stream_window.sequence;
        bytes[14] = stream_window.last(count,samples*2) ? 1 : 0;
        memcpy(bytes+15,reinterpret_cast<const uint8_t *>(pcm)+stream_window.offset,count);
    }
    xSemaphoreGive(audio_lock); return count ? count+15 : 0;
}
void watch_voice_stream_sent(uint32_t session, uint32_t epoch, size_t payload)
{
    if (!audio_lock) return;
    xSemaphoreTake(audio_lock,portMAX_DELAY);
    if (session == stream_window.session && epoch == stream_window.epoch)
        stream_window.sent(payload,samples*2);
    xSemaphoreGive(audio_lock);
}
void watch_voice_stop_stream()
{
    if (!audio_lock) return;
    xSemaphoreTake(audio_lock,portMAX_DELAY); stream_window.clear(); xSemaphoreGive(audio_lock);
}
void watch_voice_disconnected() { if (state == WatchVoiceState::Transferring || state == WatchVoiceState::Processing) watch_voice_cancel(); }
bool watch_voice_result(const uint8_t *bytes, size_t count)
{
    if (count < 11 || bytes[0] != 1 || !audio_lock) return false;
    uint32_t session = 0; for (unsigned i = 0; i < 4; ++i) session |= uint32_t(bytes[2+i]) << (8*i);
    xSemaphoreTake(audio_lock,portMAX_DELAY);
    bool valid = state == WatchVoiceState::Processing && request_id && session == request_id;
    if (valid) valid = result_text.accept(bytes[1],bytes[6] | uint16_t(bytes[7])<<8,bytes[8] | uint16_t(bytes[9])<<8,bytes+10,count-10);
    if (valid && result_text.complete()) {
        ESP_LOGI("watch_voice", "Phone result frame assembled: kind=%u bytes=%u", unsigned(bytes[1]), unsigned(result_text.total));
        ESP_LOGI("watch_voice","VoiceLatency trace=%08lx stage=result_assembled mono_us=%lld release_to_result_us=%lld",static_cast<unsigned long>(session),esp_timer_get_time(),released_us.load() ? esp_timer_get_time()-released_us.load() : -1LL);
        ++text_revision;
        if (bytes[1] != 1) { expires = 0; request_id = 0; publish(bytes[1] == 2 ? WatchVoiceState::Complete : WatchVoiceState::Error,bytes[1] == 3 ? "AI request failed. Hold to retry" : nullptr); }
    }
    xSemaphoreGive(audio_lock); return valid;
}
bool watch_voice_copy_text(uint32_t *revision, char *transcription, char *answer)
{
    if (!audio_lock) return false;
    xSemaphoreTake(audio_lock,portMAX_DELAY);
    const bool changed = *revision != text_revision;
    if (changed) {
        ESP_LOGI("watch_voice","VoiceLatency trace=%08lx stage=ui_text_copy mono_us=%lld revision=%lu",static_cast<unsigned long>(diagnostic_session),esp_timer_get_time(),static_cast<unsigned long>(text_revision));
        if (result_text.kind == 1 && !result_text.complete()) transcription[0] = 0;
        else memcpy(transcription,result_text.transcription,513);
        if (result_text.kind != 1 && !result_text.complete()) answer[0] = 0;
        else memcpy(answer,result_text.answer,3073);
        *revision = text_revision;
    }
    xSemaphoreGive(audio_lock); return changed;
}
