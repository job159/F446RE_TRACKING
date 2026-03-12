#include "App/app_encoder.h"

static int32_t AppEncoder_NormalizeCountToOneTurn(int32_t count)
{
  int32_t normalized_count;

  normalized_count = count % APP_ENCODER_COUNTS_PER_REV;
  if (normalized_count < 0)
  {
    normalized_count += APP_ENCODER_COUNTS_PER_REV;
  }

  return normalized_count;
}

static uint32_t AppEncoder_ConvertCountToAngleX10000(int32_t count)
{
  int32_t normalized_count;
  uint64_t scaled_angle;
  uint32_t rounded_angle;
  const uint32_t full_turn_angle_x10000 = 360U * APP_ENCODER_ANGLE_SCALE;

  normalized_count = AppEncoder_NormalizeCountToOneTurn(count);
  scaled_angle = (uint64_t)(uint32_t)normalized_count * (uint64_t)full_turn_angle_x10000;
  rounded_angle = (uint32_t)((scaled_angle + ((uint64_t)APP_ENCODER_COUNTS_PER_REV / 2ULL)) /
                             (uint64_t)APP_ENCODER_COUNTS_PER_REV);

  if (rounded_angle >= full_turn_angle_x10000)
  {
    rounded_angle = 0U;
  }

  return rounded_angle;
}

static int32_t AppEncoder_CalcDelta(uint32_t current_counter, uint32_t previous_counter)
{
  return (int32_t)(current_counter - previous_counter);
}

static void AppEncoder_UpdateSingle(
    TIM_HandleTypeDef *htim,
    uint32_t *last_counter,
    int32_t *accumulated_count)
{
  uint32_t current_counter;
  int32_t delta;

  current_counter = __HAL_TIM_GET_COUNTER(htim);
  delta = AppEncoder_CalcDelta(current_counter, *last_counter);
  *accumulated_count += delta;
  *last_counter = current_counter;
}

void AppEncoder_Init(
    AppEncoder_HandleTypeDef *handle,
    TIM_HandleTypeDef *htim_enc1,
    TIM_HandleTypeDef *htim_enc2)
{
  if ((handle == NULL) || (htim_enc1 == NULL) || (htim_enc2 == NULL))
  {
    return;
  }

  handle->htim_enc1 = htim_enc1;
  handle->htim_enc2 = htim_enc2;
  handle->count_enc1 = 0;
  handle->count_enc2 = 0;

  if (HAL_TIM_Encoder_Start(handle->htim_enc1, TIM_CHANNEL_ALL) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_TIM_Encoder_Start(handle->htim_enc2, TIM_CHANNEL_ALL) != HAL_OK)
  {
    Error_Handler();
  }

  handle->last_counter_enc1 = __HAL_TIM_GET_COUNTER(handle->htim_enc1);
  handle->last_counter_enc2 = __HAL_TIM_GET_COUNTER(handle->htim_enc2);
}

void AppEncoder_Task(AppEncoder_HandleTypeDef *handle)
{
  if ((handle == NULL) || (handle->htim_enc1 == NULL) || (handle->htim_enc2 == NULL))
  {
    return;
  }

  AppEncoder_UpdateSingle(
      handle->htim_enc1,
      &handle->last_counter_enc1,
      &handle->count_enc1);
  AppEncoder_UpdateSingle(
      handle->htim_enc2,
      &handle->last_counter_enc2,
      &handle->count_enc2);
}

int32_t AppEncoder_GetCount1(const AppEncoder_HandleTypeDef *handle)
{
  if (handle == NULL)
  {
    return 0;
  }

  return handle->count_enc1;
}

int32_t AppEncoder_GetCount2(const AppEncoder_HandleTypeDef *handle)
{
  if (handle == NULL)
  {
    return 0;
  }

  return handle->count_enc2;
}

uint32_t AppEncoder_GetAngle1X10000(const AppEncoder_HandleTypeDef *handle)
{
  if (handle == NULL)
  {
    return 0U;
  }

  return AppEncoder_ConvertCountToAngleX10000(handle->count_enc1);
}

uint32_t AppEncoder_GetAngle2X10000(const AppEncoder_HandleTypeDef *handle)
{
  if (handle == NULL)
  {
    return 0U;
  }

  return AppEncoder_ConvertCountToAngleX10000(handle->count_enc2);
}
