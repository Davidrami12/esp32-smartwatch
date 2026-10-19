#include "watch_ble_security.h"
#include "watch_ble_security_policy.h"
#include <atomic>
#include <string.h>
#include "esp_log.h"
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
uint16_t peer = BLE_HS_CONN_HANDLE_NONE;
bool accepted = false, storage_failed = false, forgetting = false;
std::atomic<bool> secure{false};
std::atomic<bool> initialized{false};
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
    // UI bookkeeping only. NimBLE owns persistent bonds; there is no separate
    // persistent trusted-phone slot or reservation gate.
    trust = value; return true;
}
bool matches(const ble_addr_t &address) {
    return trust.phase == 2 && trust.type == address.type && memcmp(trust.address, address.val, 6) == 0;
}
void terminate() {
    secure = false;
    if (peer != BLE_HS_CONN_HANDLE_NONE) ble_gap_terminate(peer, BLE_ERR_AUTH_FAIL);
}
int storage_full(ble_store_status_event *event, void *) {
    if (event->event_code == BLE_STORE_EVENT_OVERFLOW &&
        event->overflow.obj_type == BLE_STORE_OBJ_TYPE_PEER_ADDR) {
        const auto &mapping = event->overflow.value->rpa_rec;
        // Reclaim only this explicitly confirmed / already trusted identity's
        // obsolete RPA mappings. Never remove security keys or another phone.
        if (!forgetting && !storage_failed &&
            ((accepted && trust.phase == 1) || matches(mapping.peer_addr))) {
            union ble_store_key key = {};
            key.rpa_rec.peer_rpa_addr = mapping.peer_addr;
            int removed = 0;
            for (int i = 0; i < CONFIG_BT_NIMBLE_MAX_BONDS; ++i) {
                const int rc = ble_store_delete(BLE_STORE_OBJ_TYPE_PEER_ADDR, &key);
                if (rc == BLE_HS_ENOENT) break;
                if (rc) { storage_failed = true; secure = false; return rc; }
                ++removed;
            }
            ESP_LOGI(tag, "Private-address mapping reclamation: removed=%d same_identity_only=1", removed);
            if (removed) return 0; // NimBLE retries the failed mapping write.
        }
    }
    // FULL is a pessimistic preflight (stored bonds + active SM procedures),
    // not a failed write. Permit it only if the actual store has a free slot.
    // OVERFLOW remains fatal; never evict the trusted companion.
    if (event->event_code == BLE_STORE_EVENT_FULL) {
        int count = 0;
        const int rc = ble_store_util_count(event->full.obj_type, &count);
        ESP_LOGI(tag, "Bond capacity preflight: type=%d count=%d capacity=%d rc=%d",
                 event->full.obj_type, count, CONFIG_BT_NIMBLE_MAX_BONDS, rc);
        if (!rc && count < CONFIG_BT_NIMBLE_MAX_BONDS) return 0;
    }
    ESP_LOGE(tag, "Bond storage capacity denied: event=%d type=%d", event->event_code,
             event->event_code == BLE_STORE_EVENT_OVERFLOW ? event->overflow.obj_type : event->full.obj_type);
    // Never use ble_store_util_status_rr: it evicts the trusted companion.
    storage_failed = true; secure = false; return BLE_HS_ENOMEM;
}
void finish_forget() {
    // Clear NimBLE bonds/CCCDs first. Clear trust last so interruption remains locked.
    const int clear_result = ble_store_clear();
    if (clear_result != 0 || !persist(Trust{1, 0, 0, {}})) {
        ESP_LOGE(tag,"Forget failed: bond_clear_rc=%d persistence_failed=%d",clear_result,storage_failed);
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
        else {
            ble_gap_conn_desc descriptor;
            // A stale handle must not prevent the user's explicit Forget operation.
            if (ble_gap_conn_find(peer,&descriptor) == BLE_HS_ENOTCONN) {
                peer = BLE_HS_CONN_HANDLE_NONE; finish_forget();
            } else {
                const int rc = ble_gap_terminate(peer, BLE_ERR_REM_USER_CONN_TERM);
                if (rc) { ESP_LOGE(tag,"Forget disconnect failed rc=%d",rc); forgetting = false; publish(watch_pairing_state_t::Failed); }
            }
        }
        return;
    }
    const auto view = watch_ble_security_status();
    if (view.token != token || view.state != watch_pairing_state_t::Comparing || peer == BLE_HS_CONN_HANDLE_NONE) return;
    ble_sm_io io = {}; io.action = BLE_SM_IOACT_NUMCMP;
    if (next == Command::Accept) {
        persist(Trust{1, 1, 0, {}});
        accepted = true; io.numcmp_accept = 1; publish(watch_pairing_state_t::Securing);
    } else { io.numcmp_accept = 0; publish(watch_pairing_state_t::Rejected); }
    if (ble_sm_inject_io(peer, &io)) { publish(watch_pairing_state_t::Failed); terminate(); }
}
}

esp_err_t watch_ble_security_init() {
    ble_store_config_init();
    ble_hs_cfg.store_status_cb = storage_full;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_DISPLAY_YESNO;
    ble_hs_cfg.sm_bonding = ble_hs_cfg.sm_mitm = ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_npl_event_init(&command_event, process_command, nullptr);
    ble_npl_callout_init(&deadline, nimble_port_get_dflt_eventq(), timed_out, nullptr);
    publish(trust.phase == 2 ? watch_pairing_state_t::Trusted : trust.phase ? watch_pairing_state_t::Locked : watch_pairing_state_t::Unpaired);
    initialized = true;
    ESP_LOGI(tag, "Standard NimBLE bonding: numeric comparison, MITM, 128-bit encryption; custom trust gate removed");
    return ESP_OK;
}

void watch_ble_security_gap(ble_gap_event *event) {
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT: {
        if (event->connect.status) break;
        peer = event->connect.conn_handle; secure = false; accepted = false;
        storage_failed = false;
        ble_gap_conn_desc desc;
        if (forgetting || ble_gap_conn_find(peer, &desc)) {
            ESP_LOGW(tag,"Trust rejection context: phase=%u storage_failed=%d forgetting=%d",trust.phase,storage_failed,forgetting);
            ESP_LOGW(tag, "Untrusted connection rejected (trusted slot occupied/locked)"); terminate(); break;
        }
        ble_npl_callout_reset(&deadline, ble_npl_time_ms_to_ticks32(60000));
        // Android initiates createBond / encryption via protected GATT reads. Do not race
        // the central with a simultaneous peripheral Security Request on bonded reconnect.
        break;
    }
    case BLE_GAP_EVENT_PASSKEY_ACTION:
        if (event->passkey.conn_handle != peer) break;
        if (event->passkey.params.action != BLE_SM_IOACT_NUMCMP || forgetting) {
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
        bool valid = !forgetting && !event->enc_change.status && !ble_gap_conn_find(peer, &desc);
        if (valid) valid = desc.sec_state.encrypted && desc.sec_state.authenticated &&
                           desc.sec_state.bonded && desc.sec_state.key_size == 16;
        if (valid) {
            Trust next = {1, 2, desc.peer_id_addr.type, {}};
            memcpy(next.address, desc.peer_id_addr.val, 6); valid = persist(next);
        }
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
