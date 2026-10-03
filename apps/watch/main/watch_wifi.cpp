#include "watch_wifi.h"
#include "watch_ble.h"

#include <atomic>
#include <stdio.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs.h"

namespace {
constexpr char kTag[] = "watch_wifi";
// Short retry bursts followed by a 60-second cooldown; enabled watches keep recovering.
constexpr int64_t kRetryUs = 5'000'000;
constexpr int64_t kCooldownUs = 60'000'000;
constexpr unsigned kMaxBurst = 5;
enum class Event { Start, Lost, GotIp, Enable, Disable };
QueueHandle_t queue = nullptr;
std::atomic<watch_wifi_state_t> state{watch_wifi_state_t::Disconnected};
std::atomic<bool> request_pending{false};
std::atomic<bool> enabled{true}; // Serialized writes by worker; safe snapshot for BLE init.
nvs_handle_t handle = 0;
bool storage_ready = false;

void set_state(watch_wifi_state_t next)
{
    if (state.exchange(next) != next) {
        ESP_LOGI(kTag, "State: %s", next == watch_wifi_state_t::Connected ? "CONNECTED" :
                 next == watch_wifi_state_t::Connecting ? "CONNECTING" : "DISCONNECTED");
    }
    watch_ble_update_wifi(!enabled.load() ? 3 : next == watch_wifi_state_t::Connected ? 2 :
                          next == watch_wifi_state_t::Connecting ? 1 : 0);
}

void event_handler(void *, esp_event_base_t base, int32_t id, void *)
{
    Event event;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) event = Event::Start;
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) event = Event::Lost;
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) event = Event::GotIp;
    else return;
    // Connectivity events must not be silently dropped; this worker never waits on the event loop.
    xQueueSend(queue, &event, portMAX_DELAY);
}

void persist_intent()
{
    if (!storage_ready) return;
    esp_err_t error = nvs_set_u8(handle, "enabled", enabled ? 1 : 0);
    if (error == ESP_OK) error = nvs_commit(handle);
    if (error != ESP_OK) ESP_LOGW(kTag, "Intent persistence failed: %s", esp_err_to_name(error));
}

void worker(void *)
{
    unsigned attempts = 0;
    int64_t retry_at = 0;
    int64_t connection_deadline = 0;
    while (true) {
        Event event;
        if (xQueueReceive(queue, &event, pdMS_TO_TICKS(500)) == pdTRUE) {
            switch (event) {
                case Event::Disable:
                    enabled = false; // Set BEFORE disconnect, so its event cannot reconnect.
                    retry_at = 0;
                    connection_deadline = 0;
                    set_state(watch_wifi_state_t::Disconnected);
                    {
                        const esp_err_t error = esp_wifi_disconnect();
                        if (error != ESP_OK && error != ESP_ERR_WIFI_NOT_CONNECT) {
                            ESP_LOGW(kTag, "Disconnect failed: %s", esp_err_to_name(error));
                        }
                    }
                    persist_intent();
                    request_pending = false;
                    break;
                case Event::Enable:
                    enabled = true;
                    set_state(watch_wifi_state_t::Connecting);
                    attempts = 0;
                    retry_at = esp_timer_get_time();
                    persist_intent();
                    request_pending = false;
                    break;
                case Event::Start:
                    if (enabled) retry_at = esp_timer_get_time();
                    break;
                case Event::GotIp:
                    if (enabled) {
                        set_state(watch_wifi_state_t::Connected);
                        attempts = 0;
                        retry_at = 0;
                        connection_deadline = 0;
                    } else esp_wifi_disconnect();
                    break;
                case Event::Lost:
                    set_state(watch_wifi_state_t::Disconnected);
                    connection_deadline = 0;
                    if (enabled) retry_at = esp_timer_get_time() +
                        (attempts >= kMaxBurst ? kCooldownUs : kRetryUs);
                    break;
            }
        }
        const int64_t now = esp_timer_get_time();
        // Also bound association/DHCP stalls, where no GOT_IP/DISCONNECTED event arrives.
        if (enabled && connection_deadline && now >= connection_deadline) {
            connection_deadline = 0;
            esp_wifi_disconnect();
            set_state(watch_wifi_state_t::Disconnected);
            retry_at = now + kCooldownUs;
        }
        if (enabled && retry_at && now >= retry_at) {
            retry_at = 0;
            if (attempts >= kMaxBurst) attempts = 0;
            ++attempts;
            set_state(watch_wifi_state_t::Connecting);
            const esp_err_t error = esp_wifi_connect();
            if (error != ESP_OK) {
                ESP_LOGW(kTag, "Connect failed: %s", esp_err_to_name(error));
                set_state(watch_wifi_state_t::Disconnected);
                retry_at = now + kCooldownUs;
            } else connection_deadline = now + 30'000'000;
        }
    }
}
} // namespace

esp_err_t watch_wifi_init(void)
{
    esp_err_t error = nvs_open("watch_wifi", NVS_READWRITE, &handle);
    storage_ready = error == ESP_OK;
    if (storage_ready) {
        uint8_t value = 1;
        error = nvs_get_u8(handle, "enabled", &value);
        if (error == ESP_OK && value <= 1) enabled = value != 0;
        else if (error != ESP_ERR_NVS_NOT_FOUND) ESP_LOGW(kTag, "Invalid/unreadable Wi-Fi intent");
    } else ESP_LOGW(kTag, "NVS unavailable; defaulting to enabled");
    ESP_LOGI(kTag, "Restored user intent: %s", enabled ? "enabled" : "disabled");
    queue = xQueueCreate(16, sizeof(Event));
    if (!queue) return ESP_ERR_NO_MEM;
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&config));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, nullptr));
    wifi_config_t credentials = {};
    snprintf(reinterpret_cast<char *>(credentials.sta.ssid), sizeof(credentials.sta.ssid), "%s", WIFI_SSID);
    snprintf(reinterpret_cast<char *>(credentials.sta.password), sizeof(credentials.sta.password), "%s", WIFI_PASSWORD);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &credentials));
    if (xTaskCreate(worker, "connectivity", 4096, nullptr, 5, nullptr) != pdPASS) return ESP_ERR_NO_MEM;
    return esp_wifi_start();
}

bool watch_wifi_request_enabled(bool value)
{
    if (!queue || request_pending.exchange(true)) return false;
    if (value && state.load() != watch_wifi_state_t::Disconnected) {
        request_pending = false;
        return false;
    }
    const Event event = value ? Event::Enable : Event::Disable;
    if (xQueueSend(queue, &event, 0) != pdTRUE) { request_pending = false; return false; }
    if (value) set_state(watch_wifi_state_t::Connecting);
    return true;
}

watch_wifi_state_t watch_wifi_get_state(void) { return state.load(); }
int watch_wifi_get_rssi(void)
{
    wifi_ap_record_t info = {};
    return state.load() == watch_wifi_state_t::Connected && esp_wifi_sta_get_ap_info(&info) == ESP_OK ? info.rssi : 0;
}

bool watch_wifi_is_enabled(void) { return enabled.load(); }
