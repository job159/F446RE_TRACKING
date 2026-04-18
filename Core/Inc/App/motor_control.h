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

  /* 軟限位用的位置估算 (microstep 單位)
   * 每個 control cycle 用 hz × dt 累加,精度夠防撞。
   * Axis1 有限位,Axis2 也記錄但不 clamp。 */
  int32_t axis1_position_steps;
  int32_t axis2_position_steps;
} MotorControl_HandleTypeDef;

HAL_StatusTypeDef MotorControl_Init(
    MotorControl_HandleTypeDef *h,
    TIM_HandleTypeDef *htim1, TIM_HandleTypeDef *htim2,
    UART_HandleTypeDef *huart1, UART_HandleTypeDef *huart2);

void MotorControl_StopAll(MotorControl_HandleTypeDef *h);

/* dt_ms: 從上次呼叫到這次經過多少 ms,用來累加位置估算 */
HAL_StatusTypeDef MotorControl_ApplyCommand(
    MotorControl_HandleTypeDef *h, const MotionCommand_t *cmd, uint32_t dt_ms);

HAL_StatusTypeDef MotorControl_SetManualStage(MotorControl_HandleTypeDef *h, uint8_t stage);
void    MotorControl_ClearManualStage(MotorControl_HandleTypeDef *h);
uint8_t MotorControl_IsManualStageValid(const MotorControl_HandleTypeDef *h);
uint8_t MotorControl_GetManualStage(const MotorControl_HandleTypeDef *h);

/* 位置估算相關 */
void    MotorControl_HomePosition(MotorControl_HandleTypeDef *h);     /* 兩軸歸零(=機構在中間) */
int32_t MotorControl_GetAxis1Steps(const MotorControl_HandleTypeDef *h);
int32_t MotorControl_GetAxis2Steps(const MotorControl_HandleTypeDef *h);

/* Manual 模式用:last_cmd 的 hz 持續發 step,這裡用 dt 累加位置估算 */
void    MotorControl_AdvancePosition(MotorControl_HandleTypeDef *h, uint32_t dt_ms);

#ifdef __cplusplus
}
#endif

#endif /* APP_MOTOR_CONTROL_H */
