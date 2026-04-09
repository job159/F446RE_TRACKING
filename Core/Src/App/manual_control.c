#include "App/manual_control.h"
#include <string.h>

void ManualControl_Init(ManualControl_HandleTypeDef *h)
{
  memset(h, 0, sizeof(*h));
}

void ManualControl_Reset(ManualControl_HandleTypeDef *h)
{
  memset(h, 0, sizeof(*h));
}

void ManualControl_SetStage(ManualControl_HandleTypeDef *h, uint8_t stage)
{
  h->pending_stage = stage;
  h->pending_valid = 1;
}

void ManualControl_Task(ManualControl_HandleTypeDef *h, MotorControl_HandleTypeDef *motor)
{
  if (!h->pending_valid) return;

  if (MotorControl_SetManualStage(motor, h->pending_stage) == HAL_OK)
  {
    h->active_stage = h->pending_stage;
    h->active_valid = 1;
  }
  h->pending_valid = 0;
}

uint8_t ManualControl_IsStageValid(const ManualControl_HandleTypeDef *h)
{
  return h->active_valid;
}

uint8_t ManualControl_GetStage(const ManualControl_HandleTypeDef *h)
{
  return h->active_stage;
}
