#pragma once

#include <stdint.h>
#include "watch_motion.h"

// Resets the volatile step counter and detector state (no persistent storage).
void watch_steps_init(void);
// Feed one fresh QMI8658 acceleration sample to the detector.
void watch_steps_process_sample(const watch_motion_data_t *sample);
uint32_t watch_steps_get_count(void);
void watch_steps_set_count(uint32_t count);
void watch_steps_reset(void);
