#ifndef APP_MOTOR_CONTROL_H
#define APP_MOTOR_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "App/stepper_tmc2209.h"
#include "App/tracking_types.h"

typedef struct
{
  StepperTmc2209_HandleTypeDef axis1;
  StepperTmc2209_HandleTypeDef axis2;
  MotionCommand_t last_motion_command;
  uint8_t manual_stage;
  uint8_t manual_stage_valid;
} MotorControl_HandleTypeDef;

HAL_StatusTypeDef MotorControl_Init(
    MotorControl_HandleTypeDef *handle,
    TIM_HandleTypeDef *htim_step_1,
    TIM_HandleTypeDef *htim_step_2,
    UART_HandleTypeDef *huart_tmc_1,
    UART_HandleTypeDef *huart_tmc_2);

void MotorControl_StopAll(MotorControl_HandleTypeDef *handle);

HAL_StatusTypeDef MotorControl_ApplySignedStepHz(
    MotorControl_HandleTypeDef *handle,
    const MotionCommand_t *command);

HAL_StatusTypeDef MotorControl_SetManualStage(
    MotorControl_HandleTypeDef *handle,
    uint8_t stage);

void MotorControl_ClearManualStage(MotorControl_HandleTypeDef *handle);

uint8_t MotorControl_IsManualStageValid(const MotorControl_HandleTypeDef *handle);
uint8_t MotorControl_GetManualStage(const MotorControl_HandleTypeDef *handle);

#ifdef __cplusplus
}
#endif

#endif /* APP_MOTOR_CONTROL_H */
