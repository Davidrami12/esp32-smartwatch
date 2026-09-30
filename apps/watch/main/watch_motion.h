#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    float acceleration_x_mps2;
    float acceleration_y_mps2;
    float acceleration_z_mps2;
    uint64_t sampled_at_ms;
} watch_motion_data_t;

// Initializes QMI8658 on the already-initialized BSP I2C bus and starts sampling.
// Returns an error when unavailable; the rest of the watch can continue normally.
esp_err_t watch_motion_init(void);
bool watch_motion_is_available(void);
// Returns false until the first successful sample, or when out is null/unavailable.
bool watch_motion_read_latest(watch_motion_data_t *out);
