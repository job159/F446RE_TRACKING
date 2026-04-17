#include "App/tracker_controller.h"
#include "App/tracking_config.h"
#include <string.h>

/* 一組軸的 PID 參數 (從 tracking_config.h 帶入) */
typedef struct {
  float    kp_small;
  float    kp_medium;
  float    kp_large;
  float    ki;
  float    kd;
  float    output_gain;
  float    pos_scale;
  float    neg_scale;
  uint16_t max_step_hz;
  uint16_t rate_limit_hz;
} AxisParams_t;

static const AxisParams_t M1_PARAMS = {
  M1_KP_SMALL, M1_KP_MEDIUM, M1_KP_LARGE,
  M1_KI, M1_KD,
  M1_OUTPUT_GAIN, M1_POS_SCALE, M1_NEG_SCALE,
  M1_MAX_STEP_HZ, M1_RATE_LIMIT_HZ
};

static const AxisParams_t M2_PARAMS = {
  M2_KP_SMALL, M2_KP_MEDIUM, M2_KP_LARGE,
  M2_KI, M2_KD,
  M2_OUTPUT_GAIN, M2_POS_SCALE, M2_NEG_SCALE,
  M2_MAX_STEP_HZ, M2_RATE_LIMIT_HZ
};

/* 依誤差大小選 KP */
static float pick_kp(const AxisParams_t *p, float abs_err)
{
  if (abs_err <= PID_ERR_SMALL)  return p->kp_small;
  if (abs_err <= PID_ERR_MEDIUM) return p->kp_medium;
  return p->kp_large;
}

/* 單軸 PID,回傳 step hz */
static int32_t run_axis(AxisController_t *ax, const AxisParams_t *p,
                        float error, uint32_t period_ms)
{
  if (period_ms == 0) return 0;

  float dt = (float)period_ms / 1000.0f;
  float abs_e = (error < 0) ? -error : error;

  /* 死區: 誤差太小直接停,並讓積分器衰減 */
  if (abs_e <= PID_ERR_DEADBAND)
  {
    ax->integrator   *= PID_INTEGRATOR_DECAY;
    ax->prev_error    = error;
    ax->prev_output_hz = 0;
    return 0;
  }

  float kp    = pick_kp(p, abs_e);
  float deriv = (error - ax->prev_error) / dt;

  /* 中小誤差才積分,避免 windup */
  if (abs_e <= PID_ERR_MEDIUM)
    ax->integrator += error * p->ki * dt;

  float out = (kp * error + ax->integrator + p->kd * deriv) * p->output_gain;
  out *= (out >= 0) ? p->pos_scale : p->neg_scale;

  /* 輸出限幅 */
  if (out > (float)p->max_step_hz)  out =  (float)p->max_step_hz;
  if (out < -(float)p->max_step_hz) out = -(float)p->max_step_hz;

  int32_t hz = (int32_t)out;

  /* 速率限制 */
  int32_t delta = hz - ax->prev_output_hz;
  if (delta >  (int32_t)p->rate_limit_hz) hz = ax->prev_output_hz + (int32_t)p->rate_limit_hz;
  if (delta < -(int32_t)p->rate_limit_hz) hz = ax->prev_output_hz - (int32_t)p->rate_limit_hz;

  ax->prev_error    = error;
  ax->prev_output_hz = hz;
  return hz;
}

void TrackerController_Init(TrackerController_HandleTypeDef *h)
{
  memset(h, 0, sizeof(*h));
}

void TrackerController_Reset(TrackerController_HandleTypeDef *h)
{
  memset(h, 0, sizeof(*h));
}

MotionCommand_t TrackerController_Run(TrackerController_HandleTypeDef *h,
    const LdrTrackingFrame_t *frame, uint32_t period_ms)
{
  MotionCommand_t cmd = {0, 0};
  if (frame == NULL || !frame->is_valid) return cmd;

  /* 乘上 M*_TRACK_DIR 可整軸翻轉追蹤方向(機構裝反時用) */
  cmd.axis1_step_hz = M1_TRACK_DIR * run_axis(&h->axis1, &M1_PARAMS, frame->error_x, period_ms);
  cmd.axis2_step_hz = M2_TRACK_DIR * run_axis(&h->axis2, &M2_PARAMS, frame->error_y, period_ms);
  return cmd;
}
