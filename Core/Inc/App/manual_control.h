#ifndef APP_MANUAL_CONTROL_H
#define APP_MANUAL_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "App/motor_control.h"

typedef struct {
  uint8_t pending_stage;
  uint8_t pending_valid;
  uint8_t active_stage;
  uint8_t active_valid;
} ManualControl_HandleTypeDef;

void    ManualControl_Init(ManualControl_HandleTypeDef *h);
void    ManualControl_Reset(ManualControl_HandleTypeDef *h);
void    ManualControl_SetStage(ManualControl_HandleTypeDef *h, uint8_t stage);
void    ManualControl_Task(ManualControl_HandleTypeDef *h, MotorControl_HandleTypeDef *motor);
uint8_t ManualControl_IsStageValid(const ManualControl_HandleTypeDef *h);
uint8_t ManualControl_GetStage(const ManualControl_HandleTypeDef *h);

#ifdef __cplusplus
}
#endif

#endif /* APP_MANUAL_CONTROL_H */
