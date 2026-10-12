#pragma once
#include <stdint.h>
typedef int esp_err_t;
typedef uint32_t nvs_handle_t;
enum nvs_open_mode_t { NVS_READONLY, NVS_READWRITE };
constexpr esp_err_t ESP_OK = 0, ESP_ERR_NVS_NOT_FOUND = 1, ESP_ERR_NVS_TYPE_MISMATCH = 2, ESP_FAIL = 3;
const char *esp_err_to_name(esp_err_t);
esp_err_t nvs_open(const char *, nvs_open_mode_t, nvs_handle_t *);
esp_err_t nvs_get_u32(nvs_handle_t, const char *, uint32_t *);
esp_err_t nvs_set_u32(nvs_handle_t, const char *, uint32_t);
esp_err_t nvs_commit(nvs_handle_t);
void nvs_close(nvs_handle_t);
