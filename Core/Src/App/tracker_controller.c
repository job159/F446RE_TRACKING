#include "App/tracker_controller.h"
#include "App/tracking_config.h"
#include <string.h>

/* 根據誤差大小選不同的Kp：小誤差慢追,大誤差快追 */
static float pick_kp(float abs_err)
{
  if (abs_err <= CTRL_ERR_SMALL)  return CTRL_KP_SMALL;
  if (abs_err <= CTRL_ERR_MEDIUM) return CTRL_KP_MEDIUM;
  return CTRL_KP_LARGE;
}

/* 單軸PID計算，回傳step hz */
static int32_t run_axis(AxisController_t *ax, float error,
    float gain, float pos_scale, float neg_scale,
    uint16_t max_hz, uint16_t rate_limit, uint32_t period_ms)
{
  if (period_ms == 0) return 0;

  float dt = (float)period_ms / 1000.0f;
  float abs_e = error;
  if (abs_e < 0) abs_e = -abs_e;

  /* 死區：誤差很小時不動作，讓integrator慢慢衰減 */
  if (abs_e <= CTRL_ERR_DEADBAND)
  {
    ax->integrator *= CTRL_INTEGRATOR_DECAY;
    ax->prev_error = error;
    ax->prev_output_hz = 0;
    return 0;
  }

  float kp = pick_kp(abs_e);
  float deriv = (error - ax->prev_error) / dt;

  /* 只在中小誤差範圍做積分，避免大幅overshoot */
  if (abs_e <= CTRL_ERR_MEDIUM)
    ax->integrator += error * CTRL_KI * dt;

  float out = kp * error + ax->integrator + CTRL_KD * deriv;
  out *= gain;

  /* 正負方向補償(機構不對稱) */
  if (out >= 0)
    out *= pos_scale;
  else
    out *= neg_scale;

  /* 輸出限幅 */
  if (out > (float)max_hz) out = (float)max_hz;
  else if (out < -(float)max_hz) out = -(float)max_hz;

  int32_t hz = (int32_t)out;

  /* 限制變化速率，避免突然跳太大 */
  int32_t delta = hz - ax->prev_output_hz;
  if (delta > (int32_t)rate_limit)
    hz = ax->prev_output_hz + (int32_t)rate_limit;
  else if (delta < -(int32_t)rate_limit)
    hz = ax->prev_output_hz - (int32_t)rate_limit;

  ax->prev_error = error;
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

  cmd.axis1_step_hz = run_axis(&h->axis1,
      frame->error_x * CTRL_AXIS1_ERROR_SIGN,
      CTRL_AXIS1_OUTPUT_GAIN,
      CTRL_AXIS1_POS_SCALE, CTRL_AXIS1_NEG_SCALE,
      CTRL_AXIS1_MAX_STEP_HZ, CTRL_AXIS1_RATE_LIMIT_STEP_HZ,
      period_ms);

  cmd.axis2_step_hz = run_axis(&h->axis2,
      frame->error_y * CTRL_AXIS2_ERROR_SIGN,
      CTRL_AXIS2_OUTPUT_GAIN,
      CTRL_AXIS2_POS_SCALE, CTRL_AXIS2_NEG_SCALE,
      CTRL_AXIS2_MAX_STEP_HZ, CTRL_AXIS2_RATE_LIMIT_STEP_HZ,
      period_ms);

  return cmd;
}
