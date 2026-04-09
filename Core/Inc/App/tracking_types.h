#ifndef APP_TRACKING_TYPES_H
#define APP_TRACKING_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* 系統模式 */
typedef enum {
  MODE_IDLE = 0,
  MODE_TRACKING,
  MODE_SEARCH,
  MODE_MANUAL
} SystemMode_t;

/* IDLE子狀態 */
typedef enum {
  IDLE_CALIBRATING = 0,
  IDLE_WAIT_CMD
} IdleSubstate_t;

/* 搜尋子狀態 */
typedef enum {
  SEARCH_HISTORY_BIAS = 0,
  SEARCH_REVISIT_LAST_GOOD,
  SEARCH_SWEEP_SCAN
} SearchSubstate_t;

/* 串口指令ID */
typedef enum {
  SERIAL_CMD_NONE = 0,
  SERIAL_CMD_MODE_IDLE,
  SERIAL_CMD_MODE_TRACKING,
  SERIAL_CMD_MODE_MANUAL,
  SERIAL_CMD_MANUAL_STAGE,
  SERIAL_CMD_RECALIBRATE,
  SERIAL_CMD_STATUS_QUERY,
  SERIAL_CMD_CAL_QUERY,
  SERIAL_CMD_CONFIG_QUERY,
  SERIAL_CMD_CONTROL_PERIOD,
  SERIAL_CMD_HELP
} SerialCmdId_t;

/* 串口指令 */
typedef struct {
  SerialCmdId_t id;
  int32_t arg0;
  int32_t arg1;
} SerialCmd_t;

/* LDR追蹤用的一幀資料，存ADC差值、誤差等 */
typedef struct {
  uint16_t raw[4];
  uint16_t baseline[4];
  uint16_t noise_floor[4];
  uint16_t delta[4];
  uint16_t total;
  uint16_t contrast;
  float error_x;
  float error_y;
  uint8_t is_valid;
  uint8_t calibration_done;
} LdrTrackingFrame_t;

/* 單軸PID控制器狀態 */
typedef struct {
  float prev_error;
  float integrator;
  int32_t prev_output_hz;
} AxisController_t;

/* 雙軸運動指令 */
typedef struct {
  int32_t axis1_step_hz;
  int32_t axis2_step_hz;
} MotionCommand_t;

/* 追蹤歷史紀錄(搜尋策略用) */
typedef struct {
  uint32_t tick_ms;
  float error_x;
  float error_y;
  int32_t axis1_cmd_hz;
  int32_t axis2_cmd_hz;
  int32_t enc1_count;
  int32_t enc2_count;
  uint16_t total_light;
  uint8_t valid;
} TrackingHistoryEntry_t;

/* 遙測快照，定時送出 */
typedef struct {
  uint32_t tick_ms;
  SystemMode_t mode;
  IdleSubstate_t idle_substate;
  SearchSubstate_t search_substate;
  uint8_t calibration_done;
  uint8_t source_valid;
  uint8_t manual_stage_valid;
  uint8_t manual_stage;
  uint16_t adc[4];
  uint16_t baseline[4];
  uint16_t delta[4];
  uint16_t total_light;
  uint16_t contrast;
  int32_t enc1_count;
  int32_t enc2_count;
  uint32_t enc1_angle_x10000;
  uint32_t enc2_angle_x10000;
  int32_t cmd_axis1_hz;
  int32_t cmd_axis2_hz;
  int32_t error_x_x1000;
  int32_t error_y_x1000;
} TelemetrySnapshot_t;

#ifdef __cplusplus
}
#endif

#endif /* APP_TRACKING_TYPES_H */
