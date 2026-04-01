#include "App/manual_control.h"

#include <string.h>

void ManualControl_Init(ManualControl_HandleTypeDef *handle)
{
  if (handle == NULL)
  {
    return;
  }

  (void)memset(handle, 0, sizeof(*handle));
}

void ManualControl_Reset(ManualControl_HandleTypeDef *handle)
{
  ManualControl_Init(handle);
}

void ManualControl_SetStage(
    ManualControl_HandleTypeDef *handle,
    uint8_t stage)
{
  if (handle == NULL)
  {
    return;
  }

  handle->pending_stage = stage;
  handle->pending_valid = 1U;
}

void ManualControl_Task(
    ManualControl_HandleTypeDef *handle,
    MotorControl_HandleTypeDef *motor)
{
  if ((handle == NULL) || (motor == NULL) || (handle->pending_valid == 0U))
  {
    return;
  }

  if (MotorControl_SetManualStage(motor, handle->pending_stage) == HAL_OK)
  {
    handle->active_stage = handle->pending_stage;
    handle->active_valid = 1U;
  }

  handle->pending_valid = 0U;
}

uint8_t ManualControl_IsStageValid(const ManualControl_HandleTypeDef *handle)
{
  if (handle == NULL)
  {
    return 0U;
  }

  return handle->active_valid;
}

uint8_t ManualControl_GetStage(const ManualControl_HandleTypeDef *handle)
{
  if (handle == NULL)
  {
    return 0U;
  }

  return handle->active_stage;
}
