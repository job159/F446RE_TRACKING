#include "App/ldr_tracking.h"
#include "App/tracking_config.h"
#include <string.h>

/*
 * LDR排列(從正面看)：
 *   0=左上  1=右上
 *   3=左下  2=右下
 * error_x = (右-左)/total
 * error_y = (上-下)/total
 */

/* 重新計算delta, total, contrast, error */
static void recompute(LdrTracking_HandleTypeDef *h)
{
  uint32_t total = 0;
  uint16_t dmin = 0xFFFF, dmax = 0;

  for (int i = 0; i < LDR_CHANNEL_COUNT; i++)
  {
    /* 扣掉baseline + noise floor才算有效的光增量 */
    uint32_t floor = (uint32_t)h->frame.baseline[i] + h->frame.noise_floor[i];
    if (floor > ADC_12BIT_MAX) floor = ADC_12BIT_MAX;

    if (h->frame.calibration_done && h->frame.raw[i] > floor)
      h->frame.delta[i] = h->frame.raw[i] - (uint16_t)floor;
    else
      h->frame.delta[i] = 0;

    total += h->frame.delta[i];
    if (h->frame.delta[i] < dmin) dmin = h->frame.delta[i];
    if (h->frame.delta[i] > dmax) dmax = h->frame.delta[i];
  }

  h->frame.total = (uint16_t)total;
  h->frame.contrast = dmax - dmin;

  /* 判斷光源是否有效 */
  if (h->frame.calibration_done &&
      total >= TRACK_VALID_TOTAL_MIN &&
      h->frame.contrast >= TRACK_DIRECTION_CONTRAST_MIN)
  {
    uint32_t left  = (uint32_t)h->frame.delta[0] + h->frame.delta[3];
    uint32_t right = (uint32_t)h->frame.delta[1] + h->frame.delta[2];
    uint32_t top   = (uint32_t)h->frame.delta[0] + h->frame.delta[1];
    uint32_t bot   = (uint32_t)h->frame.delta[3] + h->frame.delta[2];

    /* 一邊對子全黑一邊亮 → error 假性飽和到 ±1.0,會讓對應軸來回抖。
     * 出現這種狀況時該軸 error 直接設 0 (該軸本 frame 停)。 */
    if (left > 0 && right > 0)
      h->frame.error_x = (float)((int32_t)right - (int32_t)left) / (float)total;
    else
      h->frame.error_x = 0.0f;

    if (top > 0 && bot > 0)
      h->frame.error_y = (float)((int32_t)top - (int32_t)bot) / (float)total;
    else
      h->frame.error_y = 0.0f;

    h->frame.is_valid = 1;
  }
  else
  {
    h->frame.error_x = 0.0f;
    h->frame.error_y = 0.0f;
    h->frame.is_valid = 0;
  }
}

void LdrTracking_Init(LdrTracking_HandleTypeDef *h)
{
  memset(h, 0, sizeof(*h));
}

void LdrTracking_ForceRecalibration(LdrTracking_HandleTypeDef *h)
{
  memset(h->cal_sum, 0, sizeof(h->cal_sum));
  memset(h->cal_min, 0xFF, sizeof(h->cal_min));  /* 設成最大值好比較 */
  memset(h->cal_max, 0, sizeof(h->cal_max));
  h->cal_samples = 0;
  h->frame.calibration_done = 0;
  h->frame.is_valid = 0;
  h->frame.total = 0;
  h->frame.contrast = 0;
  h->frame.error_x = 0.0f;
  h->frame.error_y = 0.0f;
  memset(h->frame.delta, 0, sizeof(h->frame.delta));
}

void LdrTracking_UpdateFrame(LdrTracking_HandleTypeDef *h,
    uint16_t adc1, uint16_t adc2, uint16_t adc3, uint16_t adc4)
{
  h->frame.raw[0] = adc1;
  h->frame.raw[1] = adc2;
  h->frame.raw[2] = adc3;
  h->frame.raw[3] = adc4;
  recompute(h);
}

void LdrTracking_AccumulateCalibration(LdrTracking_HandleTypeDef *h)
{
  for (int i = 0; i < LDR_CHANNEL_COUNT; i++)
  {
    uint16_t v = h->frame.raw[i];
    h->cal_sum[i] += v;

    if (h->cal_samples == 0)
    {
      h->cal_min[i] = v;
      h->cal_max[i] = v;
    }
    else
    {
      if (v < h->cal_min[i]) h->cal_min[i] = v;
      if (v > h->cal_max[i]) h->cal_max[i] = v;
    }
  }
  h->cal_samples++;
}

void LdrTracking_FinalizeCalibration(LdrTracking_HandleTypeDef *h)
{
  if (h->cal_samples == 0) return;

  for (int i = 0; i < LDR_CHANNEL_COUNT; i++)
  {
    uint16_t span = h->cal_max[i] - h->cal_min[i];
    h->frame.baseline[i] = (uint16_t)(h->cal_sum[i] / h->cal_samples);

    /* noise floor 取 span+margin 跟最小值的較大者 */
    uint16_t nf = span + LDR_BASELINE_MARGIN;
    if (nf < LDR_MIN_NOISE_FLOOR) nf = LDR_MIN_NOISE_FLOOR;
    h->frame.noise_floor[i] = nf;
  }

  h->frame.calibration_done = 1;
  recompute(h);
}
