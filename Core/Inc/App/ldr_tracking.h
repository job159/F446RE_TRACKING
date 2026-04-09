#ifndef APP_LDR_TRACKING_H
#define APP_LDR_TRACKING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "App/tracking_types.h"

typedef struct {
  LdrTrackingFrame_t frame;
  uint32_t cal_sum[4];
  uint16_t cal_min[4];
  uint16_t cal_max[4];
  uint32_t cal_samples;
} LdrTracking_HandleTypeDef;

void LdrTracking_Init(LdrTracking_HandleTypeDef *h);
void LdrTracking_ForceRecalibration(LdrTracking_HandleTypeDef *h);
void LdrTracking_UpdateFrame(LdrTracking_HandleTypeDef *h,
    uint16_t adc1, uint16_t adc2, uint16_t adc3, uint16_t adc4);
void LdrTracking_AccumulateCalibration(LdrTracking_HandleTypeDef *h);
void LdrTracking_FinalizeCalibration(LdrTracking_HandleTypeDef *h);

#ifdef __cplusplus
}
#endif

#endif /* APP_LDR_TRACKING_H */
