#include "App/tracker_controller.h"
#include "App/tracking_config.h"
#include <string.h>

/* 一組軸的參數 (從 tracking_config.h 帶入) */
typedef struct {
  float    deadband;
  float    kp_small;
  float    kp_medium;
  float    kp_large;
  float    output_gain;
  float    pos_scale;
  float    neg_scale;
  uint16_t max_step_hz;
} AxisParams_t;

static const AxisParams_t M1_PARAMS = {
  M1_ERR_DEADBAND,
  M1_KP_SMALL, M1_KP_MEDIUM, M1_KP_LARGE,
  M1_OUTPUT_GAIN, M1_POS_SCALE, M1_NEG_SCALE,
  M1_MAX_STEP_HZ
};

static const AxisParams_t M2_PARAMS = {
  PID_ERR_DEADBAND,
  M2_KP_SMALL, M2_KP_MEDIUM, M2_KP_LARGE,
  M2_OUTPUT_GAIN, M2_POS_SCALE, M2_NEG_SCALE,
  M2_MAX_STEP_HZ
};

/* 依誤差大小選 KP */
static float pick_kp(const AxisParams_t *p, float abs_err)
{
  if (abs_err <= PID_ERR_SMALL)  return p->kp_small;
  if (abs_err <= PID_ERR_MEDIUM) return p->kp_medium;
  return p->kp_large;
}

/* 純比例控制,回傳 step hz
 * 沒有積分、沒有微分、沒有速率限制、沒有記憶。
 * 每個 cycle 都重新從當下誤差算,不受歷史影響。 */
static int32_t run_axis(const AxisParams_t *p, float error)
{
  float abs_e = (error < 0) ? -error : error;

  /* 死區: 誤差太小直接停 (M1/M2 可獨立) */
  if (abs_e <= p->deadband) return 0;

  float kp  = pick_kp(p, abs_e);
  float out = kp * error * p->output_gain;
  /* pos/neg scale 不在這裡套,交給 app 層在最終物理 hz 決定後再套,
   * 避免 TRACK_DIR 或條件翻號時 scale 套到反邊造成單側抖動。 */

  /* 輸出限幅 */
  if (out > (float)p->max_step_hz)  out =  (float)p->max_step_hz;
  if (out < -(float)p->max_step_hz) out = -(float)p->max_step_hz;

  return (int32_t)out;
}

void TrackerController_Init(TrackerController_HandleTypeDef *h)
{
  memset(h, 0, sizeof(*h));
}

void TrackerController_Reset(TrackerController_HandleTypeDef *h)
{
  memset(h, 0, sizeof(*h));
}

/* error 飽和:抗「陰影側 LDR 讀值=0」造成的假極端誤差 */
static float saturate_err(float e)
{
  if (e >  TRACK_ERR_CAP) return  TRACK_ERR_CAP;
  if (e < -TRACK_ERR_CAP) return -TRACK_ERR_CAP;
  return e;
}

MotionCommand_t TrackerController_Run(TrackerController_HandleTypeDef *h,
    const LdrTrackingFrame_t *frame, uint32_t period_ms)
{
  (void)h;         /* 純比例控制不需要狀態 */
  (void)period_ms; /* 沒有積分/微分,不需要 dt */

  MotionCommand_t cmd = {0, 0, 0.0f, 0.0f};
  if (frame == NULL || !frame->is_valid) return cmd;

  /* 先把原始 error 飽和到 ±TRACK_ERR_CAP,擋陰影假值 */
  float ex = saturate_err(frame->error_x);
  float ey = saturate_err(frame->error_y);

  /* 乘上 M*_TRACK_DIR 可整軸翻轉追蹤方向(機構裝反時用)
   * 兩軸獨立,同時依各自誤差輸出 hz,不做 dominance 抑制。
   *
   * [軸對調] 硬體上 M1 裝在垂直(上下)、M2 裝在水平(左右),
   *          所以 error_y 驅動 axis1,error_x 驅動 axis2。 */
  cmd.axis1_step_hz = M1_TRACK_DIR * run_axis(&M1_PARAMS, ey);
  cmd.axis2_step_hz = M2_TRACK_DIR * run_axis(&M2_PARAMS, ex);
  cmd.error_x = ex;
  cmd.error_y = ey;
  return cmd;
}
