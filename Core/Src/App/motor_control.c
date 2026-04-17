#include "App/motor_control.h"

/* =========================================================================
 *  Manual 速度表 (兩邊各 7 段,共 14 段)
 *    F1..F7 (正轉) | R1..R7 (反轉)
 *    要調速度直接改下面這個陣列
 * ========================================================================= */
static const uint16_t MANUAL_SPEED_TABLE[TMC_SPEED_STAGE_COUNT] = {
  50, 100, 200, 400, 600, 1000, 5000,   /* F1..F7 */
  50, 100, 200, 400, 600, 1000, 5000    /* R1..R7 */
};

/* =========================================================================
 *  GPIO 腳位
 * ========================================================================= */
#define AXIS1_DIR_PORT   GPIOC
#define AXIS1_DIR_PIN    GPIO_PIN_6
#define AXIS1_EN_PORT    GPIOB
#define AXIS1_EN_PIN     GPIO_PIN_8

#define AXIS2_DIR_PORT   GPIOC
#define AXIS2_DIR_PIN    GPIO_PIN_8
#define AXIS2_EN_PORT    GPIOC
#define AXIS2_EN_PIN     GPIO_PIN_9

/* stage 轉成帶正負號 hz (正轉 +,反轉 -) */
static int32_t stage_to_signed(uint8_t stage)
{
  if (stage >= TMC_SPEED_STAGE_COUNT) return 0;
  int32_t hz = (int32_t)MANUAL_SPEED_TABLE[stage];
  return (stage >= TMC_DIR_SPLIT_STAGE) ? -hz : hz;
}

HAL_StatusTypeDef MotorControl_Init(
    MotorControl_HandleTypeDef *h,
    TIM_HandleTypeDef *htim1, TIM_HandleTypeDef *htim2,
    UART_HandleTypeDef *huart1, UART_HandleTypeDef *huart2)
{
  h->last_cmd.axis1_step_hz = 0;
  h->last_cmd.axis2_step_hz = 0;
  h->manual_stage = 0;
  h->manual_stage_valid = 0;

  HAL_StatusTypeDef s1 = StepperTmc2209_Init(&h->axis1,
      htim1, TIM_CHANNEL_1, huart1,
      AXIS1_DIR_PORT, AXIS1_DIR_PIN, AXIS1_EN_PORT, AXIS1_EN_PIN,
      0x00, MANUAL_SPEED_TABLE);

  HAL_StatusTypeDef s2 = StepperTmc2209_Init(&h->axis2,
      htim2, TIM_CHANNEL_1, huart2,
      AXIS2_DIR_PORT, AXIS2_DIR_PIN, AXIS2_EN_PORT, AXIS2_EN_PIN,
      0x00, MANUAL_SPEED_TABLE);

  MotorControl_StopAll(h);
  return (s1 == HAL_OK && s2 == HAL_OK) ? HAL_OK : HAL_ERROR;
}

void MotorControl_StopAll(MotorControl_HandleTypeDef *h)
{
  StepperTmc2209_Stop(&h->axis1);
  StepperTmc2209_Stop(&h->axis2);
  h->last_cmd.axis1_step_hz = 0;
  h->last_cmd.axis2_step_hz = 0;
}

HAL_StatusTypeDef MotorControl_ApplyCommand(MotorControl_HandleTypeDef *h, const MotionCommand_t *cmd)
{
  HAL_StatusTypeDef s1 = StepperTmc2209_SetSignedHz(&h->axis1, cmd->axis1_step_hz);
  HAL_StatusTypeDef s2 = StepperTmc2209_SetSignedHz(&h->axis2, cmd->axis2_step_hz);
  if (s1 == HAL_OK && s2 == HAL_OK)
  {
    h->last_cmd = *cmd;
    return HAL_OK;
  }
  return HAL_ERROR;
}

HAL_StatusTypeDef MotorControl_SetManualStage(MotorControl_HandleTypeDef *h, uint8_t stage)
{
  if (stage >= TMC_SPEED_STAGE_COUNT) return HAL_ERROR;

  HAL_StatusTypeDef s1 = StepperTmc2209_SetSpeedStage(&h->axis1, stage);
  HAL_StatusTypeDef s2 = StepperTmc2209_SetSpeedStage(&h->axis2, stage);
  if (s1 == HAL_OK && s2 == HAL_OK)
  {
    h->manual_stage = stage;
    h->manual_stage_valid = 1;
    h->last_cmd.axis1_step_hz = stage_to_signed(stage);
    h->last_cmd.axis2_step_hz = stage_to_signed(stage);
    return HAL_OK;
  }
  return HAL_ERROR;
}

void MotorControl_ClearManualStage(MotorControl_HandleTypeDef *h)
{
  h->manual_stage = 0;
  h->manual_stage_valid = 0;
}

uint8_t MotorControl_IsManualStageValid(const MotorControl_HandleTypeDef *h)
{
  return h->manual_stage_valid;
}

uint8_t MotorControl_GetManualStage(const MotorControl_HandleTypeDef *h)
{
  return h->manual_stage;
}
