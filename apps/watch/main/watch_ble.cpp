#include "watch_ble.h"

#include <atomic>
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
enum class Value { Battery, Version, Steps, Wifi };
Value battery_id = Value::Battery, version_id = Value::Version, steps_id = Value::Steps, wifi_id = Value::Wifi;
struct Snapshot { int battery; uint32_t steps; uint8_t wifi; };
Snapshot values = {-1, 0, 0};
portMUX_TYPE values_lock = portMUX_INITIALIZER_UNLOCKED;
std::atomic<watch_ble_state_t> state{watch_ble_state_t::Disabled};
std::atomic<bool> ready{false};
std::atomic<bool> enabled{true};
bool disconnect_requested = false;
uint8_t address_type;
uint16_t battery_handle, steps_handle, wifi_handle;
uint16_t connection = BLE_HS_CONN_HANDLE_NONE;
bool battery_subscribed = false, steps_subscribed = false, wifi_subscribed = false;
Snapshot last_notified = {-1, 0, 0};
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

int read_value(uint16_t, uint16_t, ble_gatt_access_ctxt *context, void *arg)
{
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
        default: return BLE_ATT_ERR_UNLIKELY;
    }
    uint8_t bytes[4];
    for (unsigned i = 0; i < length; ++i) bytes[i] = static_cast<uint8_t>(value >> (8 * i));
    return os_mbuf_append(context->om, bytes, length) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

ble_gatt_chr_def battery_characteristics[2] = {};
ble_gatt_chr_def watch_characteristics[4] = {};
ble_gatt_svc_def services[3] = {};

void define_characteristic(ble_gatt_chr_def &definition, const ble_uuid_t *uuid, Value *id,
                           uint16_t *handle, bool notify)
{
    definition.uuid = uuid;
    definition.access_cb = read_value;
    definition.arg = id;
    definition.flags = BLE_GATT_CHR_F_READ | (notify ? BLE_GATT_CHR_F_NOTIFY : 0);
    definition.val_handle = handle;
}

void advertise(ble_npl_event *);

void process_updates(ble_npl_event *)
{
    if (!ready) return;
    if (!enabled.load()) {
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
    const auto current = snapshot();
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
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                connection = event->connect.conn_handle;
                if (!enabled.load()) { process_updates(nullptr); break; }
                state = watch_ble_state_t::Connected;
                last_notified = snapshot();
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
            connection = BLE_HS_CONN_HANDLE_NONE;
            disconnect_requested = false;
            battery_subscribed = steps_subscribed = wifi_subscribed = false;
            ble_npl_callout_stop(&step_callout);
            ESP_LOGI(kTag, "DISCONNECTED reason=%d", event->disconnect.reason);
            advertise(nullptr);
            break;
        case BLE_GAP_EVENT_ADV_COMPLETE: advertise(nullptr); break;
        case BLE_GAP_EVENT_SUBSCRIBE:
            if (event->subscribe.attr_handle == battery_handle) battery_subscribed = event->subscribe.cur_notify;
            if (event->subscribe.attr_handle == steps_handle) steps_subscribed = event->subscribe.cur_notify;
            if (event->subscribe.attr_handle == wifi_handle) wifi_subscribed = event->subscribe.cur_notify;
            if (event->subscribe.cur_notify) {
                const auto current = snapshot();
                if (event->subscribe.attr_handle != battery_handle || current.battery >= 0) {
                    ble_gatts_chr_updated(event->subscribe.attr_handle);
                }
                if (event->subscribe.attr_handle == battery_handle) last_notified.battery = current.battery;
                if (event->subscribe.attr_handle == wifi_handle) last_notified.wifi = current.wifi;
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
    advertise(nullptr);
}

void host_reset(int reason)
{
    ready = false;
    state = watch_ble_state_t::Disabled;
    connection = BLE_HS_CONN_HANDLE_NONE;
    disconnect_requested = false;
    battery_subscribed = steps_subscribed = wifi_subscribed = false;
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
} // namespace

esp_err_t watch_ble_init(void)
{
    esp_err_t error = nimble_port_init();
    if (error != ESP_OK) { ESP_LOGE(kTag, "NimBLE init: %s", esp_err_to_name(error)); return error; }
    ble_hs_cfg.sync_cb = host_sync;
    ble_hs_cfg.reset_cb = host_reset;
    // Explicit foundation policy: no automatic pairing/bonding, no MITM, no credentials.
    // Secure Connections supported for a central-requested Just Works pairing, not required for reads.
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding = 0;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_svc_gap_init();
    ble_svc_gatt_init();
    define_characteristic(battery_characteristics[0], &battery_uuid.u, &battery_id, &battery_handle, true);
    define_characteristic(watch_characteristics[0], &version_uuid.u, &version_id, nullptr, false);
    define_characteristic(watch_characteristics[1], &steps_uuid.u, &steps_id, &steps_handle, true);
    define_characteristic(watch_characteristics[2], &wifi_uuid.u, &wifi_id, &wifi_handle, true);
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
    nimble_port_freertos_init(host_task);
    ESP_LOGI(kTag, "NimBLE initialized: protocol=%u, unencrypted read/notify, no bonding", kProtocolVersion);
    ESP_LOGI(kTag, "DMA heap after BLE init: free=%u largest=%u",
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_DMA)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_DMA)));
    return ESP_OK;
}

watch_ble_state_t watch_ble_get_state(void) { return state.load(); }
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
void watch_ble_update_wifi(uint8_t wifi)
{
    if (wifi > 3) return;
    portENTER_CRITICAL(&values_lock);
    const bool changed = values.wifi != wifi;
    values.wifi = wifi;
    portEXIT_CRITICAL(&values_lock);
    if (changed) queue_update();
}
