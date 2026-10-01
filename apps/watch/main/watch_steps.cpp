#include "watch_steps.h"

#include <inttypes.h>
#include <math.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"

namespace {

constexpr char kTag[] = "watch_steps";

// Detector tuning values (SI units; initial values require on-wrist validation).
constexpr float kGravityFilterAlpha = 0.025f; // remove slow gravity/orientation changes
constexpr float kMotionSmoothAlpha = 0.35f;   // suppress short, sharp sensor noise
constexpr float kPeakThresholdMps2 = 1.0f;    // minimum dynamic acceleration peak
constexpr float kRearmThresholdMps2 = 0.25f;  // must fall below this before next peak
constexpr float kPeakProminenceMps2 = 0.25f;  // confirm a peak on its falling edge
constexpr uint64_t kMinimumStepIntervalMs = 300; // cap at 200 steps/minute
constexpr uint64_t kStepLogIntervalMs = 1000; // at most one detection log per second

portMUX_TYPE count_lock = portMUX_INITIALIZER_UNLOCKED;
uint32_t step_count = 0;

bool filters_initialized = false;
float gravity_mps2 = 0.0f;
float filtered_motion_mps2 = 0.0f;
bool armed_for_peak = true;
bool tracking_peak = false;
float candidate_peak_mps2 = 0.0f;
uint64_t candidate_peak_time_ms = 0;
uint64_t last_step_time_ms = 0;
uint64_t last_step_log_time_ms = 0;

void reset_detector_state(void)
{
    filters_initialized = false;
    gravity_mps2 = 0.0f;
    filtered_motion_mps2 = 0.0f;
    armed_for_peak = true;
    tracking_peak = false;
    candidate_peak_mps2 = 0.0f;
    candidate_peak_time_ms = 0;
    last_step_time_ms = 0;
    last_step_log_time_ms = 0;
}

} // namespace

void watch_steps_init(void)
{
    watch_steps_reset();
}

void watch_steps_reset(void)
{
    portENTER_CRITICAL(&count_lock);
    step_count = 0;
    portEXIT_CRITICAL(&count_lock);
    reset_detector_state();
}

void watch_steps_process_sample(const watch_motion_data_t *sample)
{
    if (sample == nullptr) {
        return;
    }

    const float x = sample->acceleration_x_mps2;
    const float y = sample->acceleration_y_mps2;
    const float z = sample->acceleration_z_mps2;
    const float magnitude_mps2 = sqrtf(x * x + y * y + z * z);

    if (!filters_initialized) {
        gravity_mps2 = magnitude_mps2;
        filtered_motion_mps2 = 0.0f;
        filters_initialized = true;
        return;
    }

    // Magnitude makes the detector largely independent of watch orientation.
    gravity_mps2 += kGravityFilterAlpha * (magnitude_mps2 - gravity_mps2);
    const float dynamic_mps2 = magnitude_mps2 - gravity_mps2;
    filtered_motion_mps2 += kMotionSmoothAlpha * (dynamic_mps2 - filtered_motion_mps2);

    if (tracking_peak) {
        if (filtered_motion_mps2 > candidate_peak_mps2) {
            candidate_peak_mps2 = filtered_motion_mps2;
            candidate_peak_time_ms = sample->sampled_at_ms;
        }

        // Confirm a local peak only after its filtered signal drops by the
        // prominence amount; this is the high/low hysteresis stage.
        if (filtered_motion_mps2 <= candidate_peak_mps2 - kPeakProminenceMps2) {
            const bool high_enough = candidate_peak_mps2 >= kPeakThresholdMps2;
            const bool interval_ok = last_step_time_ms == 0 ||
                candidate_peak_time_ms - last_step_time_ms >= kMinimumStepIntervalMs;
            if (high_enough && interval_ok) {
                uint32_t count;
                portENTER_CRITICAL(&count_lock);
                count = ++step_count;
                portEXIT_CRITICAL(&count_lock);

                last_step_time_ms = candidate_peak_time_ms;
                if (candidate_peak_time_ms - last_step_log_time_ms >= kStepLogIntervalMs) {
                    ESP_LOGI(kTag, "step=%" PRIu32 " peak=%.2f m/s^2",
                             count, candidate_peak_mps2);
                    last_step_log_time_ms = candidate_peak_time_ms;
                }
            }
            tracking_peak = false;
            armed_for_peak = filtered_motion_mps2 <= kRearmThresholdMps2;
        }
    } else {
        if (!armed_for_peak && filtered_motion_mps2 <= kRearmThresholdMps2) {
            armed_for_peak = true;
        }
        if (armed_for_peak && filtered_motion_mps2 >= kPeakThresholdMps2) {
            tracking_peak = true;
            candidate_peak_mps2 = filtered_motion_mps2;
            candidate_peak_time_ms = sample->sampled_at_ms;
            armed_for_peak = false;
        }
    }
}

uint32_t watch_steps_get_count(void)
{
    portENTER_CRITICAL(&count_lock);
    const uint32_t count = step_count;
    portEXIT_CRITICAL(&count_lock);
    return count;
}

void watch_steps_set_count(uint32_t count)
{
    portENTER_CRITICAL(&count_lock);
    step_count = count;
    portEXIT_CRITICAL(&count_lock);
}
