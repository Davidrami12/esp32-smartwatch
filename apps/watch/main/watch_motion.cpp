#include "watch_motion.h"

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "qmi8658.h"
#include "watch_power.h"

namespace {

constexpr char kTag[] = "watch_motion";
constexpr uint8_t kAddress = QMI8658_ADDRESS_HIGH;
constexpr uint32_t kActivePeriodMs = 50; // 20 Hz application sampling
constexpr uint32_t kIdlePeriodMs = 500;  // 2 Hz application sampling
constexpr int64_t kActiveLogPeriodUs = 2'000'000;
constexpr int64_t kIdleLogPeriodUs = 10'000'000;

qmi8658_dev_t device = {};
bool available = false;
bool has_sample = false;
watch_motion_data_t latest = {};
portMUX_TYPE data_lock = portMUX_INITIALIZER_UNLOCKED;

qmi8658_accel_odr_t sensor_odr_for_state(bool active)
{
    return active ? QMI8658_ACCEL_ODR_62_5HZ : QMI8658_ACCEL_ODR_31_25HZ;
}

void sampling_task(void *)
{
    bool configured_active = watch_power_is_active();
    int64_t last_log_us = 0;

    while (true) {
        const bool active = watch_power_is_active();
        if (active != configured_active) {
            esp_err_t error = qmi8658_set_accel_odr(&device, sensor_odr_for_state(active));
            if (error == ESP_OK) {
                configured_active = active;
                ESP_LOGI(kTag, "Accelerometer ODR set to %s state rate",
                         active ? "ACTIVE (62.5 Hz)" : "IDLE (31.25 Hz)");
            } else {
                ESP_LOGW(kTag, "Could not change accelerometer ODR: %s", esp_err_to_name(error));
            }
        }

        bool ready = false;
        esp_err_t error = qmi8658_is_data_ready(&device, &ready);
        if (error == ESP_OK && ready) {
            qmi8658_data_t raw = {};
            error = qmi8658_read_sensor_data(&device, &raw);
            if (error == ESP_OK) {
                watch_motion_data_t sample = {
                    .acceleration_x_mps2 = raw.accelX,
                    .acceleration_y_mps2 = raw.accelY,
                    .acceleration_z_mps2 = raw.accelZ,
                    .sampled_at_ms = static_cast<uint64_t>(esp_timer_get_time() / 1000)
                };
                portENTER_CRITICAL(&data_lock);
                latest = sample;
                has_sample = true;
                portEXIT_CRITICAL(&data_lock);

                const int64_t now_us = esp_timer_get_time();
                const int64_t log_period = active ? kActiveLogPeriodUs : kIdleLogPeriodUs;
                if (now_us - last_log_us >= log_period) {
                    ESP_LOGI(kTag, "accel m/s^2: x=% .3f y=% .3f z=% .3f (%s)",
                             sample.acceleration_x_mps2,
                             sample.acceleration_y_mps2,
                             sample.acceleration_z_mps2,
                             active ? "ACTIVE" : "IDLE");
                    last_log_us = now_us;
                }
            }
        }

        if (error != ESP_OK) {
            ESP_LOGW(kTag, "Sensor sample failed: %s", esp_err_to_name(error));
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            vTaskDelay(pdMS_TO_TICKS(active ? kActivePeriodMs : kIdlePeriodMs));
        }
    }
}

} // namespace

esp_err_t watch_motion_init(void)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (bus == nullptr) {
        ESP_LOGW(kTag, "BSP I2C bus is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t error = qmi8658_init(&device, bus, kAddress);
    if (error != ESP_OK) {
        ESP_LOGW(kTag, "QMI8658 probe/init at 0x%02X failed: %s",
                 kAddress, esp_err_to_name(error));
        return error;
    }

    uint8_t who_am_i = 0;
    error = qmi8658_get_who_am_i(&device, &who_am_i);
    if (error != ESP_OK || who_am_i != 0x05) {
        ESP_LOGW(kTag, "QMI8658 identity check failed (WHO_AM_I=0x%02X): %s",
                 who_am_i, esp_err_to_name(error == ESP_OK ? ESP_ERR_NOT_FOUND : error));
        return error == ESP_OK ? ESP_ERR_NOT_FOUND : error;
    }

    // The component defaults to faster accel+gyro operation; explicitly configure
    // a watch-oriented accelerometer-only mode and leave the gyroscope disabled.
    error = qmi8658_set_accel_range(&device, QMI8658_ACCEL_RANGE_4G);
    if (error == ESP_OK) {
        error = qmi8658_set_accel_odr(&device, sensor_odr_for_state(watch_power_is_active()));
    }
    if (error == ESP_OK) {
        qmi8658_set_accel_unit_mps2(&device, true);
        error = qmi8658_enable_sensors(&device, QMI8658_ENABLE_ACCEL);
    }
    if (error != ESP_OK) {
        ESP_LOGW(kTag, "QMI8658 configuration failed: %s", esp_err_to_name(error));
        return error;
    }

    available = true;
    ESP_LOGI(kTag, "QMI8658 ready at 0x%02X: accel ±4 g, gyro disabled", kAddress);
    if (xTaskCreate(sampling_task, "motion_sample", 3072, nullptr, 4, nullptr) != pdPASS) {
        available = false;
        ESP_LOGE(kTag, "Could not start motion sampling task");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

bool watch_motion_is_available(void)
{
    return available;
}

bool watch_motion_read_latest(watch_motion_data_t *out)
{
    if (out == nullptr || !available) {
        return false;
    }

    portENTER_CRITICAL(&data_lock);
    const bool valid = has_sample;
    if (valid) {
        *out = latest;
    }
    portEXIT_CRITICAL(&data_lock);
    return valid;
}
