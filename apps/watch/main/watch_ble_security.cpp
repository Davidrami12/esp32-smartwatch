#include "watch_ble_security.h"
#include "watch_ble_security_policy.h"
#include <atomic>
#include <string.h>
#include "esp_log.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
extern "C" {
#include "nimble/nimble_port.h"
#include "host/ble_hs.h"
#include "host/ble_store.h"
#include "host/ble_sm.h"
void ble_store_config_init(void);
}

namespace {
constexpr char tag[] = "watch_security";
// Reserved is deliberately fail-closed after power loss or interrupted first pairing.
struct Trust { uint8_t version, phase, type, address[6]; };
Trust trust = {1, 0, 0, {}};
nvs_handle_t nvs;
uint16_t peer = BLE_HS_CONN_HANDLE_NONE;
bool accepted = false, storage_failed = false, forgetting = false;
std::atomic<bool> secure{false};
std::atomic<bool> initialized{false};
ble_store_write_fn *store_write;
ble_npl_event command_event;
ble_npl_callout deadline;
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
watch_pairing_status_t status = {watch_pairing_state_t::Unpaired, 0, 0, false};
enum class Command { None, Accept, Reject, Forget };
Command command = Command::None;
uint32_t command_token = 0;

void publish(watch_pairing_state_t value, uint32_t code = 0) {
    portENTER_CRITICAL(&lock);
    ++status.token; status.state = value; status.code = code; status.has_phone = trust.phase != 0;
    portEXIT_CRITICAL(&lock);
}
bool persist(const Trust &value) {
    if (nvs_set_blob(nvs, "phone", &value, sizeof(value)) != ESP_OK || nvs_commit(nvs) != ESP_OK) {
        storage_failed = true; ESP_LOGE(tag, "Trust persistence failed; access denied"); return false;
    }
    trust = value; return true;
}
bool matches(const ble_addr_t &address) {
    return trust.phase == 2 && trust.type == address.type && memcmp(trust.address, address.val, 6) == 0;
}
void terminate() {
    secure = false;
    if (peer != BLE_HS_CONN_HANDLE_NONE) ble_gap_terminate(peer, BLE_ERR_AUTH_FAIL);
}
int guarded_store(int type, const union ble_store_value *value) {
    if (type == BLE_STORE_OBJ_TYPE_OUR_SEC || type == BLE_STORE_OBJ_TYPE_PEER_SEC) {
        const auto &key = value->sec;
        if (forgetting || (!accepted && !matches(key.peer_addr)) || !key.authenticated || !key.sc || key.key_size != 16) {
            storage_failed = true; secure = false; return BLE_HS_EAUTHEN;
        }
    }
    const int rc = store_write(type, value);
    if (rc) { storage_failed = true; secure = false; ESP_LOGE(tag, "Bond/CCCD storage failed rc=%d", rc); }
    return rc;
}
int storage_full(ble_store_status_event *, void *) {
    // Never use ble_store_util_status_rr: it evicts the trusted companion.
    storage_failed = true; secure = false; return BLE_HS_ENOMEM;
}
void finish_forget() {
    // Clear NimBLE bonds/CCCDs first. Clear trust last so interruption remains locked.
    if (ble_store_clear() != 0 || !persist(Trust{1, 0, 0, {}})) {
        forgetting = false; publish(watch_pairing_state_t::Failed); return;
    }
    accepted = storage_failed = forgetting = false;
    publish(watch_pairing_state_t::Forgotten);
    ESP_LOGI(tag, "Trusted phone and BLE bonding records forgotten");
}
void timed_out(ble_npl_event *) {
    ESP_LOGW(tag, "Pairing/security timeout");
    publish(watch_pairing_state_t::Failed); terminate();
}
void process_command(ble_npl_event *) {
    Command next; uint32_t token;
    portENTER_CRITICAL(&lock); next = command; token = command_token; command = Command::None; portEXIT_CRITICAL(&lock);
    if (next == Command::Forget) {
        forgetting = true; secure = false; accepted = false;
        ble_npl_callout_stop(&deadline); publish(watch_pairing_state_t::Forgetting);
        if (peer == BLE_HS_CONN_HANDLE_NONE) finish_forget();
        else if (ble_gap_terminate(peer, BLE_ERR_REM_USER_CONN_TERM)) { forgetting = false; publish(watch_pairing_state_t::Failed); }
        return;
    }
    const auto view = watch_ble_security_status();
    if (view.token != token || view.state != watch_pairing_state_t::Comparing || peer == BLE_HS_CONN_HANDLE_NONE) return;
    ble_sm_io io = {}; io.action = BLE_SM_IOACT_NUMCMP;
    if (next == Command::Accept) {
        if (trust.phase != 0 || !persist(Trust{1, 1, 0, {}})) { publish(watch_pairing_state_t::Failed); terminate(); return; }
        accepted = true; io.numcmp_accept = 1; publish(watch_pairing_state_t::Securing);
    } else { io.numcmp_accept = 0; publish(watch_pairing_state_t::Rejected); }
    if (ble_sm_inject_io(peer, &io)) { publish(watch_pairing_state_t::Failed); terminate(); }
}
}

esp_err_t watch_ble_security_init() {
    esp_err_t rc = nvs_open("watch_ble_sec", NVS_READWRITE, &nvs);
    if (rc != ESP_OK) return rc;
    size_t size = sizeof(trust);
    rc = nvs_get_blob(nvs, "phone", &trust, &size);
    if (rc != ESP_ERR_NVS_NOT_FOUND && (rc != ESP_OK || size != sizeof(trust) || trust.version != 1 || trust.phase > 2)) {
        trust = {1, 1, 0, {}}; storage_failed = true; // No automatic reset/erase.
    }
    ble_store_config_init();
    store_write = ble_hs_cfg.store_write_cb;
    ble_hs_cfg.store_write_cb = guarded_store;
    ble_hs_cfg.store_status_cb = storage_full;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_DISPLAY_YESNO;
    ble_hs_cfg.sm_bonding = ble_hs_cfg.sm_mitm = ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_addr_t bonds[2]; int count = 0;
    if (ble_store_util_bonded_peers(bonds, &count, 2) || count > 1 || (count && !matches(bonds[0]))) {
        trust.phase = 1; storage_failed = true;
    }
    ble_npl_event_init(&command_event, process_command, nullptr);
    ble_npl_callout_init(&deadline, nimble_port_get_dflt_eventq(), timed_out, nullptr);
    publish(trust.phase == 2 ? watch_pairing_state_t::Trusted : trust.phase ? watch_pairing_state_t::Locked : watch_pairing_state_t::Unpaired);
    initialized = true;
    ESP_LOGI(tag, "SC-only numeric comparison, MITM, 128-bit keys, persistent single-phone bonding; slot=%u", trust.phase);
    return ESP_OK;
}

void watch_ble_security_gap(ble_gap_event *event) {
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT: {
        if (event->connect.status) break;
        peer = event->connect.conn_handle; secure = false; accepted = false;
        ble_gap_conn_desc desc;
        if (forgetting || storage_failed || ble_gap_conn_find(peer, &desc) || (trust.phase && !matches(desc.peer_id_addr))) {
            ESP_LOGW(tag, "Untrusted connection rejected (trusted slot occupied/locked)"); terminate(); break;
        }
        ble_npl_callout_reset(&deadline, ble_npl_time_ms_to_ticks32(60000));
        // Android initiates createBond / encryption via protected GATT reads. Do not race
        // the central with a simultaneous peripheral Security Request on bonded reconnect.
        break;
    }
    case BLE_GAP_EVENT_PASSKEY_ACTION:
        if (event->passkey.conn_handle != peer) break;
        if (event->passkey.params.action != BLE_SM_IOACT_NUMCMP || !watch_ble_can_pair(trust.phase, storage_failed, forgetting)) {
            ESP_LOGW(tag, "Non-numeric-comparison pairing rejected; no downgrade");
            publish(watch_pairing_state_t::Rejected); terminate(); break;
        }
        publish(watch_pairing_state_t::Comparing, event->passkey.params.numcmp);
        ESP_LOGI(tag, "Numeric comparison requires local user confirmation");
        break;
    case BLE_GAP_EVENT_REPEAT_PAIRING:
        ESP_LOGW(tag, "Stale/repeated bond rejected; forget on watch and Android before retry");
        publish(watch_pairing_state_t::Locked); terminate(); break;
    case BLE_GAP_EVENT_ENC_CHANGE: {
        if (event->enc_change.conn_handle != peer) break;
        ble_gap_conn_desc desc;
        ble_store_value_sec key = {}; ble_store_key_sec lookup = {};
        bool valid = !forgetting && !storage_failed && !event->enc_change.status && !ble_gap_conn_find(peer, &desc);
        if (valid) {
            lookup.peer_addr = desc.peer_id_addr;
            valid = ble_store_read_our_sec(&lookup, &key) == 0 && key.ltk_present && key.authenticated && key.key_size == 16 &&
                watch_ble_key_is_strong(desc.sec_state.encrypted, desc.sec_state.authenticated, desc.sec_state.bonded, desc.sec_state.key_size, key.sc);
        }
        if (valid && accepted && trust.phase == 1) {
            Trust next = {1, 2, desc.peer_id_addr.type, {}};
            memcpy(next.address, desc.peer_id_addr.val, 6); valid = persist(next);
        }
        valid = valid && matches(desc.peer_id_addr);
        ble_npl_callout_stop(&deadline);
        if (!valid) { ESP_LOGW(tag, "Secure link failed; access denied status=%d", event->enc_change.status); publish(watch_pairing_state_t::Failed); terminate(); }
        else { accepted = false; secure = true; publish(watch_pairing_state_t::Trusted); ESP_LOGI(tag, "SECURE: encrypted=1 authenticated=1 bonded=1 SC=1 key_size=16 trusted=1"); }
        break;
    }
    case BLE_GAP_EVENT_DISCONNECT:
        secure = false; peer = BLE_HS_CONN_HANDLE_NONE; accepted = false;
        ble_npl_callout_stop(&deadline);
        if (forgetting) finish_forget();
        else if (watch_ble_security_status().state == watch_pairing_state_t::Comparing || watch_ble_security_status().state == watch_pairing_state_t::Securing)
            publish(watch_pairing_state_t::Failed);
        break;
    default: break;
    }
}
bool watch_ble_security_ready(uint16_t handle) { return secure.load() && handle == peer; }
watch_pairing_status_t watch_ble_security_status() {
    portENTER_CRITICAL(&lock); const auto result = status; portEXIT_CRITICAL(&lock); return result;
}
bool watch_ble_security_confirm(uint32_t token, bool accept) {
    if (!initialized) return false;
    portENTER_CRITICAL(&lock);
    const bool valid = command == Command::None && status.token == token && status.state == watch_pairing_state_t::Comparing;
    if (valid) { command = accept ? Command::Accept : Command::Reject; command_token = token; }
    portEXIT_CRITICAL(&lock);
    if (valid) ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &command_event);
    return valid;
}
bool watch_ble_security_forget() {
    if (!initialized) return false;
    portENTER_CRITICAL(&lock); const bool valid = command == Command::None; if (valid) command = Command::Forget; portEXIT_CRITICAL(&lock);
    if (valid) ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &command_event);
    return valid;
}
void watch_ble_security_reset() {
    secure = false; peer = BLE_HS_CONN_HANDLE_NONE; accepted = false;
    ble_npl_callout_stop(&deadline); publish(watch_pairing_state_t::Failed);
}
