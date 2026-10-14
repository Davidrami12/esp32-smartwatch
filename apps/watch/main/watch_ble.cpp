#include "watch_ble.h"
#include "watch_notifications.h"
#include "watch_ble_security.h"
#include "watch_media.h"
#include "watch_activity.h"
#include "watch_voice.h"

#include <atomic>
#include <algorithm>
#include <string.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
extern "C" {
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
}

namespace {
constexpr char kTag[] = "watch_ble";
constexpr char kName[] = "ESP32 Smartwatch";
constexpr uint16_t kProtocolVersion = 1;
constexpr int64_t kStepNotifyUs = 5'000'000; // At most once/5s for meaningful changes.
constexpr int64_t kStepTrailingUs = 15'000'000; // Flush a final 1–4 step change without spamming.
constexpr uint32_t kStepNotifyDelta = 5; // Five steps are meaningful; rollover also qualifies.
const ble_uuid16_t battery_service = BLE_UUID16_INIT(0x180F);
const ble_uuid16_t battery_uuid = BLE_UUID16_INIT(0x2A19);
// UUIDs are stored least-significant byte first by NimBLE. Canonical strings are in docs/ble-protocol.md.
const ble_uuid128_t watch_service = BLE_UUID128_INIT(
    0x00,0x10,0x2f,0x3e,0x4d,0x6c,0x2a,0x9f,0x6d,0x4b,0x7a,0x8e,0x01,0x00,0x57,0x7e);
const ble_uuid128_t version_uuid = BLE_UUID128_INIT(
    0x00,0x10,0x2f,0x3e,0x4d,0x6c,0x2a,0x9f,0x6d,0x4b,0x7a,0x8e,0x02,0x00,0x57,0x7e);
const ble_uuid128_t steps_uuid = BLE_UUID128_INIT(
    0x00,0x10,0x2f,0x3e,0x4d,0x6c,0x2a,0x9f,0x6d,0x4b,0x7a,0x8e,0x03,0x00,0x57,0x7e);
const ble_uuid128_t wifi_uuid = BLE_UUID128_INIT(
    0x00,0x10,0x2f,0x3e,0x4d,0x6c,0x2a,0x9f,0x6d,0x4b,0x7a,0x8e,0x04,0x00,0x57,0x7e);
const ble_uuid128_t brightness_uuid = BLE_UUID128_INIT(
    0x00,0x10,0x2f,0x3e,0x4d,0x6c,0x2a,0x9f,0x6d,0x4b,0x7a,0x8e,0x05,0x00,0x57,0x7e);
const ble_uuid128_t timeout_uuid = BLE_UUID128_INIT(
    0x00,0x10,0x2f,0x3e,0x4d,0x6c,0x2a,0x9f,0x6d,0x4b,0x7a,0x8e,0x06,0x00,0x57,0x7e);
const ble_uuid128_t wifi_enabled_uuid = BLE_UUID128_INIT(
    0x00,0x10,0x2f,0x3e,0x4d,0x6c,0x2a,0x9f,0x6d,0x4b,0x7a,0x8e,0x07,0x00,0x57,0x7e);
const ble_uuid128_t notification_uuid = BLE_UUID128_INIT(
    0x00,0x10,0x2f,0x3e,0x4d,0x6c,0x2a,0x9f,0x6d,0x4b,0x7a,0x8e,0x08,0x00,0x57,0x7e);
const ble_uuid128_t security_uuid = BLE_UUID128_INIT(
    0x00,0x10,0x2f,0x3e,0x4d,0x6c,0x2a,0x9f,0x6d,0x4b,0x7a,0x8e,0x09,0x00,0x57,0x7e);
const ble_uuid128_t media_uuid = BLE_UUID128_INIT(
    0x00,0x10,0x2f,0x3e,0x4d,0x6c,0x2a,0x9f,0x6d,0x4b,0x7a,0x8e,0x0a,0x00,0x57,0x7e);
const ble_uuid128_t media_command_uuid = BLE_UUID128_INIT(
    0x00,0x10,0x2f,0x3e,0x4d,0x6c,0x2a,0x9f,0x6d,0x4b,0x7a,0x8e,0x0b,0x00,0x57,0x7e);
const ble_uuid128_t media_capabilities_uuid = BLE_UUID128_INIT(
    0x00,0x10,0x2f,0x3e,0x4d,0x6c,0x2a,0x9f,0x6d,0x4b,0x7a,0x8e,0x0c,0x00,0x57,0x7e);
const ble_uuid128_t activity_uuid = BLE_UUID128_INIT(
    0x00,0x10,0x2f,0x3e,0x4d,0x6c,0x2a,0x9f,0x6d,0x4b,0x7a,0x8e,0x0d,0x00,0x57,0x7e);
const ble_uuid128_t step_goal_uuid = BLE_UUID128_INIT(
    0x00,0x10,0x2f,0x3e,0x4d,0x6c,0x2a,0x9f,0x6d,0x4b,0x7a,0x8e,0x0e,0x00,0x57,0x7e);
const ble_uuid128_t voice_meta_uuid = BLE_UUID128_INIT(0x00,0x10,0x2f,0x3e,0x4d,0x6c,0x2a,0x9f,0x6d,0x4b,0x7a,0x8e,0x0f,0x00,0x57,0x7e);
const ble_uuid128_t voice_data_uuid = BLE_UUID128_INIT(0x00,0x10,0x2f,0x3e,0x4d,0x6c,0x2a,0x9f,0x6d,0x4b,0x7a,0x8e,0x10,0x00,0x57,0x7e);
const ble_uuid128_t voice_control_uuid = BLE_UUID128_INIT(0x00,0x10,0x2f,0x3e,0x4d,0x6c,0x2a,0x9f,0x6d,0x4b,0x7a,0x8e,0x11,0x00,0x57,0x7e);
const ble_uuid128_t voice_result_uuid = BLE_UUID128_INIT(0x00,0x10,0x2f,0x3e,0x4d,0x6c,0x2a,0x9f,0x6d,0x4b,0x7a,0x8e,0x12,0x00,0x57,0x7e);
const ble_uuid128_t voice_caps_uuid = BLE_UUID128_INIT(0x00,0x10,0x2f,0x3e,0x4d,0x6c,0x2a,0x9f,0x6d,0x4b,0x7a,0x8e,0x13,0x00,0x57,0x7e);
const ble_uuid128_t voice_stream_uuid = BLE_UUID128_INIT(0x00,0x10,0x2f,0x3e,0x4d,0x6c,0x2a,0x9f,0x6d,0x4b,0x7a,0x8e,0x14,0x00,0x57,0x7e);
enum class Value { Battery, Version, Steps, Wifi, Brightness, Timeout, WifiEnabled, Notification, Security, Media, MediaCommand, MediaCapabilities, Activity, StepGoal, VoiceMeta, VoiceData, VoiceControl, VoiceResult, VoiceCaps, VoiceStream };
Value voice_caps_id = Value::VoiceCaps, voice_stream_id = Value::VoiceStream;
uint16_t voice_stream_handle;
bool voice_stream_subscribed = false;
unsigned voice_send_failures = 0;
ble_npl_callout voice_callout;
Value voice_result_id = Value::VoiceResult;
Value voice_meta_id = Value::VoiceMeta, voice_data_id = Value::VoiceData, voice_control_id = Value::VoiceControl;
Value step_goal_id = Value::StepGoal;
std::atomic<uint32_t> step_goal{0};
Value activity_id = Value::Activity;
uint8_t activity_bytes[170];
size_t activity_size = 0;
int64_t activity_cached_at = 0;
Value battery_id = Value::Battery, version_id = Value::Version, steps_id = Value::Steps, wifi_id = Value::Wifi;
Value brightness_id = Value::Brightness, timeout_id = Value::Timeout, wifi_enabled_id = Value::WifiEnabled;
Value notification_id = Value::Notification;
Value security_id = Value::Security;
Value media_id = Value::Media, media_command_id = Value::MediaCommand;
Value media_capabilities_id = Value::MediaCapabilities;
uint16_t media_command_handle;
bool media_command_subscribed = false;
int64_t last_media_command = 0;
struct Snapshot { int battery; uint32_t steps; uint8_t wifi; uint8_t brightness; uint32_t timeout; };
Snapshot values = {-1, 0, 0, 60, 10000};
watch_ble_control_cb_t control_callback = nullptr;
portMUX_TYPE values_lock = portMUX_INITIALIZER_UNLOCKED;
std::atomic<watch_ble_state_t> state{watch_ble_state_t::Disabled};
std::atomic<bool> ready{false};
std::atomic<bool> enabled{true};
bool disconnect_requested = false;
uint8_t address_type;
uint16_t battery_handle, steps_handle, wifi_handle;
uint16_t brightness_handle, timeout_handle, wifi_enabled_handle;
uint16_t service_changed_handle;
bool service_changed_sent = false;
uint16_t connection = BLE_HS_CONN_HANDLE_NONE;
bool battery_subscribed = false, steps_subscribed = false, wifi_subscribed = false;
bool brightness_subscribed = false, timeout_subscribed = false, wifi_enabled_subscribed = false;
Snapshot last_notified = {-1, 0, 0, 60, 10000};
bool last_wifi_enabled = true;
int64_t last_step_notify = 0;
ble_npl_event update_event;
ble_npl_callout step_callout;
ble_npl_callout advertise_callout;

Snapshot snapshot()
{
    portENTER_CRITICAL(&values_lock);
    const Snapshot result = values;
    portEXIT_CRITICAL(&values_lock);
    return result;
}

void voice_pump(ble_npl_event *)
{
    // One notification per host event; at most four unacknowledged frames.
    if (!enabled || !voice_stream_subscribed || connection == BLE_HS_CONN_HANDLE_NONE ||
        !watch_ble_security_ready(connection)) { watch_voice_stop_stream(); return; }
    uint8_t bytes[512];
    const size_t count = watch_voice_stream_chunk(bytes,std::min<size_t>(sizeof(bytes),ble_att_mtu(connection)-3));
    if (!count) return;
    auto *payload = ble_hs_mbuf_from_flat(bytes,count);
    // NimBLE consumes the mbuf on both success and failure; never double-free.
    const int rc = payload ? ble_gatts_notify_custom(connection,voice_stream_handle,payload) : BLE_HS_ENOMEM;
    if (!rc) {
        uint32_t session = 0, epoch = 0;
        for (unsigned i = 0; i < 4; ++i) { session |= uint32_t(bytes[1+i])<<(i*8); epoch |= uint32_t(bytes[5+i])<<(i*8); }
        watch_voice_stream_sent(session,epoch,count-15); voice_send_failures = 0;
    }
    volatile uint8_t *scratch = bytes; for (size_t i = 0; i < sizeof(bytes); ++i) scratch[i] = 0;
    if (rc && ++voice_send_failures >= 20) {
        watch_voice_stop_stream(); ESP_LOGW(kTag,"Voice notification enqueue stalled; waiting for bounded peer retry"); return;
    }
    ble_npl_callout_reset(&voice_callout,ble_npl_time_ms_to_ticks32(rc ? 25 : 10));
}

int read_value(uint16_t conn_handle, uint16_t, ble_gatt_access_ctxt *context, void *arg)
{
    // NimBLE reads locally (NONE) to build notifications; only a secure peer can trigger them.
    if (!watch_ble_security_ready(conn_handle == BLE_HS_CONN_HANDLE_NONE ? connection : conn_handle)) return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
    const auto id = *static_cast<Value *>(arg);
    if (id == Value::VoiceResult) {
        if (context->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_READ_NOT_PERMITTED;
        uint8_t bytes[244]; const size_t length = OS_MBUF_PKTLEN(context->om);
        if (length < 11 || length > sizeof(bytes)) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        if (os_mbuf_copydata(context->om,0,length,bytes)) return BLE_ATT_ERR_UNLIKELY;
        return watch_voice_result(bytes,length) ? 0 : BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }
    if (id == Value::VoiceCaps || id == Value::VoiceStream) {
        if (context->op != BLE_GATT_ACCESS_OP_READ_CHR) return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
        const uint8_t caps[] = {2,1,4,15,0,2}; // v2, PCM, credits, header, max frame LE512.
        const uint8_t noop = 2;
        return os_mbuf_append(context->om,id == Value::VoiceCaps ? caps : &noop,
            id == Value::VoiceCaps ? sizeof(caps) : 1) ? BLE_ATT_ERR_INSUFFICIENT_RES : 0;
    }
    if (id == Value::VoiceMeta || id == Value::VoiceData) {
        if (context->op != BLE_GATT_ACCESS_OP_READ_CHR) return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
        // Stay below MTU-1 so Android needs no Read Blob continuation.
        // Larger secure reads amortize the serialized select-write/read round trips.
        uint8_t bytes[512];
        const size_t capacity = std::min<size_t>(sizeof(bytes),ble_att_mtu(conn_handle)-3);
        const size_t length = id == Value::VoiceMeta ? watch_voice_metadata(bytes) : watch_voice_chunk(bytes,capacity);
        return length && !os_mbuf_append(context->om,bytes,length) ? 0 : BLE_ATT_ERR_UNLIKELY;
    }
    if (id == Value::VoiceControl) {
        if (context->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_READ_NOT_PERMITTED;
        if (OS_MBUF_PKTLEN(context->om) == 15) {
            uint8_t grant[15];
            if (os_mbuf_copydata(context->om,0,sizeof(grant),grant) || grant[0] != 2 || grant[1] != 4 ||
                !voice_stream_subscribed) return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
            uint32_t session = 0, epoch = 0, offset = 0;
            for (unsigned i = 0; i < 4; ++i) { session |= uint32_t(grant[2+i])<<(8*i); epoch |= uint32_t(grant[6+i])<<(8*i); offset |= uint32_t(grant[10+i])<<(8*i); }
            if (!watch_voice_grant(session,epoch,offset,grant[14])) return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
            voice_send_failures = 0;
            if (!offset) {
                ble_gap_upd_params fast = {}; fast.itvl_min = 12; fast.itvl_max = 24;
                fast.latency = 0; fast.supervision_timeout = 600;
                const int rc = ble_gap_update_params(conn_handle,&fast);
                ESP_LOGI(kTag,"Voice notification fast connection requested: rc=%d",rc);
            }
            ble_npl_callout_reset(&voice_callout,ble_npl_time_ms_to_ticks32(10));
            return 0;
        }
        if (OS_MBUF_PKTLEN(context->om) != 10) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        uint8_t bytes[10];
        if (os_mbuf_copydata(context->om,0,10,bytes) || bytes[0] != 1 || bytes[1] < 1 || bytes[1] > 3) return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
        uint32_t session = 0, offset = 0;
        for (unsigned i = 0; i < 4; ++i) { session |= uint32_t(bytes[2+i]) << (8*i); offset |= uint32_t(bytes[6+i]) << (8*i); }
        if (!watch_voice_select(session,offset,bytes[1])) return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
        if (bytes[1] == 1 && offset == 0) {
            ble_gap_upd_params parameters = {};
            parameters.itvl_min = 12; parameters.itvl_max = 24;
            parameters.latency = 0; parameters.supervision_timeout = 600;
            const int result = ble_gap_update_params(conn_handle, &parameters);
            ESP_LOGI("watch_ble", "Voice fast connection requested: result=%d", result);
        }
        return 0;
    }
    if (id == Value::StepGoal) {
        if (context->op != BLE_GATT_ACCESS_OP_READ_CHR) return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
        const uint32_t goal = step_goal.load();
        if (!goal) return BLE_ATT_ERR_UNLIKELY;
        uint8_t bytes[4];
        for (unsigned i = 0; i < 4; ++i) bytes[i] = static_cast<uint8_t>(goal >> (8*i));
        return os_mbuf_append(context->om, bytes, sizeof(bytes)) ? BLE_ATT_ERR_INSUFFICIENT_RES : 0;
    }
    if (id == Value::Activity) {
        if (context->op != BLE_GATT_ACCESS_OP_READ_CHR) return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
        const int64_t now = esp_timer_get_time();
        // Immutable across Read/Read Blob fragments, even at minimum ATT MTU.
        if (!activity_size || now-activity_cached_at >= 10'000'000) {
            activity_size = watch_activity_export(activity_bytes); activity_cached_at = now;
        }
        return os_mbuf_append(context->om, activity_bytes, activity_size) ? BLE_ATT_ERR_INSUFFICIENT_RES : 0;
    }
    if (id == Value::MediaCapabilities) {
        if (context->op != BLE_GATT_ACCESS_OP_READ_CHR) return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
        const uint8_t capability = 1; // Snapshot schema 2 + relative-volume commands.
        return os_mbuf_append(context->om, &capability, 1) ? BLE_ATT_ERR_INSUFFICIENT_RES : 0;
    }
    if (id == Value::MediaCommand) {
        if (context->op != BLE_GATT_ACCESS_OP_READ_CHR) return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
        const uint8_t noop[8] = {1, 0}; // Reading/subscribing never replays a command.
        return os_mbuf_append(context->om, noop, sizeof(noop)) ? BLE_ATT_ERR_INSUFFICIENT_RES : 0;
    }
    if (id == Value::Media) {
        if (context->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_READ_NOT_PERMITTED;
        const size_t length = OS_MBUF_PKTLEN(context->om);
        if (length < 9 || length > 20) { watch_media_reset(); return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN; }
        uint8_t bytes[20];
        if (os_mbuf_copydata(context->om, 0, length, bytes)) return BLE_ATT_ERR_UNLIKELY;
        return watch_media_receive(bytes, length) < 0 ? BLE_ATT_ERR_VALUE_NOT_ALLOWED : 0;
    }
    if (id == Value::Security) {
        if (context->op != BLE_GATT_ACCESS_OP_READ_CHR) return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
        const uint8_t bytes[] = {1, 0x0f, 16}; // Schema, encrypted/authenticated/SC/trusted, key bytes.
        return os_mbuf_append(context->om, bytes, sizeof(bytes)) ? BLE_ATT_ERR_INSUFFICIENT_RES : 0;
    }
    if (id == Value::Notification) {
        if (context->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_READ_NOT_PERMITTED;
        const size_t length = OS_MBUF_PKTLEN(context->om);
        if (length < 9 || length > 20) {
            watch_notifications_reset_transport();
            ESP_LOGW(kTag, "Rejected notification frame length=%u", static_cast<unsigned>(length));
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        uint8_t frame[20];
        if (os_mbuf_copydata(context->om, 0, length, frame)) return BLE_ATT_ERR_UNLIKELY;
        return watch_notifications_receive(frame, length) < 0 ? BLE_ATT_ERR_VALUE_NOT_ALLOWED : 0;
    }
    if (context->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        watch_ble_control_t control;
        if (id == Value::Brightness) control = watch_ble_control_t::Brightness;
        else if (id == Value::Timeout) control = watch_ble_control_t::ScreenTimeout;
        else if (id == Value::WifiEnabled) control = watch_ble_control_t::WifiEnabled;
        else return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
        const size_t length = OS_MBUF_PKTLEN(context->om);
        if (length != watch_ble_control_length(control)) {
            ESP_LOGW(kTag, "Rejected control=%u length=%u", static_cast<unsigned>(control), static_cast<unsigned>(length));
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        uint8_t bytes[4]; uint32_t value;
        if (os_mbuf_copydata(context->om, 0, length, bytes) || !watch_ble_decode_control(control, bytes, length, value)) {
            ESP_LOGW(kTag, "Rejected control=%u value", static_cast<unsigned>(control));
            return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
        }
        const esp_err_t result = control_callback ? control_callback(control, value) : ESP_ERR_INVALID_STATE;
        if (result != ESP_OK) {
            ESP_LOGW(kTag, "Control=%u failed: %s", static_cast<unsigned>(control), esp_err_to_name(result));
            return BLE_ATT_ERR_UNLIKELY;
        }
        ESP_LOGI(kTag, "Applied control=%u", static_cast<unsigned>(control));
        return 0;
    }
    if (context->op != BLE_GATT_ACCESS_OP_READ_CHR) return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    const auto current = snapshot();
    uint32_t value;
    unsigned length;
    switch (*static_cast<Value *>(arg)) {
        case Value::Battery:
            if (current.battery < 0) return BLE_ATT_ERR_UNLIKELY;
            value = current.battery; length = 1; break;
        case Value::Version: value = kProtocolVersion; length = 2; break;
        case Value::Steps: value = current.steps; length = 4; break;
        case Value::Wifi: value = current.wifi; length = 1; break;
        case Value::Brightness: value = current.brightness; length = 1; break;
        case Value::Timeout: value = current.timeout; length = 4; break;
        case Value::WifiEnabled: value = current.wifi != 3; length = 1; break;
        default: return BLE_ATT_ERR_UNLIKELY;
    }
    uint8_t bytes[4];
    for (unsigned i = 0; i < length; ++i) bytes[i] = static_cast<uint8_t>(value >> (8 * i));
    return os_mbuf_append(context->om, bytes, length) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

ble_gatt_chr_def battery_characteristics[2] = {};
ble_gatt_chr_def watch_characteristics[20] = {};
ble_gatt_svc_def services[3] = {};

void define_characteristic(ble_gatt_chr_def &definition, const ble_uuid_t *uuid, Value *id,
                           uint16_t *handle, bool notify, bool writable = false)
{
    definition.uuid = uuid;
    definition.access_cb = read_value;
    definition.arg = id;
    definition.flags = BLE_GATT_CHR_F_READ | (notify ? BLE_GATT_CHR_F_NOTIFY : 0) | (writable ? BLE_GATT_CHR_F_WRITE : 0);
    definition.flags |= BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_READ_AUTHEN;
    if (writable) definition.flags |= BLE_GATT_CHR_F_WRITE_ENC | BLE_GATT_CHR_F_WRITE_AUTHEN;
    definition.min_key_size = 16;
    definition.val_handle = handle;
}

void advertise(ble_npl_event *);

void process_updates(ble_npl_event *)
{
    if (!ready) return;
    if (!enabled.load()) {
        ble_npl_callout_stop(&voice_callout); watch_voice_stop_stream();
        ble_npl_callout_stop(&advertise_callout);
        ble_npl_callout_stop(&step_callout);
        if (ble_gap_adv_active()) ble_gap_adv_stop();
        if (connection != BLE_HS_CONN_HANDLE_NONE && !disconnect_requested) {
            const int result = ble_gap_terminate(connection, BLE_ERR_REM_USER_CONN_TERM);
            disconnect_requested = result == 0;
            if (result) {
                ESP_LOGW(kTag, "Disable disconnect failed rc=%d; retry in 2s", result);
                ble_npl_callout_reset(&step_callout, ble_npl_time_ms_to_ticks32(2000));
            }
        }
        if (state.exchange(watch_ble_state_t::Disabled) != watch_ble_state_t::Disabled)
            ESP_LOGI(kTag, "DISABLED by user");
        return;
    }
    if (connection == BLE_HS_CONN_HANDLE_NONE) { advertise(nullptr); return; }
    if (!watch_ble_security_ready(connection)) return;
    uint8_t media_command[8];
    if (watch_media_take_command(media_command)) {
        const int64_t now = esp_timer_get_time();
        if (media_command_subscribed && now-last_media_command >= 350000) {
            auto *payload = ble_hs_mbuf_from_flat(media_command, sizeof(media_command));
            const int rc = payload ? ble_gatts_notify_custom(connection, media_command_handle, payload) : BLE_HS_ENOMEM;
            last_media_command = now;
            ESP_LOGI(kTag, "Media command=%u sent rc=%d", media_command[1], rc);
        }
    }
    const auto current = snapshot();
    if (brightness_subscribed && current.brightness != last_notified.brightness) {
        ble_gatts_chr_updated(brightness_handle); last_notified.brightness = current.brightness;
    }
    if (timeout_subscribed && current.timeout != last_notified.timeout) {
        ble_gatts_chr_updated(timeout_handle); last_notified.timeout = current.timeout;
    }
    if (wifi_enabled_subscribed && (current.wifi != 3) != last_wifi_enabled) {
        ble_gatts_chr_updated(wifi_enabled_handle); last_wifi_enabled = current.wifi != 3;
    }
    if (battery_subscribed && current.battery >= 0 && current.battery != last_notified.battery) {
        ble_gatts_chr_updated(battery_handle);
        last_notified.battery = current.battery;
    }
    if (wifi_subscribed && current.wifi != last_notified.wifi) {
        ble_gatts_chr_updated(wifi_handle);
        last_notified.wifi = current.wifi;
    }
    if (steps_subscribed && current.steps != last_notified.steps) {
        const int64_t now = esp_timer_get_time();
        const bool meaningful = current.steps < last_notified.steps ||
            current.steps - last_notified.steps >= kStepNotifyDelta;
        const int64_t delay_us = meaningful ? kStepNotifyUs : kStepTrailingUs;
        if (now - last_step_notify >= delay_us) {
            ble_gatts_chr_updated(steps_handle);
            last_notified.steps = current.steps;
            last_step_notify = now;
        } else {
            const uint32_t remaining_ms = static_cast<uint32_t>((delay_us - (now - last_step_notify) + 999) / 1000);
            ble_npl_callout_reset(&step_callout, ble_npl_time_ms_to_ticks32(remaining_ms));
        }
    }
}

void queue_update()
{
    if (ready.load()) ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &update_event);
}

int gap_event(ble_gap_event *event, void *);
void advertise(ble_npl_event *)
{
    if (!ready || connection != BLE_HS_CONN_HANDLE_NONE) return;
    if (!enabled.load()) { state = watch_ble_state_t::Disabled; return; }
    if (ble_gap_adv_active()) return;
    ble_hs_adv_fields fields = {};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = const_cast<ble_uuid128_t *>(&watch_service);
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    fields.uuids16 = const_cast<ble_uuid16_t *>(&battery_service);
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;
    int result = ble_gap_adv_set_fields(&fields);
    ble_hs_adv_fields scan = {};
    scan.name = reinterpret_cast<uint8_t *>(const_cast<char *>(kName));
    scan.name_len = strlen(kName);
    scan.name_is_complete = 1;
    if (!result) result = ble_gap_adv_rsp_set_fields(&scan);
    ble_gap_adv_params params = {};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    params.itvl_min = 800; // 500–750ms legacy advertising; reduced duty cycle.
    params.itvl_max = 1200;
    if (!result) result = ble_gap_adv_start(address_type, nullptr, BLE_HS_FOREVER, &params, gap_event, nullptr);
    if (result) {
        state = watch_ble_state_t::Disabled;
        ESP_LOGW(kTag, "Advertising failed rc=%d; retry in 2s", result);
        ble_npl_callout_reset(&advertise_callout, ble_npl_time_ms_to_ticks32(2000));
    } else {
        state = watch_ble_state_t::Advertising;
        ESP_LOGI(kTag, "ADVERTISING as %s", kName);
    }
}

int gap_event(ble_gap_event *event, void *)
{
    watch_ble_security_gap(event);
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                connection = event->connect.conn_handle;
                activity_size = 0;
                service_changed_sent = false;
                if (!enabled.load()) { process_updates(nullptr); break; }
                state = watch_ble_state_t::Advertising; // ON only after authenticated encryption.
                last_notified = snapshot();
                last_wifi_enabled = last_notified.wifi != 3;
                last_step_notify = esp_timer_get_time();
                ESP_LOGI(kTag, "CONNECTED handle=%u", connection);
                // Central may choose different parameters; request a modest interval and latency.
                ble_gap_upd_params params = {};
                params.itvl_min = 80; params.itvl_max = 160; // 100–200ms
                params.latency = 4; params.supervision_timeout = 600; // 6s
                const int result = ble_gap_update_params(connection, &params);
                if (result) ESP_LOGW(kTag, "Connection parameter request rc=%d", result);
            } else {
                ESP_LOGW(kTag, "Connect failed rc=%d", event->connect.status);
                advertise(nullptr);
            }
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            voice_stream_subscribed = false; ble_npl_callout_stop(&voice_callout);
            watch_voice_disconnected();
            watch_media_reset(); media_command_subscribed = false;
            watch_notifications_reset_transport();
            connection = BLE_HS_CONN_HANDLE_NONE;
            disconnect_requested = false;
            battery_subscribed = steps_subscribed = wifi_subscribed = false;
            brightness_subscribed = timeout_subscribed = wifi_enabled_subscribed = false;
            ble_npl_callout_stop(&step_callout);
            ESP_LOGI(kTag, "DISCONNECTED reason=%d", event->disconnect.reason);
            advertise(nullptr);
            break;
        case BLE_GAP_EVENT_ADV_COMPLETE: advertise(nullptr); break;
        case BLE_GAP_EVENT_CONN_UPDATE: {
            ble_gap_conn_desc descriptor = {};
            if (!event->conn_update.status && !ble_gap_conn_find(event->conn_update.conn_handle,&descriptor))
                ESP_LOGI(kTag,"Voice link parameters: interval_units=%u latency=%u supervision_units=%u",descriptor.conn_itvl,descriptor.conn_latency,descriptor.supervision_timeout);
            break;
        }
        case BLE_GAP_EVENT_REPEAT_PAIRING: return BLE_GAP_REPEAT_PAIRING_IGNORE;
        case BLE_GAP_EVENT_ENC_CHANGE:
            if (watch_ble_security_ready(connection)) { state = watch_ble_state_t::Connected; process_updates(nullptr); }
            break;
        case BLE_GAP_EVENT_SUBSCRIBE:
            if (!watch_ble_security_ready(connection)) break;
            if (event->subscribe.attr_handle == voice_stream_handle) {
                voice_stream_subscribed = event->subscribe.cur_notify;
                if (!voice_stream_subscribed) { ble_npl_callout_stop(&voice_callout); watch_voice_stop_stream(); }
                break; // No audio replay: the peer must explicitly grant credits.
            }
            if (event->subscribe.attr_handle == media_command_handle) {
                media_command_subscribed = event->subscribe.cur_notify;
                if (!media_command_subscribed) watch_media_reset();
                break;
            }
            if (event->subscribe.attr_handle == service_changed_handle && event->subscribe.cur_indicate && !service_changed_sent) {
                service_changed_sent = true;
                ble_svc_gatt_changed(1, UINT16_MAX);
                ESP_LOGI(kTag, "Standard GATT Service Changed queued");
            }
            if (event->subscribe.attr_handle == battery_handle) battery_subscribed = event->subscribe.cur_notify;
            if (event->subscribe.attr_handle == steps_handle) steps_subscribed = event->subscribe.cur_notify;
            if (event->subscribe.attr_handle == wifi_handle) wifi_subscribed = event->subscribe.cur_notify;
            if (event->subscribe.attr_handle == brightness_handle) brightness_subscribed = event->subscribe.cur_notify;
            if (event->subscribe.attr_handle == timeout_handle) timeout_subscribed = event->subscribe.cur_notify;
            if (event->subscribe.attr_handle == wifi_enabled_handle) wifi_enabled_subscribed = event->subscribe.cur_notify;
            if (event->subscribe.cur_notify) {
                const auto current = snapshot();
                if (event->subscribe.attr_handle != battery_handle || current.battery >= 0) {
                    ble_gatts_chr_updated(event->subscribe.attr_handle);
                }
                if (event->subscribe.attr_handle == battery_handle) last_notified.battery = current.battery;
                if (event->subscribe.attr_handle == wifi_handle) last_notified.wifi = current.wifi;
                if (event->subscribe.attr_handle == brightness_handle) last_notified.brightness = current.brightness;
                if (event->subscribe.attr_handle == timeout_handle) last_notified.timeout = current.timeout;
                if (event->subscribe.attr_handle == wifi_enabled_handle) last_wifi_enabled = current.wifi != 3;
                if (event->subscribe.attr_handle == steps_handle) {
                    last_notified.steps = current.steps;
                    last_step_notify = esp_timer_get_time();
                }
            }
            break;
        default: break;
    }
    return 0;
}

void host_sync()
{
    int result = ble_hs_util_ensure_addr(0);
    if (!result) result = ble_hs_id_infer_auto(0, &address_type);
    if (result) { ESP_LOGE(kTag, "Address initialization failed rc=%d", result); return; }
    ready = true;
    ESP_LOGI(kTag, "Controls registered: brightness=%u timeout=%u wifi_enabled=%u", brightness_handle, timeout_handle, wifi_enabled_handle);
    advertise(nullptr);
}

void host_reset(int reason)
{
    voice_stream_subscribed = false; ble_npl_callout_stop(&voice_callout);
    watch_voice_stop_stream(); watch_voice_disconnected();
    watch_media_reset(); media_command_subscribed = false;
    watch_ble_security_reset();
    watch_notifications_reset_transport();
    ready = false;
    state = watch_ble_state_t::Disabled;
    connection = BLE_HS_CONN_HANDLE_NONE;
    disconnect_requested = false;
    battery_subscribed = steps_subscribed = wifi_subscribed = false;
    brightness_subscribed = timeout_subscribed = wifi_enabled_subscribed = false;
    ble_npl_callout_stop(&step_callout);
    ble_npl_callout_stop(&advertise_callout);
    ESP_LOGW(kTag, "Host reset reason=%d", reason);
}

void host_task(void *)
{
    nimble_port_run();
    ready = false;
    state = watch_ble_state_t::Disabled;
    nimble_port_freertos_deinit();
}
void register_attribute(ble_gatt_register_ctxt *context, void *)
{
    if (context->op == BLE_GATT_REGISTER_OP_CHR && ble_uuid_u16(context->chr.chr_def->uuid) == 0x2a05)
        service_changed_handle = context->chr.val_handle;
}
} // namespace

esp_err_t watch_ble_init(void)
{
    esp_err_t error = nimble_port_init();
    if (error != ESP_OK) { ESP_LOGE(kTag, "NimBLE init: %s", esp_err_to_name(error)); return error; }
    ble_hs_cfg.sync_cb = host_sync;
    ble_hs_cfg.reset_cb = host_reset;
    ble_hs_cfg.gatts_register_cb = register_attribute;
    error = watch_ble_security_init();
    if (error != ESP_OK) { nimble_port_deinit(); return error; }
    ble_svc_gap_init();
    ble_svc_gatt_init();
    define_characteristic(battery_characteristics[0], &battery_uuid.u, &battery_id, &battery_handle, true);
    define_characteristic(watch_characteristics[0], &version_uuid.u, &version_id, nullptr, false);
    define_characteristic(watch_characteristics[1], &steps_uuid.u, &steps_id, &steps_handle, true);
    define_characteristic(watch_characteristics[2], &wifi_uuid.u, &wifi_id, &wifi_handle, true);
    define_characteristic(watch_characteristics[3], &brightness_uuid.u, &brightness_id, &brightness_handle, true, true);
    define_characteristic(watch_characteristics[4], &timeout_uuid.u, &timeout_id, &timeout_handle, true, true);
    define_characteristic(watch_characteristics[5], &wifi_enabled_uuid.u, &wifi_enabled_id, &wifi_enabled_handle, true, true);
    define_characteristic(watch_characteristics[6], &notification_uuid.u, &notification_id, nullptr, false, true);
    watch_characteristics[6].flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC | BLE_GATT_CHR_F_WRITE_AUTHEN;
    define_characteristic(watch_characteristics[7], &security_uuid.u, &security_id, nullptr, false);
    define_characteristic(watch_characteristics[8], &media_uuid.u, &media_id, nullptr, false, true);
    watch_characteristics[8].flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC | BLE_GATT_CHR_F_WRITE_AUTHEN;
    define_characteristic(watch_characteristics[9], &media_command_uuid.u, &media_command_id, &media_command_handle, true);
    define_characteristic(watch_characteristics[10], &media_capabilities_uuid.u, &media_capabilities_id, nullptr, false);
    define_characteristic(watch_characteristics[11], &activity_uuid.u, &activity_id, nullptr, false);
    define_characteristic(watch_characteristics[12], &step_goal_uuid.u, &step_goal_id, nullptr, false);
    define_characteristic(watch_characteristics[13], &voice_meta_uuid.u, &voice_meta_id, nullptr, false);
    define_characteristic(watch_characteristics[14], &voice_data_uuid.u, &voice_data_id, nullptr, false);
    define_characteristic(watch_characteristics[15], &voice_control_uuid.u, &voice_control_id, nullptr, false, true);
    watch_characteristics[15].flags &= ~BLE_GATT_CHR_F_READ;
    define_characteristic(watch_characteristics[16], &voice_result_uuid.u, &voice_result_id, nullptr, false, true);
    watch_characteristics[16].flags &= ~BLE_GATT_CHR_F_READ;
    define_characteristic(watch_characteristics[17], &voice_caps_uuid.u, &voice_caps_id, nullptr, false);
    define_characteristic(watch_characteristics[18], &voice_stream_uuid.u, &voice_stream_id, &voice_stream_handle, true);
    services[0].type = services[1].type = BLE_GATT_SVC_TYPE_PRIMARY;
    services[0].uuid = &battery_service.u;
    services[0].characteristics = battery_characteristics;
    services[1].uuid = &watch_service.u;
    services[1].characteristics = watch_characteristics;
    int result = ble_gatts_count_cfg(services);
    if (!result) result = ble_gatts_add_svcs(services);
    if (!result) result = ble_svc_gap_device_name_set(kName);
    if (result) {
        ESP_LOGE(kTag, "GATT setup failed rc=%d", result);
        nimble_port_deinit();
        return ESP_FAIL;
    }
    ble_npl_event_init(&update_event, process_updates, nullptr);
    ble_npl_callout_init(&step_callout, nimble_port_get_dflt_eventq(), process_updates, nullptr);
    ble_npl_callout_init(&advertise_callout, nimble_port_get_dflt_eventq(), advertise, nullptr);
    ble_npl_callout_init(&voice_callout, nimble_port_get_dflt_eventq(), voice_pump, nullptr);
    nimble_port_freertos_init(host_task);
    ESP_LOGI(kTag, "NimBLE initialized: protocol=%u, authenticated/encrypted GATT, persistent bonding", kProtocolVersion);
    ESP_LOGI(kTag, "DMA heap after BLE init: free=%u largest=%u",
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_DMA)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_DMA)));
    return ESP_OK;
}

watch_ble_state_t watch_ble_get_state(void) { return state.load(); }
bool watch_ble_media_command(uint8_t command, uint32_t session)
{
    if (state.load() != watch_ble_state_t::Connected || !watch_media_queue_command(command, session)) return false;
    queue_update(); return true;
}
void watch_ble_set_control_callback(watch_ble_control_cb_t callback) { control_callback = callback; }
void watch_ble_update_settings(uint8_t brightness, uint32_t timeout_ms)
{
    portENTER_CRITICAL(&values_lock);
    const bool changed = values.brightness != brightness || values.timeout != timeout_ms;
    values.brightness = brightness; values.timeout = timeout_ms;
    portEXIT_CRITICAL(&values_lock);
    if (changed) queue_update();
}
bool watch_ble_set_enabled(bool value)
{
    if (!ready.load()) return false;
    enabled = value;
    queue_update();
    return true;
}
void watch_ble_update_battery(int percentage)
{
    if (percentage > 100) percentage = 100;
    if (percentage < 0) percentage = -1;
    portENTER_CRITICAL(&values_lock);
    const bool changed = values.battery != percentage;
    values.battery = percentage;
    portEXIT_CRITICAL(&values_lock);
    if (changed) queue_update();
}
void watch_ble_update_steps(uint32_t steps)
{
    portENTER_CRITICAL(&values_lock);
    const bool changed = values.steps != steps;
    values.steps = steps;
    portEXIT_CRITICAL(&values_lock);
    if (changed) queue_update();
}
void watch_ble_update_step_goal(uint32_t goal)
{
    if (goal >= 1000 && goal <= 30000 && goal % 1000 == 0) step_goal = goal;
}
void watch_ble_update_wifi(uint8_t wifi)
{
    if (wifi > 3) return;
    portENTER_CRITICAL(&values_lock);
    const bool changed = values.wifi != wifi;
    values.wifi = wifi;
    portEXIT_CRITICAL(&values_lock);
    if (changed) queue_update();
}
