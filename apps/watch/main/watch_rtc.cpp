#include "watch_rtc.h"

#include <stdint.h>
#include <string.h>

#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

namespace {

constexpr uint8_t kAddress = 0x51;
constexpr uint8_t kControl1 = 0x00;
constexpr uint8_t kSecondsRegister = 0x04;
constexpr uint8_t kControl1_12_24 = 1u << 1;
constexpr uint8_t kSecondsOscillatorStop = 1u << 7;
constexpr uint32_t kI2cTimeoutMs = 1000;
constexpr char kTag[] = "watch_rtc";

i2c_master_dev_handle_t device = nullptr;

bool decode_bcd(uint8_t raw, uint8_t mask, uint8_t *value)
{
    const uint8_t bcd = raw & mask;
    const uint8_t ones = bcd & 0x0f;
    const uint8_t tens = (bcd >> 4) & 0x0f;
    if (ones > 9 || tens > 9) {
        return false;
    }
    *value = static_cast<uint8_t>(tens * 10 + ones);
    return true;
}

uint8_t encode_bcd(uint8_t value)
{
    return static_cast<uint8_t>(((value / 10) << 4) | (value % 10));
}

bool valid_date(const struct tm &utc)
{
    if (utc.tm_year < 100 || utc.tm_year > 199 ||
        utc.tm_mon < 0 || utc.tm_mon > 11 ||
        utc.tm_mday < 1 || utc.tm_mday > 31 ||
        utc.tm_hour < 0 || utc.tm_hour > 23 ||
        utc.tm_min < 0 || utc.tm_min > 59 ||
        utc.tm_sec < 0 || utc.tm_sec > 59) {
        return false;
    }

    struct tm candidate = utc;
    candidate.tm_isdst = 0;
    const time_t converted = timegm(&candidate);
    struct tm round_trip = {};
    gmtime_r(&converted, &round_trip);
    return round_trip.tm_year == utc.tm_year &&
           round_trip.tm_mon == utc.tm_mon &&
           round_trip.tm_mday == utc.tm_mday &&
           round_trip.tm_hour == utc.tm_hour &&
           round_trip.tm_min == utc.tm_min &&
           round_trip.tm_sec == utc.tm_sec;
}

} // namespace

esp_err_t watch_rtc_init(void)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (bus == nullptr) {
        ESP_LOGE(kTag, "BSP I2C bus is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    i2c_device_config_t config = {};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = kAddress;
    config.scl_speed_hz = 400000;
    esp_err_t error = i2c_master_bus_add_device(bus, &config, &device);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "Failed to add PCF85063: %s", esp_err_to_name(error));
        return error;
    }

    uint8_t control = 0;
    error = i2c_master_transmit_receive(device, &kControl1, 1, &control, 1,
                                        kI2cTimeoutMs);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "PCF85063 probe failed: %s", esp_err_to_name(error));
        i2c_master_bus_rm_device(device);
        device = nullptr;
        return error;
    }

    control &= static_cast<uint8_t>(~kControl1_12_24);
    const uint8_t control_write[] = {kControl1, control};
    error = i2c_master_transmit(device, control_write, sizeof(control_write),
                                kI2cTimeoutMs);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "Failed to select 24-hour mode: %s", esp_err_to_name(error));
    }
    return error;
}

bool watch_rtc_read_utc(time_t *timestamp)
{
    if (device == nullptr || timestamp == nullptr) {
        return false;
    }

    uint8_t registers[7] = {};
    esp_err_t error = i2c_master_transmit_receive(
        device, &kSecondsRegister, 1, registers, sizeof(registers), kI2cTimeoutMs);
    if (error != ESP_OK) {
        ESP_LOGW(kTag, "RTC read failed: %s", esp_err_to_name(error));
        return false;
    }
    if ((registers[0] & kSecondsOscillatorStop) != 0) {
        ESP_LOGW(kTag, "RTC oscillator-stop flag indicates invalid time");
        return false;
    }

    uint8_t second, minute, hour, day, weekday, month, year;
    if (!decode_bcd(registers[0], 0x7f, &second) ||
        !decode_bcd(registers[1], 0x7f, &minute) ||
        !decode_bcd(registers[2], 0x3f, &hour) ||
        !decode_bcd(registers[3], 0x3f, &day) ||
        !decode_bcd(registers[4], 0x07, &weekday) ||
        !decode_bcd(registers[5], 0x1f, &month) ||
        !decode_bcd(registers[6], 0xff, &year) ||
        weekday > 6 || month < 1 || month > 12) {
        ESP_LOGW(kTag, "RTC contains malformed BCD/calendar fields");
        return false;
    }

    struct tm utc = {};
    utc.tm_sec = second;
    utc.tm_min = minute;
    utc.tm_hour = hour;
    utc.tm_mday = day;
    utc.tm_mon = month - 1;
    utc.tm_year = 100 + year;
    utc.tm_wday = weekday;
    if (!valid_date(utc)) {
        ESP_LOGW(kTag, "RTC date is outside supported calendar range");
        return false;
    }

    *timestamp = timegm(&utc);
    ESP_LOGI(kTag, "Read valid UTC time from PCF85063");
    return true;
}

esp_err_t watch_rtc_write_utc(time_t timestamp)
{
    if (device == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    struct tm utc = {};
    gmtime_r(&timestamp, &utc);
    if (!valid_date(utc)) {
        return ESP_ERR_INVALID_ARG;
    }

    // Seconds..Years (0x04..0x0A) are written as one contiguous transfer.
    const uint8_t registers[] = {
        kSecondsRegister,
        encode_bcd(static_cast<uint8_t>(utc.tm_sec)),
        encode_bcd(static_cast<uint8_t>(utc.tm_min)),
        encode_bcd(static_cast<uint8_t>(utc.tm_hour)),
        encode_bcd(static_cast<uint8_t>(utc.tm_mday)),
        encode_bcd(static_cast<uint8_t>(utc.tm_wday)),
        encode_bcd(static_cast<uint8_t>(utc.tm_mon + 1)),
        encode_bcd(static_cast<uint8_t>(utc.tm_year - 100))
    };

    esp_err_t error = i2c_master_transmit(device, registers, sizeof(registers),
                                         kI2cTimeoutMs);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "RTC write failed: %s", esp_err_to_name(error));
    } else {
        ESP_LOGI(kTag, "Synchronized PCF85063 from system UTC time");
    }
    return error;
}
