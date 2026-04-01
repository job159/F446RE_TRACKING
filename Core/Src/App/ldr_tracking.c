#include "App/ldr_tracking.h"

#include "App/tracking_config.h"

#include <string.h>

/*
 * Physical LDR order follows clockwise numbering:
 * 1 = top-left, 2 = top-right, 3 = bottom-right, 4 = bottom-left.
 * raw[0..3] keeps adc1..adc4 order, so left/right must map 4th sensor before 3rd.
 */
#define LDR_IDX_TOP_LEFT      0U
#define LDR_IDX_TOP_RIGHT     1U
#define LDR_IDX_BOTTOM_RIGHT  2U
#define LDR_IDX_BOTTOM_LEFT   3U

static uint16_t LdrTracking_MaxU16(uint16_t a, uint16_t b)
{
  return (a > b) ? a : b;
}

static void LdrTracking_Recompute(LdrTracking_HandleTypeDef *handle)
{
  uint32_t total = 0U;
  uint16_t min_delta = 0U;
  uint16_t max_delta = 0U;
  uint32_t sum_left;
  uint32_t sum_right;
  uint32_t sum_top;
  uint32_t sum_bottom;
  uint32_t index;

  if (handle == NULL)
  {
    return;
  }

  for (index = 0U; index < LDR_CHANNEL_COUNT; index++)
  {
    uint32_t effective_baseline = (uint32_t)handle->frame.baseline[index] + (uint32_t)handle->frame.noise_floor[index];

    if (effective_baseline > 4095U)
    {
      effective_baseline = 4095U;
    }

    if ((handle->frame.calibration_done != 0U) && (handle->frame.raw[index] > effective_baseline))
    {
      handle->frame.delta[index] = (uint16_t)(handle->frame.raw[index] - effective_baseline);
    }
    else
    {
      handle->frame.delta[index] = 0U;
    }

    total += handle->frame.delta[index];

    if (index == 0U)
    {
      min_delta = handle->frame.delta[index];
      max_delta = handle->frame.delta[index];
    }
    else
    {
      if (handle->frame.delta[index] < min_delta)
      {
        min_delta = handle->frame.delta[index];
      }

      if (handle->frame.delta[index] > max_delta)
      {
        max_delta = handle->frame.delta[index];
      }
    }
  }

  handle->frame.total = (uint16_t)total;
  handle->frame.contrast = (uint16_t)(max_delta - min_delta);

  sum_left = (uint32_t)handle->frame.delta[LDR_IDX_TOP_LEFT] + (uint32_t)handle->frame.delta[LDR_IDX_BOTTOM_LEFT];
  sum_right = (uint32_t)handle->frame.delta[LDR_IDX_TOP_RIGHT] + (uint32_t)handle->frame.delta[LDR_IDX_BOTTOM_RIGHT];
  sum_top = (uint32_t)handle->frame.delta[LDR_IDX_TOP_LEFT] + (uint32_t)handle->frame.delta[LDR_IDX_TOP_RIGHT];
  sum_bottom = (uint32_t)handle->frame.delta[LDR_IDX_BOTTOM_LEFT] + (uint32_t)handle->frame.delta[LDR_IDX_BOTTOM_RIGHT];

  if ((handle->frame.calibration_done != 0U) &&
      (handle->frame.total >= TRACK_VALID_TOTAL_MIN) &&
      (handle->frame.contrast >= TRACK_DIRECTION_CONTRAST_MIN))
  {
    handle->frame.error_x = (float)((int32_t)sum_right - (int32_t)sum_left) / (float)handle->frame.total;
    handle->frame.error_y = (float)((int32_t)sum_top - (int32_t)sum_bottom) / (float)handle->frame.total;
    handle->frame.is_valid = 1U;
  }
  else
  {
    handle->frame.error_x = 0.0f;
    handle->frame.error_y = 0.0f;
    handle->frame.is_valid = 0U;
  }
}

void LdrTracking_Init(LdrTracking_HandleTypeDef *handle)
{
  if (handle == NULL)
  {
    return;
  }

  (void)memset(handle, 0, sizeof(*handle));
}

void LdrTracking_ForceRecalibration(LdrTracking_HandleTypeDef *handle)
{
  if (handle == NULL)
  {
    return;
  }

  (void)memset(handle->calibration_sum, 0, sizeof(handle->calibration_sum));
  (void)memset(handle->calibration_min, 0xFF, sizeof(handle->calibration_min));
  (void)memset(handle->calibration_max, 0, sizeof(handle->calibration_max));
  handle->calibration_samples = 0U;
  handle->frame.calibration_done = 0U;
  handle->frame.is_valid = 0U;
  handle->frame.total = 0U;
  handle->frame.contrast = 0U;
  handle->frame.error_x = 0.0f;
  handle->frame.error_y = 0.0f;
  (void)memset(handle->frame.delta, 0, sizeof(handle->frame.delta));
}

void LdrTracking_UpdateFrame(
    LdrTracking_HandleTypeDef *handle,
    uint16_t adc1_value,
    uint16_t adc2_value,
    uint16_t adc3_value,
    uint16_t adc4_value)
{
  if (handle == NULL)
  {
    return;
  }

  handle->frame.raw[0] = adc1_value;
  handle->frame.raw[1] = adc2_value;
  handle->frame.raw[2] = adc3_value;
  handle->frame.raw[3] = adc4_value;

  LdrTracking_Recompute(handle);
}

void LdrTracking_AccumulateCalibration(LdrTracking_HandleTypeDef *handle)
{
  uint32_t index;

  if (handle == NULL)
  {
    return;
  }

  for (index = 0U; index < LDR_CHANNEL_COUNT; index++)
  {
    uint16_t sample = handle->frame.raw[index];

    handle->calibration_sum[index] += sample;

    if (handle->calibration_samples == 0U)
    {
      handle->calibration_min[index] = sample;
      handle->calibration_max[index] = sample;
    }
    else
    {
      if (sample < handle->calibration_min[index])
      {
        handle->calibration_min[index] = sample;
      }

      if (sample > handle->calibration_max[index])
      {
        handle->calibration_max[index] = sample;
      }
    }
  }

  handle->calibration_samples++;
}

void LdrTracking_FinalizeCalibration(LdrTracking_HandleTypeDef *handle)
{
  uint32_t index;

  if ((handle == NULL) || (handle->calibration_samples == 0U))
  {
    return;
  }

  for (index = 0U; index < LDR_CHANNEL_COUNT; index++)
  {
    uint16_t noise_span = (uint16_t)(handle->calibration_max[index] - handle->calibration_min[index]);

    handle->frame.baseline[index] = (uint16_t)(handle->calibration_sum[index] / handle->calibration_samples);
    handle->frame.noise_floor[index] = LdrTracking_MaxU16((uint16_t)(noise_span + LDR_BASELINE_MARGIN),
                                                          LDR_MIN_NOISE_FLOOR);
  }

  handle->frame.calibration_done = 1U;
  LdrTracking_Recompute(handle);
}
