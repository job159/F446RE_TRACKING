#include "App/tracker_controller.h"

#include "App/tracking_config.h"

#include <string.h>

static float TrackerController_Abs(float value)
{
  return (value >= 0.0f) ? value : -value;
}

static float TrackerController_SelectKp(float abs_error)
{
  if (abs_error <= CTRL_ERR_SMALL)
  {
    return CTRL_KP_SMALL;
  }

  if (abs_error <= CTRL_ERR_MEDIUM)
  {
    return CTRL_KP_MEDIUM;
  }

  return CTRL_KP_LARGE;
}

static int32_t TrackerController_RunAxis(
    AxisController_t *axis,
    float error,
    float output_gain,
    float pos_scale,
    float neg_scale,
    uint16_t max_step_hz,
    uint16_t rate_limit_hz,
    uint32_t control_period_ms)
{
  float dt_s;
  float abs_error;
  float kp;
  float derivative;
  float output_f;
  int32_t output_hz;
  int32_t delta_hz;

  if ((axis == NULL) || (control_period_ms == 0U))
  {
    return 0;
  }

  dt_s = (float)control_period_ms / 1000.0f;
  abs_error = TrackerController_Abs(error);

  if (abs_error <= CTRL_ERR_DEADBAND)
  {
    axis->integrator *= 0.8f;
    axis->prev_error = error;
    axis->prev_output_hz = 0;
    return 0;
  }

  kp = TrackerController_SelectKp(abs_error);
  derivative = (error - axis->prev_error) / dt_s;

  if (abs_error <= CTRL_ERR_MEDIUM)
  {
    axis->integrator += (error * CTRL_KI * dt_s);
  }

  output_f = (kp * error) + axis->integrator + (CTRL_KD * derivative);
  output_f *= output_gain;

  if (output_f >= 0.0f)
  {
    output_f *= pos_scale;
  }
  else
  {
    output_f *= neg_scale;
  }

  if (output_f > (float)max_step_hz)
  {
    output_f = (float)max_step_hz;
  }
  else if (output_f < -(float)max_step_hz)
  {
    output_f = -(float)max_step_hz;
  }

  output_hz = (int32_t)output_f;
  delta_hz = output_hz - axis->prev_output_hz;

  if (delta_hz > (int32_t)rate_limit_hz)
  {
    output_hz = axis->prev_output_hz + (int32_t)rate_limit_hz;
  }
  else if (delta_hz < -(int32_t)rate_limit_hz)
  {
    output_hz = axis->prev_output_hz - (int32_t)rate_limit_hz;
  }

  axis->prev_error = error;
  axis->prev_output_hz = output_hz;
  return output_hz;
}

void TrackerController_Init(TrackerController_HandleTypeDef *handle)
{
  if (handle == NULL)
  {
    return;
  }

  (void)memset(handle, 0, sizeof(*handle));
}

void TrackerController_Reset(TrackerController_HandleTypeDef *handle)
{
  TrackerController_Init(handle);
}

MotionCommand_t TrackerController_Run(
    TrackerController_HandleTypeDef *handle,
    const LdrTrackingFrame_t *frame,
    uint32_t control_period_ms)
{
  MotionCommand_t command = {0, 0};

  if ((handle == NULL) || (frame == NULL) || (frame->is_valid == 0U))
  {
    return command;
  }

  command.axis1_step_hz = TrackerController_RunAxis(&handle->axis1,
                                                    frame->error_x * CTRL_AXIS1_ERROR_SIGN,
                                                    CTRL_AXIS1_OUTPUT_GAIN,
                                                    CTRL_AXIS1_POS_SCALE,
                                                    CTRL_AXIS1_NEG_SCALE,
                                                    CTRL_AXIS1_MAX_STEP_HZ,
                                                    CTRL_AXIS1_RATE_LIMIT_STEP_HZ,
                                                    control_period_ms);
  command.axis2_step_hz = TrackerController_RunAxis(&handle->axis2,
                                                    frame->error_y * CTRL_AXIS2_ERROR_SIGN,
                                                    CTRL_AXIS2_OUTPUT_GAIN,
                                                    CTRL_AXIS2_POS_SCALE,
                                                    CTRL_AXIS2_NEG_SCALE,
                                                    CTRL_AXIS2_MAX_STEP_HZ,
                                                    CTRL_AXIS2_RATE_LIMIT_STEP_HZ,
                                                    control_period_ms);

  return command;
}
