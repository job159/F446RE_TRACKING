#include "App/motor_control.h"

/* GPIO腳位定義 - 對應PCB layout */
#define AXIS1_DIR_PORT   GPIOC
#define AXIS1_DIR_PIN    GPIO_PIN_6
#define AXIS1_EN_PORT    GPIOB
#define AXIS1_EN_PIN     GPIO_PIN_8
#define AXIS2_DIR_PORT   GPIOC
#define AXIS2_DIR_PIN    GPIO_PIN_8
#define AXIS2_EN_PORT    GPIOC
#define AXIS2_EN_PIN     GPIO_PIN_9

/* stage轉成帶正負號的step hz */
static int32_t stage_to_signed(const StepperTmc2209_HandleTypeDef *s, uint8_t stage)
{
  if (stage >= TMC_SPEED_STAGE_COUNT) return 0;
  int32_t hz = (int32_t)s->speed_hz[stage];
  if (stage >= TMC_DIR_SPLIT_STAGE) hz = -hz;
  return hz;
}

HAL_StatusTypeDef MotorControl_Init(
    MotorControl_HandleTypeDef *h,
    TIM_HandleTypeDef *htim1, TIM_HandleTypeDef *htim2,
    UART_HandleTypeDef *huart1, UART_HandleTypeDef *huart2)
{
  static const uint16_t speed_table[TMC_SPEED_STAGE_COUNT] = {
    200, 1400, 5000, 7500,
    200, 1400, 5000, 7500
  };

  h->last_cmd.axis1_step_hz = 0;
  h->last_cmd.axis2_step_hz = 0;
  h->manual_stage = 0;
  h->manual_stage_valid = 0;

  HAL_StatusTypeDef s1 = StepperTmc2209_Init(&h->axis1,
      htim1, TIM_CHANNEL_1, huart1,
      AXIS1_DIR_PORT, AXIS1_DIR_PIN,
      AXIS1_EN_PORT,  AXIS1_EN_PIN,
      0x00, speed_table);

  HAL_StatusTypeDef s2 = StepperTmc2209_Init(&h->axis2,
      htim2, TIM_CHANNEL_1, huart2,
      AXIS2_DIR_PORT, AXIS2_DIR_PIN,
      AXIS2_EN_PORT,  AXIS2_EN_PIN,
      0x00, speed_table);

  MotorControl_StopAll(h);

  if (s1 != HAL_OK || s2 != HAL_OK) return HAL_ERROR;
  return HAL_OK;
}

void MotorControl_StopAll(MotorControl_HandleTypeDef *h)
{
  StepperTmc2209_Stop(&h->axis1);
  StepperTmc2209_Stop(&h->axis2);
  h->last_cmd.axis1_step_hz = 0;
  h->last_cmd.axis2_step_hz = 0;
}

HAL_StatusTypeDef MotorControl_ApplyCommand(
    MotorControl_HandleTypeDef *h, const MotionCommand_t *cmd)
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
    h->last_cmd.axis1_step_hz = stage_to_signed(&h->axis1, stage);
    h->last_cmd.axis2_step_hz = stage_to_signed(&h->axis2, stage);
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

HAL_StatusTypeDef MotorControl_SetMicrosteps(MotorControl_HandleTypeDef *h, uint16_t microsteps)
{
  MotorControl_StopAll(h);

  HAL_StatusTypeDef s1 = StepperTmc2209_SetMicrosteps(&h->axis1, microsteps);
  HAL_StatusTypeDef s2 = StepperTmc2209_SetMicrosteps(&h->axis2, microsteps);

  if (s1 == HAL_OK && s2 == HAL_OK) return HAL_OK;
  return HAL_ERROR;
}

uint16_t MotorControl_GetMicrosteps(const MotorControl_HandleTypeDef *h)
{
  return StepperTmc2209_GetMicrosteps(&h->axis1);
}

HAL_StatusTypeDef MotorControl_SetCurrentConfig(MotorControl_HandleTypeDef *h,
    StepperTmc2209_CurrentConfig_t cfg)
{
  HAL_StatusTypeDef s1 = StepperTmc2209_SetCurrentConfig(&h->axis1, cfg);
  HAL_StatusTypeDef s2 = StepperTmc2209_SetCurrentConfig(&h->axis2, cfg);

  if (s1 == HAL_OK && s2 == HAL_OK) return HAL_OK;
  return HAL_ERROR;
}

StepperTmc2209_CurrentConfig_t MotorControl_GetCurrentConfig(const MotorControl_HandleTypeDef *h)
{
  return StepperTmc2209_GetCurrentConfig(&h->axis1);
}

HAL_StatusTypeDef MotorControl_ReadTmcDebug(MotorControl_HandleTypeDef *h, MotorControl_TmcDebug_t *out)
{
  if (h == NULL || out == NULL) return HAL_ERROR;

  out->axis1_ifcnt = 0;
  out->axis2_ifcnt = 0;
  out->axis1_gconf = 0;
  out->axis2_gconf = 0;
  out->axis1_ihold_irun = 0;
  out->axis2_ihold_irun = 0;
  out->axis1_chopconf = 0;
  out->axis2_chopconf = 0;

  out->axis1_ifcnt_status = StepperTmc2209_ReadIfcnt(&h->axis1, &out->axis1_ifcnt);
  out->axis1_gconf_status = StepperTmc2209_ReadGconf(&h->axis1, &out->axis1_gconf);
  out->axis1_ihold_irun_status = StepperTmc2209_ReadIholdIrun(&h->axis1, &out->axis1_ihold_irun);
  out->axis1_chopconf_status = StepperTmc2209_ReadChopconf(&h->axis1, &out->axis1_chopconf);
  out->axis2_ifcnt_status = StepperTmc2209_ReadIfcnt(&h->axis2, &out->axis2_ifcnt);
  out->axis2_gconf_status = StepperTmc2209_ReadGconf(&h->axis2, &out->axis2_gconf);
  out->axis2_ihold_irun_status = StepperTmc2209_ReadIholdIrun(&h->axis2, &out->axis2_ihold_irun);
  out->axis2_chopconf_status = StepperTmc2209_ReadChopconf(&h->axis2, &out->axis2_chopconf);

  if (out->axis1_ifcnt_status == HAL_OK &&
      out->axis1_gconf_status == HAL_OK &&
      out->axis1_ihold_irun_status == HAL_OK &&
      out->axis1_chopconf_status == HAL_OK &&
      out->axis2_ifcnt_status == HAL_OK &&
      out->axis2_gconf_status == HAL_OK &&
      out->axis2_ihold_irun_status == HAL_OK &&
      out->axis2_chopconf_status == HAL_OK)
    return HAL_OK;

  return HAL_ERROR;
}
