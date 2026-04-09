#ifndef APP_ENCODER_H
#define APP_ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* 17HS15-1504-ME1K: 1000 pulse/rev, x4 mode => 4000 count/rev */
#define ENCODER_COUNTS_PER_REV   4000L
#define ENCODER_ANGLE_SCALE      10000UL   /* 1.0000 deg = 10000 */

typedef struct {
  TIM_HandleTypeDef *htim1;
  TIM_HandleTypeDef *htim2;
  uint32_t prev_cnt1;
  uint32_t prev_cnt2;
  int32_t count1;
  int32_t count2;
} AppEncoder_HandleTypeDef;

void     AppEncoder_Init(AppEncoder_HandleTypeDef *h, TIM_HandleTypeDef *htim1, TIM_HandleTypeDef *htim2);
void     AppEncoder_Task(AppEncoder_HandleTypeDef *h);
int32_t  AppEncoder_GetCount(const AppEncoder_HandleTypeDef *h, uint8_t axis);
uint32_t AppEncoder_GetAngleX10000(const AppEncoder_HandleTypeDef *h, uint8_t axis);

#ifdef __cplusplus
}
#endif

#endif /* APP_ENCODER_H */
