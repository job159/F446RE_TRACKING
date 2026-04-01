#include "App/motor_control.h"

#define MOTOR_CTRL_TMC2209_AXIS1_ADDR    0x00U
#define MOTOR_CTRL_TMC2209_AXIS2_ADDR    0x00U
#define MOTOR_CTRL_AXIS1_DIR_GPIO_PORT   GPIOC
#define MOTOR_CTRL_AXIS1_DIR_PIN         GPIO_PIN_6
#define MOTOR_CTRL_AXIS1_EN_GPIO_PORT    GPIOB
#define MOTOR_CTRL_AXIS1_EN_PIN          GPIO_PIN_8
#define MOTOR_CTRL_AXIS2_DIR_GPIO_PORT   GPIOC
#define MOTOR_CTRL_AXIS2_DIR_PIN         GPIO_PIN_8
#define MOTOR_CTRL_AXIS2_EN_GPIO_PORT    GPIOC
#define MOTOR_CTRL_AXIS2_EN_PIN          GPIO_PIN_9

static HAL_StatusTypeDef MotorControl_ApplySingle(
    StepperTmc2209_HandleTypeDef *stepper,
    int32_t signed_hz)
{
  return StepperTmc2209_SetSignedStepRate(stepper, signed_hz);
}

static int32_t MotorControl_StageToSignedHz(
    const StepperTmc2209_HandleTypeDef *stepper,
    uint8_t stage)
{
  int32_t signed_hz;

  if ((stepper == NULL) || (stage >= STEPPER_TMC2209_SPEED_STAGE_COUNT))
  {
    return 0;
  }

  signed_hz = (int32_t)stepper->speed_hz[stage];
  if (stage >= STEPPER_TMC2209_DIRECTION_SPLIT_STAGE)
  {
    signed_hz = -signed_hz;
  }

  return signed_hz;
}

HAL_StatusTypeDef MotorControl_Init(
    MotorControl_HandleTypeDef *handle,
    TIM_HandleTypeDef *htim_step_1,
    TIM_HandleTypeDef *htim_step_2,
    UART_HandleTypeDef *huart_tmc_1,
    UART_HandleTypeDef *huart_tmc_2)
{
  static const uint16_t speed_table[STEPPER_TMC2209_SPEED_STAGE_COUNT] = {
      200U, 1400U, 5000U, 7500U,
      200U, 1400U, 5000U, 7500U};
  HAL_StatusTypeDef axis1_status;
  HAL_StatusTypeDef axis2_status;

  if (handle == NULL)
  {
    return HAL_ERROR;
  }

  handle->last_motion_command.axis1_step_hz = 0;
  handle->last_motion_command.axis2_step_hz = 0;
  handle->manual_stage = 0U;
  handle->manual_stage_valid = 0U;

  axis1_status = StepperTmc2209_Init(&handle->axis1,
                                     htim_step_1,
                                     TIM_CHANNEL_1,
                                     huart_tmc_1,
                                     MOTOR_CTRL_AXIS1_DIR_GPIO_PORT,
                                     MOTOR_CTRL_AXIS1_DIR_PIN,
                                     MOTOR_CTRL_AXIS1_EN_GPIO_PORT,
                                     MOTOR_CTRL_AXIS1_EN_PIN,
                                     MOTOR_CTRL_TMC2209_AXIS1_ADDR,
                                     speed_table);
  axis2_status = StepperTmc2209_Init(&handle->axis2,
                                     htim_step_2,
                                     TIM_CHANNEL_1,
                                     huart_tmc_2,
                                     MOTOR_CTRL_AXIS2_DIR_GPIO_PORT,
                                     MOTOR_CTRL_AXIS2_DIR_PIN,
                                     MOTOR_CTRL_AXIS2_EN_GPIO_PORT,
                                     MOTOR_CTRL_AXIS2_EN_PIN,
                                     MOTOR_CTRL_TMC2209_AXIS2_ADDR,
                                     speed_table);

  MotorControl_StopAll(handle);

  if ((axis1_status != HAL_OK) || (axis2_status != HAL_OK))
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

void MotorControl_StopAll(MotorControl_HandleTypeDef *handle)
{
  if (handle == NULL)
  {
    return;
  }

  (void)StepperTmc2209_Stop(&handle->axis1);
  (void)StepperTmc2209_Stop(&handle->axis2);
  handle->last_motion_command.axis1_step_hz = 0;
  handle->last_motion_command.axis2_step_hz = 0;
}

HAL_StatusTypeDef MotorControl_ApplySignedStepHz(
    MotorControl_HandleTypeDef *handle,
    const MotionCommand_t *command)
{
  HAL_StatusTypeDef axis1_status;
  HAL_StatusTypeDef axis2_status;

  if ((handle == NULL) || (command == NULL))
  {
    return HAL_ERROR;
  }

  axis1_status = MotorControl_ApplySingle(&handle->axis1, command->axis1_step_hz);
  axis2_status = MotorControl_ApplySingle(&handle->axis2, command->axis2_step_hz);

  if ((axis1_status == HAL_OK) && (axis2_status == HAL_OK))
  {
    handle->last_motion_command = *command;
    return HAL_OK;
  }

  return HAL_ERROR;
}

HAL_StatusTypeDef MotorControl_SetManualStage(
    MotorControl_HandleTypeDef *handle,
    uint8_t stage)
{
  HAL_StatusTypeDef axis1_status;
  HAL_StatusTypeDef axis2_status;

  if ((handle == NULL) || (stage >= STEPPER_TMC2209_SPEED_STAGE_COUNT))
  {
    return HAL_ERROR;
  }

  axis1_status = StepperTmc2209_SetSpeedStage(&handle->axis1, stage);
  axis2_status = StepperTmc2209_SetSpeedStage(&handle->axis2, stage);

  if ((axis1_status == HAL_OK) && (axis2_status == HAL_OK))
  {
    handle->manual_stage = stage;
    handle->manual_stage_valid = 1U;
    handle->last_motion_command.axis1_step_hz = MotorControl_StageToSignedHz(&handle->axis1, stage);
    handle->last_motion_command.axis2_step_hz = MotorControl_StageToSignedHz(&handle->axis2, stage);
    return HAL_OK;
  }

  return HAL_ERROR;
}

void MotorControl_ClearManualStage(MotorControl_HandleTypeDef *handle)
{
  if (handle == NULL)
  {
    return;
  }

  handle->manual_stage = 0U;
  handle->manual_stage_valid = 0U;
}

uint8_t MotorControl_IsManualStageValid(const MotorControl_HandleTypeDef *handle)
{
  if (handle == NULL)
  {
    return 0U;
  }

  return handle->manual_stage_valid;
}

uint8_t MotorControl_GetManualStage(const MotorControl_HandleTypeDef *handle)
{
  if (handle == NULL)
  {
    return 0U;
  }

  return handle->manual_stage;
}
