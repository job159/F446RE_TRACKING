#include "App/app_main.h"
#include "App/app_adc.h"
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

#define TEXT_BUF_SIZE        192
#define BUTTON_DEBOUNCE_MS   180

/* B1 按鈕在 Nucleo F446RE 上是 PC13 */
#define B1_GPIO_Port         GPIOC
#define B1_Pin               GPIO_PIN_13

/* ---------- 全域狀態 ---------- */
typedef struct {
  AppAdc_HandleTypeDef             adc;
  SerialCmd_HandleTypeDef          serial;
  LdrTracking_HandleTypeDef        ldr;
  TrackerController_HandleTypeDef  tracker;
  MotorControl_HandleTypeDef       motor;
  ManualControl_HandleTypeDef      manual;
  Telemetry_HandleTypeDef          telem;
  TelemetrySnapshot_t              snap;

  SystemMode_t    mode;
  SystemMode_t    mode_after_cal;
  IdleSubstate_t  idle_sub;
  uint32_t        cal_start_tick;
  uint32_t        ctrl_period_ms;
  uint32_t        last_ctrl_tick;
  uint32_t        last_btn_tick;
  uint8_t         req_stage;
  uint8_t         req_stage_valid;
  GPIO_PinState   last_btn;
} App_t;

static App_t g;

/* ---------- 工具 ---------- */

static int32_t f_x1000(float v)
{
  return (v >= 0) ? (int32_t)(v * 1000.0f + 0.5f) : (int32_t)(v * 1000.0f - 0.5f);
}

static const char *mode_text(SystemMode_t m)
{
  switch (m) {
  case MODE_IDLE:     return "IDLE";
  case MODE_TRACKING: return "TRACK";
  case MODE_MANUAL:   return "MANUAL";
  default:            return "?";
  }
}

static void send(const char *t) { Telemetry_SendLine(&g.telem, t); }

/* ---------- 模式切換 ---------- */

static void enter_idle(void)
{
  MotorControl_StopAll(&g.motor);
  g.req_stage_valid = 0;
  g.mode     = MODE_IDLE;
  g.idle_sub = IDLE_WAIT_CMD;
}

static void start_calibration(uint32_t now)
{
  MotorControl_StopAll(&g.motor);
  TrackerController_Reset(&g.tracker);
  ManualControl_Reset(&g.manual);
  MotorControl_ClearManualStage(&g.motor);
  LdrTracking_ForceRecalibration(&g.ldr);

  g.mode            = MODE_IDLE;
  g.idle_sub        = IDLE_CALIBRATING;
  g.mode_after_cal  = MODE_TRACKING;
  g.req_stage_valid = 0;
  g.cal_start_tick  = now;
}

static void enter_tracking(void)
{
  if (!g.ldr.frame.calibration_done) { g.mode_after_cal = MODE_TRACKING; return; }
  MotorControl_StopAll(&g.motor);
  MotorControl_ClearManualStage(&g.motor);
  TrackerController_Reset(&g.tracker);
  g.req_stage_valid = 0;
  g.mode = MODE_TRACKING;
}

static void enter_manual(void)
{
  /* manual 不需要光源校正,任何時候(包含開機/校正中)都能直接進 */
  MotorControl_StopAll(&g.motor);
  TrackerController_Reset(&g.tracker);
  ManualControl_Reset(&g.manual);
  MotorControl_ClearManualStage(&g.motor);
  g.mode = MODE_MANUAL;

  if (g.req_stage_valid)
  {
    ManualControl_SetStage(&g.manual, g.req_stage);
    g.req_stage_valid = 0;
  }
}

static void finalize_cal(void)
{
  LdrTracking_FinalizeCalibration(&g.ldr);

  SystemMode_t next = g.mode_after_cal;
  g.mode_after_cal = MODE_IDLE;
  if      (next == MODE_TRACKING) enter_tracking();
  else if (next == MODE_MANUAL)   enter_manual();
  else                            enter_idle();
}

/* ---------- 報告 ---------- */

static void send_status(void)
{
  char buf[TEXT_BUF_SIZE];
  const char *idle_str = "-";
  if (g.mode == MODE_IDLE)
    idle_str = (g.idle_sub == IDLE_CALIBRATING) ? "CAL" : "WAIT";

  snprintf(buf, sizeof(buf),
      "STATUS mode:%s idle:%s cal:%u valid:%u total:%u contrast:%u cmd:%ld,%ld\r\n",
      mode_text(g.mode), idle_str,
      g.ldr.frame.calibration_done, g.ldr.frame.is_valid,
      g.ldr.frame.total, g.ldr.frame.contrast,
      (long)g.motor.last_cmd.axis1_step_hz, (long)g.motor.last_cmd.axis2_step_hz);
  send(buf);
}

static void send_caldata(void)
{
  char buf[TEXT_BUF_SIZE];
  snprintf(buf, sizeof(buf),
      "CALDATA base:%u,%u,%u,%u floor:%u,%u,%u,%u done:%u\r\n",
      g.ldr.frame.baseline[0], g.ldr.frame.baseline[1],
      g.ldr.frame.baseline[2], g.ldr.frame.baseline[3],
      g.ldr.frame.noise_floor[0], g.ldr.frame.noise_floor[1],
      g.ldr.frame.noise_floor[2], g.ldr.frame.noise_floor[3],
      g.ldr.frame.calibration_done);
  send(buf);
}

static void send_config(void)
{
  char buf[TEXT_BUF_SIZE];
  snprintf(buf, sizeof(buf),
      "CONFIG period_ms:%lu track_min:%u contrast_min:%u "
      "M1_kp:%ld/%ld/%ld M2_kp:%ld/%ld/%ld invert:%u\r\n",
      (unsigned long)g.ctrl_period_ms,
      TRACK_VALID_TOTAL_MIN, TRACK_DIRECTION_CONTRAST_MIN,
      (long)f_x1000(M1_KP_SMALL), (long)f_x1000(M1_KP_MEDIUM), (long)f_x1000(M1_KP_LARGE),
      (long)f_x1000(M2_KP_SMALL), (long)f_x1000(M2_KP_MEDIUM), (long)f_x1000(M2_KP_LARGE),
      ADC_INVERT);
  send(buf);
}

static void set_ctrl_period(uint32_t ms)
{
  if (ms != 1 && ms != 2 && ms != 5)
  {
    send("ERR period only 1|2|5\r\n");
    return;
  }
  g.ctrl_period_ms = ms;

  char buf[64];
  snprintf(buf, sizeof(buf), "PERIOD %luMS OK\r\n", (unsigned long)ms);
  send(buf);
}

/* ---------- 手動 stage ---------- */

static void send_stage_notice(uint8_t stage, const char *state)
{
  char buf[64];
  /* stage 0..6 -> F1..F7,  7..13 -> R1..R7 */
  char dir   = (stage < TMC_DIR_SPLIT_STAGE) ? 'F' : 'R';
  uint8_t n  = (stage % TMC_DIR_SPLIT_STAGE) + 1U;
  snprintf(buf, sizeof(buf), "MAN %c%u %s\r\n", dir, n, state);
  send(buf);
}

static void request_manual_stage(uint8_t stage, uint8_t during_cal)
{
  g.req_stage       = stage;
  g.req_stage_valid = 1;

  if (during_cal)
  {
    g.mode_after_cal = MODE_MANUAL;
    send_stage_notice(stage, "QUEUED");
    return;
  }

  if (g.mode != MODE_MANUAL) enter_manual();

  if (g.mode == MODE_MANUAL && g.req_stage_valid)
  {
    ManualControl_SetStage(&g.manual, g.req_stage);
    g.req_stage_valid = 0;
    send_stage_notice(stage, "ACTIVE");
  }
}

/* ---------- 按鈕 ---------- */

static void handle_button(uint32_t now)
{
  if ((now - g.last_btn_tick) < BUTTON_DEBOUNCE_MS) return;
  g.last_btn_tick = now;

  uint8_t next;

  /* 任何模式下按 B1 都直接進 manual / 切下一段,不用等校正 */
  if (g.mode == MODE_MANUAL && ManualControl_IsStageValid(&g.manual))
    next = (ManualControl_GetStage(&g.manual) + 1) % TMC_SPEED_STAGE_COUNT;
  else if (MotorControl_IsManualStageValid(&g.motor))
    next = (MotorControl_GetManualStage(&g.motor) + 1) % TMC_SPEED_STAGE_COUNT;
  else
    next = 0;

  request_manual_stage(next, 0);
}

static void poll_button(uint32_t now)
{
  GPIO_PinState btn = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);
  if (g.last_btn == GPIO_PIN_SET && btn == GPIO_PIN_RESET)
    handle_button(now);
  g.last_btn = btn;
}

/* ---------- 指令 ---------- */

static void handle_cmd(const SerialCmd_t *cmd, uint32_t now)
{
  uint8_t is_cal = (g.mode == MODE_IDLE && g.idle_sub == IDLE_CALIBRATING);

  switch (cmd->id)
  {
  case SERIAL_CMD_MODE_IDLE:     g.mode_after_cal = MODE_IDLE; enter_idle(); break;
  case SERIAL_CMD_MODE_TRACKING: if (is_cal) g.mode_after_cal = MODE_TRACKING; else enter_tracking(); break;
  case SERIAL_CMD_MODE_MANUAL:   if (is_cal) g.mode_after_cal = MODE_MANUAL;   else enter_manual();   break;
  case SERIAL_CMD_MANUAL_STAGE:  request_manual_stage((uint8_t)cmd->arg0, is_cal); break;
  case SERIAL_CMD_RECALIBRATE:   start_calibration(now); break;
  case SERIAL_CMD_STATUS_QUERY:  send_status();  break;
  case SERIAL_CMD_CAL_QUERY:     send_caldata(); break;
  case SERIAL_CMD_CONFIG_QUERY:  send_config();  break;
  case SERIAL_CMD_CONTROL_PERIOD: set_ctrl_period((uint32_t)cmd->arg0); break;
  case SERIAL_CMD_HELP:
    send("CMD: IDLE | TRACK | MANUAL | F1..F7 | R1..R7 | MAN 1..14 | "
         "PERIOD 1|2|5 | RECAL | STATUS | CAL? | CFG? | HELP\r\n");
    break;
  default: break;
  }
}

/* ---------- 控制迴圈 ---------- */

static void run_control(uint32_t now)
{
  /* 更新 LDR */
  LdrTracking_UpdateFrame(&g.ldr,
      AppAdc_GetFiltered(&g.adc, 0),
      AppAdc_GetFiltered(&g.adc, 1),
      AppAdc_GetFiltered(&g.adc, 2),
      AppAdc_GetFiltered(&g.adc, 3));

  switch (g.mode)
  {
  case MODE_IDLE:
    MotorControl_StopAll(&g.motor);
    if (g.idle_sub == IDLE_CALIBRATING)
    {
      LdrTracking_AccumulateCalibration(&g.ldr);
      if ((now - g.cal_start_tick) >= SYS_BOOT_CALIBRATION_MS)
        finalize_cal();
    }
    break;

  case MODE_TRACKING:
    if (!g.ldr.frame.is_valid)
    {
      TrackerController_Reset(&g.tracker);
      MotorControl_StopAll(&g.motor);
    }
    else
    {
      MotionCommand_t cmd = TrackerController_Run(&g.tracker, &g.ldr.frame, g.ctrl_period_ms);
      MotorControl_ApplyCommand(&g.motor, &cmd);
    }
    break;

  case MODE_MANUAL:
    ManualControl_Task(&g.manual, &g.motor);
    break;

  default:
    enter_idle();
    break;
  }
}

static void update_snapshot(uint32_t now)
{
  g.snap.tick_ms            = now;
  g.snap.mode               = g.mode;
  g.snap.idle_substate      = g.idle_sub;
  g.snap.calibration_done   = g.ldr.frame.calibration_done;
  g.snap.source_valid       = g.ldr.frame.is_valid;
  g.snap.manual_stage_valid = ManualControl_IsStageValid(&g.manual);
  g.snap.manual_stage       = ManualControl_GetStage(&g.manual);
  g.snap.total_light        = g.ldr.frame.total;
  g.snap.contrast           = g.ldr.frame.contrast;
  g.snap.cmd_axis1_hz       = g.motor.last_cmd.axis1_step_hz;
  g.snap.cmd_axis2_hz       = g.motor.last_cmd.axis2_step_hz;
  g.snap.error_x_x1000      = f_x1000(g.ldr.frame.error_x);
  g.snap.error_y_x1000      = f_x1000(g.ldr.frame.error_y);

  for (int i = 0; i < 4; i++)
  {
    g.snap.adc[i]      = g.ldr.frame.raw[i];
    g.snap.baseline[i] = g.ldr.frame.baseline[i];
    g.snap.delta[i]    = g.ldr.frame.delta[i];
  }
}

/* ---------- 公開介面 ---------- */

void AppMain_Init(ADC_HandleTypeDef  *hadc1,
                  ADC_HandleTypeDef  *hadc2,
                  UART_HandleTypeDef *huart_log,
                  TIM_HandleTypeDef  *htim_step1,
                  TIM_HandleTypeDef  *htim_step2,
                  UART_HandleTypeDef *huart_tmc1,
                  UART_HandleTypeDef *huart_tmc2)
{
  memset(&g, 0, sizeof(g));

  Telemetry_Init(&g.telem, huart_log, SYS_TELEMETRY_PERIOD_MS);

  AppAdc_Init(&g.adc, hadc1, hadc2);
  SerialCmd_Init(&g.serial, huart_log);
  LdrTracking_Init(&g.ldr);
  LdrTracking_ForceRecalibration(&g.ldr);
  TrackerController_Init(&g.tracker);
  ManualControl_Init(&g.manual);

  HAL_StatusTypeDef mst = MotorControl_Init(&g.motor, htim_step1, htim_step2, huart_tmc1, huart_tmc2);
  send((mst == HAL_OK) ? "APP READY\r\n" : "APP INIT ERROR\r\n");

  /* 開機做 5s 光源校正,校正完自動進 TRACKING。
   * 要改 manual 隨時按 B1 (manual 不用校正,會打斷追日)。 */
  g.mode             = MODE_IDLE;
  g.idle_sub         = IDLE_CALIBRATING;
  g.mode_after_cal   = MODE_TRACKING;
  g.ctrl_period_ms   = SYS_CONTROL_PERIOD_MS;
  g.cal_start_tick   = HAL_GetTick();
  g.last_ctrl_tick   = HAL_GetTick();
  g.last_btn_tick    = HAL_GetTick();
  g.last_btn         = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);
}

void AppMain_Task(void)
{
  uint32_t now = HAL_GetTick();

  poll_button(now);
  SerialCmd_PollRx(&g.serial);
  AppAdc_Task(&g.adc);

  SerialCmd_t cmd;
  while (SerialCmd_Dequeue(&g.serial, &cmd))
    handle_cmd(&cmd, now);

  if ((now - g.last_ctrl_tick) >= g.ctrl_period_ms)
  {
    g.last_ctrl_tick = now;
    run_control(now);
  }

  update_snapshot(now);
  Telemetry_Task(&g.telem, &g.snap);
}
