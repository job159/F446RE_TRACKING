#include "App/app_encoder.h"

/* 把累積count換算成0~359.9999度(x10000表示) */
static uint32_t count_to_angle(int32_t count)
{
  /* 先正規化到一圈以內 */
  int32_t norm = count % ENCODER_COUNTS_PER_REV;
  if (norm < 0) norm += ENCODER_COUNTS_PER_REV;

  uint64_t deg = (uint64_t)(uint32_t)norm * 3600000ULL;  /* 360.0000 * 10000 */
  uint32_t angle = (uint32_t)((deg + ENCODER_COUNTS_PER_REV / 2) / ENCODER_COUNTS_PER_REV);

  if (angle >= 3600000UL) angle = 0;
  return angle;
}

/* 讀取一個encoder的TIM counter，累加差值 */
static void update_one(TIM_HandleTypeDef *htim, uint32_t *prev, int32_t *accum)
{
  uint32_t now = __HAL_TIM_GET_COUNTER(htim);
  *accum += (int32_t)(now - *prev);
  *prev = now;
}

void AppEncoder_Init(AppEncoder_HandleTypeDef *h,
    TIM_HandleTypeDef *htim1, TIM_HandleTypeDef *htim2)
{
  h->htim1 = htim1;
  h->htim2 = htim2;
  h->count1 = 0;
  h->count2 = 0;

  HAL_TIM_Encoder_Start(htim1, TIM_CHANNEL_ALL);
  HAL_TIM_Encoder_Start(htim2, TIM_CHANNEL_ALL);

  h->prev_cnt1 = __HAL_TIM_GET_COUNTER(htim1);
  h->prev_cnt2 = __HAL_TIM_GET_COUNTER(htim2);
}

void AppEncoder_Task(AppEncoder_HandleTypeDef *h)
{
  update_one(h->htim1, &h->prev_cnt1, &h->count1);
  update_one(h->htim2, &h->prev_cnt2, &h->count2);
}

int32_t AppEncoder_GetCount(const AppEncoder_HandleTypeDef *h, uint8_t axis)
{
  if (axis == 0) return h->count1;
  return h->count2;
}

uint32_t AppEncoder_GetAngleX10000(const AppEncoder_HandleTypeDef *h, uint8_t axis)
{
  if (axis == 0) return count_to_angle(h->count1);
  return count_to_angle(h->count2);
}
