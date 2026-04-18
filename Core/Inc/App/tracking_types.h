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
  MODE_MANUAL
} SystemMode_t;

/* IDLE 子狀態 */
typedef enum {
  IDLE_CALIBRATING = 0,
  IDLE_WAIT_CMD
} IdleSubstate_t;

/* 串口指令 ID */
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
  SERIAL_CMD_HELP,
  SERIAL_CMD_HOME,          /* 位置歸零(機構在中間時呼叫) */
  SERIAL_CMD_POS_QUERY      /* 查兩軸位置 step 數 */
} SerialCmdId_t;

typedef struct {
  SerialCmdId_t id;
  int32_t arg0;
  int32_t arg1;
} SerialCmd_t;

/* LDR 一幀資料 */
typedef struct {
  uint16_t raw[4];
  uint16_t baseline[4];
  uint16_t noise_floor[4];
  uint16_t delta[4];
  uint16_t total;
  uint16_t contrast;
  float    error_x;
  float    error_y;
  uint8_t  is_valid;
  uint8_t  calibration_done;
} LdrTrackingFrame_t;

/* 雙軸運動指令
 * error_x/y 由 tracker 填,ApplyCommand 用它來判斷軸分離(dominance)。
 * manual 模式不經過 ApplyCommand,欄位留 0 即可。 */
typedef struct {
  int32_t axis1_step_hz;
  int32_t axis2_step_hz;
  float   error_x;
  float   error_y;
} MotionCommand_t;

/* 遙測快照 */
typedef struct {
  uint32_t       tick_ms;
  SystemMode_t   mode;
  IdleSubstate_t idle_substate;
  uint8_t        calibration_done;
  uint8_t        source_valid;
  uint8_t        manual_stage_valid;
  uint8_t        manual_stage;
  uint16_t       adc[4];
  uint16_t       baseline[4];
  uint16_t       delta[4];
  uint16_t       total_light;
  uint16_t       contrast;
  int32_t        cmd_axis1_hz;
  int32_t        cmd_axis2_hz;
  int32_t        error_x_x1000;
  int32_t        error_y_x1000;
} TelemetrySnapshot_t;

#ifdef __cplusplus
}
#endif

#endif /* APP_TRACKING_TYPES_H */
