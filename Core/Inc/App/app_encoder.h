#ifndef APP_ENCODER_H
#define APP_ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* 17HS15-1504-ME1K encoder spec: 1000 pulse/rev. In x4 encoder mode => 4000 count/rev. */
#define APP_ENCODER_PULSE_PER_REV 1000L
#define APP_ENCODER_QUADRATURE_MULTIPLIER 4L
#define APP_ENCODER_COUNTS_PER_REV (APP_ENCODER_PULSE_PER_REV * APP_ENCODER_QUADRATURE_MULTIPLIER)
#define APP_ENCODER_ANGLE_SCALE 10000UL /* 1.0000 deg = 10000 */

typedef struct
{
  TIM_HandleTypeDef *htim_enc1;
  TIM_HandleTypeDef *htim_enc2;
  uint32_t last_counter_enc1;
  uint32_t last_counter_enc2;
  int32_t count_enc1;
  int32_t count_enc2;
} AppEncoder_HandleTypeDef;

void AppEncoder_Init(
    AppEncoder_HandleTypeDef *handle,
    TIM_HandleTypeDef *htim_enc1,
    TIM_HandleTypeDef *htim_enc2);

void AppEncoder_Task(AppEncoder_HandleTypeDef *handle);

int32_t AppEncoder_GetCount1(const AppEncoder_HandleTypeDef *handle);
int32_t AppEncoder_GetCount2(const AppEncoder_HandleTypeDef *handle);
uint32_t AppEncoder_GetAngle1X10000(const AppEncoder_HandleTypeDef *handle);
uint32_t AppEncoder_GetAngle2X10000(const AppEncoder_HandleTypeDef *handle);

#ifdef __cplusplus
}
#endif

#endif /* APP_ENCODER_H */
