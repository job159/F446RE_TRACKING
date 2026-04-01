#include "App/app_main.h"

#include "App/app_adc.h"
#include "App/app_encoder.h"
#include "App/ldr_tracking.h"
#include "App/manual_control.h"
#include "App/motor_control.h"
#include "App/serial_cmd.h"
#include "App/telemetry.h"
#include "App/tracker_controller.h"
#include "App/tracking_config.h"
#include "App/tracking_types.h"

#include <stdio.h>
#include <string.h>

#define APP_MAIN_TEXT_BUFFER_SIZE 192U
#define APP_MAIN_BUTTON_DEBOUNCE_MS 180U

typedef struct
{
  AppAdc_HandleTypeDef adc;
  AppEncoder_HandleTypeDef encoder;
  SerialCmd_HandleTypeDef serial;
  LdrTracking_HandleTypeDef ldr;
  TrackerController_HandleTypeDef tracker;
  MotorControl_HandleTypeDef motor;
  ManualControl_HandleTypeDef manual;
  Telemetry_HandleTypeDef telemetry;
  TelemetrySnapshot_t snapshot;
  SystemMode_t mode;
  SystemMode_t requested_mode_after_calibration;
  IdleSubstate_t idle_substate;
  uint32_t calibration_start_tick_ms;
  uint32_t control_period_ms;
  uint32_t last_control_tick_ms;
  uint32_t last_button_tick_ms;
  uint8_t requested_manual_stage;
  uint8_t requested_manual_stage_valid;
  GPIO_PinState last_button_state;
} AppMain_Context_t;

static AppMain_Context_t g_app;

static int32_t AppMain_FloatToX1000(float value)
{
  if (value >= 0.0f)
  {
    return (int32_t)((value * 1000.0f) + 0.5f);
  }

  return (int32_t)((value * 1000.0f) - 0.5f);
}

static const char *AppMain_ModeToText(SystemMode_t mode)
{
  switch (mode)
  {
    case MODE_IDLE:
      return "IDLE";
    case MODE_TRACKING:
      return "TRACK";
    case MODE_SEARCH:
      return "SEARCH";
    case MODE_MANUAL:
      return "MANUAL";
    default:
      return "UNKNOWN";
  }
}

static const char *AppMain_IdleToText(
    SystemMode_t mode,
    IdleSubstate_t idle_substate)
{
  if (mode != MODE_IDLE)
  {
    return "-";
  }

  return (idle_substate == IDLE_CALIBRATING) ? "CAL" : "WAIT";
}

static void AppMain_SendLine(const char *text)
{
  Telemetry_SendLine(&g_app.telemetry, text);
}

static void AppMain_SendManualStageNotice(
    uint8_t stage,
    const char *state_text)
{
  char text[APP_MAIN_TEXT_BUFFER_SIZE];
  const char *prefix;
  uint32_t stage_index;

  prefix = (stage < 4U) ? "F" : "R";
  stage_index = (uint32_t)(stage % 4U) + 1U;

  (void)snprintf(text,
                 sizeof(text),
                 "MAN %s%lu %s\r\n",
                 prefix,
                 (unsigned long)stage_index,
                 (state_text != NULL) ? state_text : "ACTIVE");
  AppMain_SendLine(text);
}

static uint8_t AppMain_IsControlPeriodSupported(uint32_t control_period_ms)
{
  return (uint8_t)((control_period_ms == 1U) ||
                   (control_period_ms == 2U) ||
                   (control_period_ms == 5U));
}

static void AppMain_SetControlPeriod(uint32_t control_period_ms)
{
  char text[APP_MAIN_TEXT_BUFFER_SIZE];

  if (AppMain_IsControlPeriodSupported(control_period_ms) == 0U)
  {
    AppMain_SendLine("ERR period_ms only 1|2|5\r\n");
    return;
  }

  g_app.control_period_ms = control_period_ms;
  (void)snprintf(text,
                 sizeof(text),
                 "PERIOD %luMS OK\r\n",
                 (unsigned long)g_app.control_period_ms);
  AppMain_SendLine(text);
}

static void AppMain_EnterIdleWait(void)
{
  MotorControl_StopAll(&g_app.motor);
  g_app.requested_manual_stage_valid = 0U;
  g_app.mode = MODE_IDLE;
  g_app.idle_substate = IDLE_WAIT_CMD;
}

static void AppMain_StartCalibration(uint32_t now_ms)
{
  MotorControl_StopAll(&g_app.motor);
  TrackerController_Reset(&g_app.tracker);
  ManualControl_Reset(&g_app.manual);
  MotorControl_ClearManualStage(&g_app.motor);
  LdrTracking_ForceRecalibration(&g_app.ldr);

  g_app.mode = MODE_IDLE;
  g_app.idle_substate = IDLE_CALIBRATING;
  g_app.requested_mode_after_calibration = MODE_TRACKING;
  g_app.requested_manual_stage_valid = 0U;
  g_app.calibration_start_tick_ms = now_ms;
}

static void AppMain_EnterTracking(void)
{
  if (g_app.ldr.frame.calibration_done == 0U)
  {
    g_app.requested_mode_after_calibration = MODE_TRACKING;
    return;
  }

  MotorControl_StopAll(&g_app.motor);
  MotorControl_ClearManualStage(&g_app.motor);
  TrackerController_Reset(&g_app.tracker);
  g_app.requested_manual_stage_valid = 0U;
  g_app.mode = MODE_TRACKING;
}

static void AppMain_EnterManual(void)
{
  if (g_app.ldr.frame.calibration_done == 0U)
  {
    g_app.requested_mode_after_calibration = MODE_MANUAL;
    return;
  }

  MotorControl_StopAll(&g_app.motor);
  TrackerController_Reset(&g_app.tracker);
  ManualControl_Reset(&g_app.manual);
  MotorControl_ClearManualStage(&g_app.motor);
  g_app.mode = MODE_MANUAL;

  if (g_app.requested_manual_stage_valid != 0U)
  {
    ManualControl_SetStage(&g_app.manual, g_app.requested_manual_stage);
    g_app.requested_manual_stage_valid = 0U;
  }
}

static void AppMain_FinalizeCalibration(void)
{
  LdrTracking_FinalizeCalibration(&g_app.ldr);

  if (g_app.requested_mode_after_calibration == MODE_TRACKING)
  {
    g_app.requested_mode_after_calibration = MODE_IDLE;
    AppMain_EnterTracking();
  }
  else if (g_app.requested_mode_after_calibration == MODE_MANUAL)
  {
    g_app.requested_mode_after_calibration = MODE_IDLE;
    AppMain_EnterManual();
  }
  else
  {
    AppMain_EnterIdleWait();
  }
}

static void AppMain_SendStatusSummary(void)
{
  char text[APP_MAIN_TEXT_BUFFER_SIZE];

  (void)snprintf(text,
                 sizeof(text),
                 "STATUS mode:%s idle:%s cal:%u valid:%u total:%u contrast:%u cmd:%ld,%ld\r\n",
                 AppMain_ModeToText(g_app.mode),
                 AppMain_IdleToText(g_app.mode, g_app.idle_substate),
                 (unsigned int)g_app.ldr.frame.calibration_done,
                 (unsigned int)g_app.ldr.frame.is_valid,
                 (unsigned int)g_app.ldr.frame.total,
                 (unsigned int)g_app.ldr.frame.contrast,
                 (long)g_app.motor.last_motion_command.axis1_step_hz,
                 (long)g_app.motor.last_motion_command.axis2_step_hz);
  AppMain_SendLine(text);
}

static void AppMain_SendCalibrationSummary(void)
{
  char text[APP_MAIN_TEXT_BUFFER_SIZE];

  (void)snprintf(text,
                 sizeof(text),
                 "CALDATA base:%u,%u,%u,%u floor:%u,%u,%u,%u done:%u\r\n",
                 (unsigned int)g_app.ldr.frame.baseline[0],
                 (unsigned int)g_app.ldr.frame.baseline[1],
                 (unsigned int)g_app.ldr.frame.baseline[2],
                 (unsigned int)g_app.ldr.frame.baseline[3],
                 (unsigned int)g_app.ldr.frame.noise_floor[0],
                 (unsigned int)g_app.ldr.frame.noise_floor[1],
                 (unsigned int)g_app.ldr.frame.noise_floor[2],
                 (unsigned int)g_app.ldr.frame.noise_floor[3],
                 (unsigned int)g_app.ldr.frame.calibration_done);
  AppMain_SendLine(text);
}

static void AppMain_SendConfigSummary(void)
{
  char text[APP_MAIN_TEXT_BUFFER_SIZE];

  (void)snprintf(text,
                 sizeof(text),
                 "CONFIG period_ms:%u track_min:%u contrast_min:%u deadband_x1000:%ld gain_x1000:%ld,%ld max_hz:%u,%u\r\n",
                 (unsigned int)g_app.control_period_ms,
                 (unsigned int)TRACK_VALID_TOTAL_MIN,
                 (unsigned int)TRACK_DIRECTION_CONTRAST_MIN,
                 (long)AppMain_FloatToX1000(CTRL_ERR_DEADBAND),
                 (long)AppMain_FloatToX1000(CTRL_AXIS1_OUTPUT_GAIN),
                 (long)AppMain_FloatToX1000(CTRL_AXIS2_OUTPUT_GAIN),
                 (unsigned int)CTRL_AXIS1_MAX_STEP_HZ,
                 (unsigned int)CTRL_AXIS2_MAX_STEP_HZ);
  AppMain_SendLine(text);
}

static void AppMain_SendHelp(void)
{
  AppMain_SendLine("B1 cycle manual | IDLE | TRACK | MANUAL | MAN 1..8 | PERIOD 1MS|2MS|5MS | RECAL | STATUS | CALDATA | CONFIG | HELP\r\n");
}

static void AppMain_RequestManualStage(
    uint8_t stage,
    uint8_t queued_by_calibration)
{
  g_app.requested_manual_stage = stage;
  g_app.requested_manual_stage_valid = 1U;

  if (queued_by_calibration != 0U)
  {
    g_app.requested_mode_after_calibration = MODE_MANUAL;
    AppMain_SendManualStageNotice(stage, "QUEUED");
    return;
  }

  if (g_app.mode != MODE_MANUAL)
  {
    AppMain_EnterManual();
  }

  if (g_app.mode == MODE_MANUAL)
  {
    if (g_app.requested_manual_stage_valid != 0U)
    {
      ManualControl_SetStage(&g_app.manual, g_app.requested_manual_stage);
      g_app.requested_manual_stage_valid = 0U;
    }

    AppMain_SendManualStageNotice(stage, "ACTIVE");
  }
}

static void AppMain_HandleButtonPress(uint32_t now_ms)
{
  uint8_t next_stage;

  if ((now_ms - g_app.last_button_tick_ms) < APP_MAIN_BUTTON_DEBOUNCE_MS)
  {
    return;
  }

  g_app.last_button_tick_ms = now_ms;

  if ((g_app.mode == MODE_IDLE) && (g_app.idle_substate == IDLE_CALIBRATING))
  {
    next_stage = (g_app.requested_manual_stage_valid != 0U)
                     ? (uint8_t)((g_app.requested_manual_stage + 1U) % STEPPER_TMC2209_SPEED_STAGE_COUNT)
                     : 0U;
    AppMain_RequestManualStage(next_stage, 1U);
    return;
  }

  if ((g_app.mode == MODE_MANUAL) && (ManualControl_IsStageValid(&g_app.manual) != 0U))
  {
    next_stage = (uint8_t)((ManualControl_GetStage(&g_app.manual) + 1U) % STEPPER_TMC2209_SPEED_STAGE_COUNT);
  }
  else if (MotorControl_IsManualStageValid(&g_app.motor) != 0U)
  {
    next_stage = (uint8_t)((MotorControl_GetManualStage(&g_app.motor) + 1U) % STEPPER_TMC2209_SPEED_STAGE_COUNT);
  }
  else
  {
    next_stage = 0U;
  }

  AppMain_RequestManualStage(next_stage, 0U);
}

static void AppMain_PollButton(uint32_t now_ms)
{
  GPIO_PinState button_state;

  button_state = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);
  if ((g_app.last_button_state == GPIO_PIN_SET) && (button_state == GPIO_PIN_RESET))
  {
    AppMain_HandleButtonPress(now_ms);
  }

  g_app.last_button_state = button_state;
}

static void AppMain_HandleCommand(
    const SerialCmd_t *command,
    uint32_t now_ms)
{
  if (command == NULL)
  {
    return;
  }

  switch (command->id)
  {
    case SERIAL_CMD_MODE_IDLE:
      g_app.requested_mode_after_calibration = MODE_IDLE;
      AppMain_EnterIdleWait();
      break;

    case SERIAL_CMD_MODE_TRACKING:
      if ((g_app.mode == MODE_IDLE) && (g_app.idle_substate == IDLE_CALIBRATING))
      {
        g_app.requested_mode_after_calibration = MODE_TRACKING;
      }
      else
      {
        AppMain_EnterTracking();
      }
      break;

    case SERIAL_CMD_MODE_MANUAL:
      if ((g_app.mode == MODE_IDLE) && (g_app.idle_substate == IDLE_CALIBRATING))
      {
        g_app.requested_mode_after_calibration = MODE_MANUAL;
      }
      else
      {
        AppMain_EnterManual();
      }
      break;

    case SERIAL_CMD_MANUAL_STAGE:
      AppMain_RequestManualStage((uint8_t)command->arg0,
                                 (uint8_t)((g_app.mode == MODE_IDLE) && (g_app.idle_substate == IDLE_CALIBRATING)));
      break;

    case SERIAL_CMD_RECALIBRATE:
      AppMain_StartCalibration(now_ms);
      break;

    case SERIAL_CMD_STATUS_QUERY:
      AppMain_SendStatusSummary();
      break;

    case SERIAL_CMD_CAL_QUERY:
      AppMain_SendCalibrationSummary();
      break;

    case SERIAL_CMD_CONFIG_QUERY:
      AppMain_SendConfigSummary();
      break;

    case SERIAL_CMD_CONTROL_PERIOD:
      AppMain_SetControlPeriod((uint32_t)command->arg0);
      break;

    case SERIAL_CMD_HELP:
      AppMain_SendHelp();
      break;

    case SERIAL_CMD_NONE:
    default:
      break;
  }
}

static void AppMain_RunIdle(uint32_t now_ms)
{
  MotorControl_StopAll(&g_app.motor);

  if (g_app.idle_substate == IDLE_CALIBRATING)
  {
    LdrTracking_AccumulateCalibration(&g_app.ldr);

    if ((now_ms - g_app.calibration_start_tick_ms) >= SYS_BOOT_CALIBRATION_MS)
    {
      AppMain_FinalizeCalibration();
    }
  }
}

static void AppMain_RunTracking(uint32_t now_ms)
{
  MotionCommand_t command;
  (void)now_ms;

  if (g_app.ldr.frame.is_valid == 0U)
  {
    TrackerController_Reset(&g_app.tracker);
    MotorControl_StopAll(&g_app.motor);
    return;
  }

  command = TrackerController_Run(&g_app.tracker, &g_app.ldr.frame, g_app.control_period_ms);
  (void)MotorControl_ApplySignedStepHz(&g_app.motor, &command);
}

static void AppMain_RunManual(void)
{
  ManualControl_Task(&g_app.manual, &g_app.motor);
}

static void AppMain_RunControl(uint32_t now_ms)
{
  LdrTracking_UpdateFrame(&g_app.ldr,
                          AppAdc_GetFilteredAdc1(&g_app.adc),
                          AppAdc_GetFilteredAdc2(&g_app.adc),
                          AppAdc_GetFilteredAdc3(&g_app.adc),
                          AppAdc_GetFilteredAdc4(&g_app.adc));

  switch (g_app.mode)
  {
    case MODE_IDLE:
      AppMain_RunIdle(now_ms);
      break;

    case MODE_TRACKING:
      AppMain_RunTracking(now_ms);
      break;

    case MODE_SEARCH:
      AppMain_EnterTracking();
      AppMain_RunTracking(now_ms);
      break;

    case MODE_MANUAL:
      AppMain_RunManual();
      break;

    default:
      AppMain_EnterIdleWait();
      break;
  }
}

static void AppMain_UpdateSnapshot(uint32_t now_ms)
{
  uint32_t index;

  g_app.snapshot.tick_ms = now_ms;
  g_app.snapshot.mode = g_app.mode;
  g_app.snapshot.idle_substate = g_app.idle_substate;
  g_app.snapshot.search_substate = SEARCH_HISTORY_BIAS;
  g_app.snapshot.calibration_done = g_app.ldr.frame.calibration_done;
  g_app.snapshot.source_valid = g_app.ldr.frame.is_valid;
  g_app.snapshot.manual_stage_valid = ManualControl_IsStageValid(&g_app.manual);
  g_app.snapshot.manual_stage = ManualControl_GetStage(&g_app.manual);
  g_app.snapshot.total_light = g_app.ldr.frame.total;
  g_app.snapshot.contrast = g_app.ldr.frame.contrast;
  g_app.snapshot.enc1_count = AppEncoder_GetCount1(&g_app.encoder);
  g_app.snapshot.enc2_count = AppEncoder_GetCount2(&g_app.encoder);
  g_app.snapshot.enc1_angle_x10000 = AppEncoder_GetAngle1X10000(&g_app.encoder);
  g_app.snapshot.enc2_angle_x10000 = AppEncoder_GetAngle2X10000(&g_app.encoder);
  g_app.snapshot.cmd_axis1_hz = g_app.motor.last_motion_command.axis1_step_hz;
  g_app.snapshot.cmd_axis2_hz = g_app.motor.last_motion_command.axis2_step_hz;
  g_app.snapshot.error_x_x1000 = AppMain_FloatToX1000(g_app.ldr.frame.error_x);
  g_app.snapshot.error_y_x1000 = AppMain_FloatToX1000(g_app.ldr.frame.error_y);

  for (index = 0U; index < 4U; index++)
  {
    g_app.snapshot.adc[index] = g_app.ldr.frame.raw[index];
    g_app.snapshot.baseline[index] = g_app.ldr.frame.baseline[index];
    g_app.snapshot.delta[index] = g_app.ldr.frame.delta[index];
  }
}

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
  const char ready_text[] = "APP READY DUAL MODE\r\n";
  const char error_text[] = "APP INIT ERROR\r\n";
  HAL_StatusTypeDef motor_status;

  (void)memset(&g_app, 0, sizeof(g_app));

  AppAdc_Init(&g_app.adc, hadc1, hadc2);
  AppEncoder_Init(&g_app.encoder, htim_enc_1, htim_enc_2);
  SerialCmd_Init(&g_app.serial, huart_log);
  LdrTracking_Init(&g_app.ldr);
  LdrTracking_ForceRecalibration(&g_app.ldr);
  TrackerController_Init(&g_app.tracker);
  ManualControl_Init(&g_app.manual);
  Telemetry_Init(&g_app.telemetry, huart_log, SYS_TELEMETRY_PERIOD_MS);

  motor_status = MotorControl_Init(&g_app.motor,
                                   htim_step_1,
                                   htim_step_2,
                                   huart_tmc_1,
                                   huart_tmc_2);
  if (motor_status != HAL_OK)
  {
    AppMain_SendLine(error_text);
  }
  else
  {
    AppMain_SendLine(ready_text);
  }

  g_app.mode = MODE_IDLE;
  g_app.idle_substate = IDLE_CALIBRATING;
  g_app.requested_mode_after_calibration = MODE_TRACKING;
  g_app.control_period_ms = SYS_CONTROL_PERIOD_DEFAULT_MS;
  g_app.calibration_start_tick_ms = HAL_GetTick();
  g_app.last_control_tick_ms = HAL_GetTick();
  g_app.last_button_tick_ms = HAL_GetTick();
  g_app.last_button_state = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);
}

void AppMain_Task(void)
{
  SerialCmd_t command;
  uint32_t now_ms;

  now_ms = HAL_GetTick();

  AppMain_PollButton(now_ms);
  SerialCmd_PollRx(&g_app.serial);
  AppAdc_Task(&g_app.adc);
  AppEncoder_Task(&g_app.encoder);

  while (SerialCmd_Dequeue(&g_app.serial, &command) != 0U)
  {
    AppMain_HandleCommand(&command, now_ms);
  }

  if ((now_ms - g_app.last_control_tick_ms) >= g_app.control_period_ms)
  {
    g_app.last_control_tick_ms = now_ms;
    AppMain_RunControl(now_ms);
  }

  AppMain_UpdateSnapshot(now_ms);
  Telemetry_Task(&g_app.telemetry, &g_app.snapshot);
}
