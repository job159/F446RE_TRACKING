#ifndef APP_LDR_TRACKING_H
#define APP_LDR_TRACKING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "App/tracking_types.h"

typedef struct
{
  LdrTrackingFrame_t frame;
  uint32_t calibration_sum[4];
  uint16_t calibration_min[4];
  uint16_t calibration_max[4];
  uint32_t calibration_samples;
} LdrTracking_HandleTypeDef;

void LdrTracking_Init(LdrTracking_HandleTypeDef *handle);
void LdrTracking_ForceRecalibration(LdrTracking_HandleTypeDef *handle);
void LdrTracking_UpdateFrame(
    LdrTracking_HandleTypeDef *handle,
    uint16_t adc1_value,
    uint16_t adc2_value,
    uint16_t adc3_value,
    uint16_t adc4_value);
void LdrTracking_AccumulateCalibration(LdrTracking_HandleTypeDef *handle);
void LdrTracking_FinalizeCalibration(LdrTracking_HandleTypeDef *handle);

#ifdef __cplusplus
}
#endif

#endif /* APP_LDR_TRACKING_H */
