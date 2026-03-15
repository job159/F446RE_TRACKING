#include "App/app_main.h"
#include "App/app_adc.h"
#include "App/app_encoder.h"
#include "App/stepper_tmc2209.h"
#include "App/uart_sequence.h"

#define APP_BUTTON_DEBOUNCE_MS 180U
#define APP_LOG_PERIOD_MS 100U
#define APP_TMC2209_MOTOR_1_SLAVE_ADDR 0x00U
#define APP_TMC2209_MOTOR_2_SLAVE_ADDR 0x00U
#define APP_MODE_SLOW_STEP_HZ 200U
#define APP_MODE_FAST_STEP_HZ 1400U
#define APP_MODE_ULTRA_STEP_HZ 5000U
#define APP_MODE_MAX_STEP_HZ 7500U
#define APP_MOTOR_1_DIR_GPIO_PORT GPIOC
#define APP_MOTOR_1_DIR_PIN GPIO_PIN_6
#define APP_MOTOR_1_EN_GPIO_PORT GPIOB
#define APP_MOTOR_1_EN_PIN GPIO_PIN_8
#define APP_MOTOR_2_DIR_GPIO_PORT GPIOC
#define APP_MOTOR_2_DIR_PIN GPIO_PIN_8
#define APP_MOTOR_2_EN_GPIO_PORT GPIOC
#define APP_MOTOR_2_EN_PIN GPIO_PIN_9

static StepperTmc2209_HandleTypeDef g_stepper_1;
static StepperTmc2209_HandleTypeDef g_stepper_2;
static UartSequence_HandleTypeDef g_uart_seq;
static AppAdc_HandleTypeDef g_app_adc;
static AppEncoder_HandleTypeDef g_app_encoder;
static GPIO_PinState g_last_button_state = GPIO_PIN_SET;
static uint32_t g_last_button_tick = 0U;

void AppMain_Init(ADC_HandleTypeDef *hadc1,
                  ADC_HandleTypeDef *hadc2,
                  UART_HandleTypeDef *huart_log,
                  TIM_HandleTypeDef *htim_step_1,
                  TIM_HandleTypeDef *htim_step_2,
                  TIM_HandleTypeDef *htim_enc_1,
                  TIM_HandleTypeDef *htim_enc_2,
                  UART_HandleTypeDef *huart_tmc_1,
                  UART_HandleTypeDef *huart_tmc_2)
{
  static const uint16_t speed_table[STEPPER_TMC2209_SPEED_STAGE_COUNT] = {
      APP_MODE_SLOW_STEP_HZ, APP_MODE_FAST_STEP_HZ, APP_MODE_ULTRA_STEP_HZ, APP_MODE_MAX_STEP_HZ,
      APP_MODE_SLOW_STEP_HZ, APP_MODE_FAST_STEP_HZ, APP_MODE_ULTRA_STEP_HZ, APP_MODE_MAX_STEP_HZ};
  HAL_StatusTypeDef motor_1_status;
  HAL_StatusTypeDef motor_2_status;
  const uint8_t ready_text[] = "APP READY M1+M2\r\n";
  const uint8_t motor_1_error_text[] = "TMC2209 M1 INIT ERROR\r\n";
  const uint8_t motor_2_error_text[] = "TMC2209 M2 INIT ERROR\r\n";

  HAL_GPIO_WritePin(APP_MOTOR_1_DIR_GPIO_PORT, APP_MOTOR_1_DIR_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(APP_MOTOR_2_DIR_GPIO_PORT, APP_MOTOR_2_DIR_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(APP_MOTOR_1_EN_GPIO_PORT, APP_MOTOR_1_EN_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(APP_MOTOR_2_EN_GPIO_PORT, APP_MOTOR_2_EN_PIN, GPIO_PIN_SET);

  motor_1_status = StepperTmc2209_Init(&g_stepper_1,
                                       htim_step_1,
                                       TIM_CHANNEL_1,
                                       huart_tmc_1,
                                       APP_MOTOR_1_DIR_GPIO_PORT,
                                       APP_MOTOR_1_DIR_PIN,
                                       APP_MOTOR_1_EN_GPIO_PORT,
                                       APP_MOTOR_1_EN_PIN,
                                       APP_TMC2209_MOTOR_1_SLAVE_ADDR,
                                       speed_table);
  motor_2_status = StepperTmc2209_Init(&g_stepper_2,
                                       htim_step_2,
                                       TIM_CHANNEL_1,
                                       huart_tmc_2,
                                       APP_MOTOR_2_DIR_GPIO_PORT,
                                       APP_MOTOR_2_DIR_PIN,
                                       APP_MOTOR_2_EN_GPIO_PORT,
                                       APP_MOTOR_2_EN_PIN,
                                       APP_TMC2209_MOTOR_2_SLAVE_ADDR,
                                       speed_table);

  g_last_button_state = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);
  g_last_button_tick = HAL_GetTick();

  AppAdc_Init(&g_app_adc, hadc1, hadc2);
  AppEncoder_Init(&g_app_encoder, htim_enc_1, htim_enc_2);
  UartSequence_Init(&g_uart_seq, huart_log, APP_LOG_PERIOD_MS);

  if ((motor_1_status == HAL_OK) && (motor_2_status == HAL_OK))
  {
    (void)HAL_UART_Transmit(huart_log, (uint8_t *)ready_text, (uint16_t)(sizeof(ready_text) - 1U), 50U);
  }
  else
  {
    if (motor_1_status != HAL_OK)
    {
      (void)HAL_UART_Transmit(huart_log, (uint8_t *)motor_1_error_text, (uint16_t)(sizeof(motor_1_error_text) - 1U), 50U);
    }

    if (motor_2_status != HAL_OK)
    {
      (void)HAL_UART_Transmit(huart_log, (uint8_t *)motor_2_error_text, (uint16_t)(sizeof(motor_2_error_text) - 1U), 50U);
    }
  }
}

void AppMain_Task(void)
{
  GPIO_PinState current_button_state;
  uint32_t now_tick_ms;
  HAL_StatusTypeDef step_update_status;
  uint8_t next_stage;

  now_tick_ms = HAL_GetTick();
  current_button_state = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);

  if ((g_last_button_state == GPIO_PIN_SET) && (current_button_state == GPIO_PIN_RESET))
  {
    if ((now_tick_ms - g_last_button_tick) >= APP_BUTTON_DEBOUNCE_MS)
    {
      g_last_button_tick = now_tick_ms;
      step_update_status = StepperTmc2209_NextSpeedStage(&g_stepper_1);
      if (step_update_status == HAL_OK)
      {
        next_stage = StepperTmc2209_GetSpeedStage(&g_stepper_1);
        (void)StepperTmc2209_SetSpeedStage(&g_stepper_2, next_stage);
      }
    }
  }

  g_last_button_state = current_button_state;
  AppAdc_Task(&g_app_adc);
  AppEncoder_Task(&g_app_encoder);
  UartSequence_Task(
      &g_uart_seq,
      AppAdc_GetFilteredAdc1(&g_app_adc),
      AppAdc_GetFilteredAdc2(&g_app_adc),
      AppAdc_GetFilteredAdc3(&g_app_adc),
      AppAdc_GetFilteredAdc4(&g_app_adc),
      AppEncoder_GetCount1(&g_app_encoder),
      AppEncoder_GetCount2(&g_app_encoder),
      AppEncoder_GetAngle1X10000(&g_app_encoder),
      AppEncoder_GetAngle2X10000(&g_app_encoder),
      StepperTmc2209_GetSpeedStage(&g_stepper_1),
      StepperTmc2209_GetSpeedStage(&g_stepper_2));
}
