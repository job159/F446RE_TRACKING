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

typedef struct {
  uint8_t axis1_ifcnt;
  uint8_t axis2_ifcnt;
  uint32_t axis1_gconf;
  uint32_t axis2_gconf;
  uint32_t axis1_ihold_irun;
  uint32_t axis2_ihold_irun;
  uint32_t axis1_chopconf;
  uint32_t axis2_chopconf;
  HAL_StatusTypeDef axis1_ifcnt_status;
  HAL_StatusTypeDef axis2_ifcnt_status;
  HAL_StatusTypeDef axis1_gconf_status;
  HAL_StatusTypeDef axis2_gconf_status;
  HAL_StatusTypeDef axis1_ihold_irun_status;
  HAL_StatusTypeDef axis2_ihold_irun_status;
  HAL_StatusTypeDef axis1_chopconf_status;
  HAL_StatusTypeDef axis2_chopconf_status;
} MotorControl_TmcDebug_t;

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
HAL_StatusTypeDef MotorControl_SetMicrosteps(MotorControl_HandleTypeDef *h, uint16_t microsteps);
uint16_t MotorControl_GetMicrosteps(const MotorControl_HandleTypeDef *h);
HAL_StatusTypeDef MotorControl_SetCurrentConfig(MotorControl_HandleTypeDef *h,
    StepperTmc2209_CurrentConfig_t cfg);
StepperTmc2209_CurrentConfig_t MotorControl_GetCurrentConfig(const MotorControl_HandleTypeDef *h);
HAL_StatusTypeDef MotorControl_ReadTmcDebug(MotorControl_HandleTypeDef *h, MotorControl_TmcDebug_t *out);

#ifdef __cplusplus
}
#endif

#endif /* APP_MOTOR_CONTROL_H */
