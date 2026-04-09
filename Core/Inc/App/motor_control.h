#ifndef APP_MOTOR_CONTROL_H
#define APP_MOTOR_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "App/stepper_tmc2209.h"
#include "App/tracking_types.h"

typedef struct {
  StepperTmc2209_HandleTypeDef axis1;
  StepperTmc2209_HandleTypeDef axis2;
  MotionCommand_t last_cmd;
  uint8_t manual_stage;
  uint8_t manual_stage_valid;
} MotorControl_HandleTypeDef;

HAL_StatusTypeDef MotorControl_Init(
    MotorControl_HandleTypeDef *h,
    TIM_HandleTypeDef *htim1, TIM_HandleTypeDef *htim2,
    UART_HandleTypeDef *huart1, UART_HandleTypeDef *huart2);

void MotorControl_StopAll(MotorControl_HandleTypeDef *h);

HAL_StatusTypeDef MotorControl_ApplyCommand(
    MotorControl_HandleTypeDef *h, const MotionCommand_t *cmd);

HAL_StatusTypeDef MotorControl_SetManualStage(MotorControl_HandleTypeDef *h, uint8_t stage);
void    MotorControl_ClearManualStage(MotorControl_HandleTypeDef *h);
uint8_t MotorControl_IsManualStageValid(const MotorControl_HandleTypeDef *h);
uint8_t MotorControl_GetManualStage(const MotorControl_HandleTypeDef *h);

#ifdef __cplusplus
}
#endif

#endif /* APP_MOTOR_CONTROL_H */
