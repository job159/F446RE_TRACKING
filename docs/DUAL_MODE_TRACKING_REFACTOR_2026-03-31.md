# Dual Mode Tracking Refactor

Date: 2026-03-31

## Goal

Refactor the current bring-up / manual-test project into a dual-mode tracking system with:

- 5-second startup adaptive calibration
- serial-controlled mode switching
- `MODE_IDLE`
- `MODE_TRACKING`
- `MODE_SEARCH`
- `MODE_MANUAL`

## Existing Base

The current project already provides:

- 4-channel ADC sampling via DMA in `app_adc`
- 2-channel encoder accumulation in `app_encoder`
- dual TMC2209 stepper output in `stepper_tmc2209`
- `USART2` logging

## Target Module Split

Keep the low-level modules and add:

- `tracking_config.h`
- `tracking_types.h`
- `serial_cmd.[hc]`
- `ldr_tracking.[hc]`
- `tracker_controller.[hc]`
- `search_strategy.[hc]`
- `motor_control.[hc]`
- `manual_control.[hc]`
- `telemetry.[hc]`

## State Machine

### Main modes

- `MODE_IDLE`
- `MODE_TRACKING`
- `MODE_SEARCH`
- `MODE_MANUAL`

### Idle substate

- `IDLE_CALIBRATING`
- `IDLE_WAIT_CMD`

### Search substate

- `SEARCH_HISTORY_BIAS`
- `SEARCH_REVISIT_LAST_GOOD`
- `SEARCH_SWEEP_SCAN`

## Startup Calibration

For the first 5 seconds:

- motors stay stopped
- accumulate 4 filtered LDR values
- compute baseline and noise floor for each LDR

Use:

- `baseline[i] = average_5s[i]`
- `noise_floor[i] = max(max_5s - min_5s + margin, minimum_floor)`

## Tracking

Use 2x2 LDR layout:

```text
TL  TR
BL  BR
```

Compute:

- `delta[i] = max(filtered[i] - baseline[i] - noise_floor[i], 0)`
- `sum_left = TL + BL`
- `sum_right = TR + BR`
- `sum_top = TL + TR`
- `sum_bottom = BL + BR`
- `total = TL + TR + BL + BR`
- `error_x = (sum_right - sum_left) / total`
- `error_y = (sum_top - sum_bottom) / total`

Tracking is valid only if:

- `total >= TRACK_VALID_TOTAL_MIN`
- `contrast >= TRACK_DIRECTION_CONTRAST_MIN`

Controller style:

- nonlinear gain-scheduled PI/PID equivalent
- large error => large output
- small error => reduced output
- deadband near zero
- output saturation
- rate limit to avoid jitter

## Search

When tracking becomes invalid:

1. use history-biased search first
2. revisit last good encoder position
3. fall back to sweep scan

History keeps:

- recent `error_x`
- recent `error_y`
- recent command direction
- recent valid encoder position

## Serial Commands

Mode switching:

- `0` or `MODE 0`
- `1` or `MODE 1`
- `2` or `MODE 2`

Manual stage commands:

- `F1` `F2` `F3` `F4`
- `R1` `R2` `R3` `R4`
- `STAGE 0` .. `STAGE 7`

Service commands:

- `CAL`
- `STAT?`
- `CAL?`
- `CFG?`
- `HELP`

## Stepper API Extension

Keep current stage API and add:

```c
HAL_StatusTypeDef StepperTmc2209_SetStepHz(
    StepperTmc2209_HandleTypeDef *handle,
    GPIO_PinState direction_state,
    uint16_t step_hz);

HAL_StatusTypeDef StepperTmc2209_Stop(
    StepperTmc2209_HandleTypeDef *handle);

HAL_StatusTypeDef StepperTmc2209_SetSignedStepRate(
    StepperTmc2209_HandleTypeDef *handle,
    int32_t signed_step_hz);
```

## Integration Order

1. add serial parser
2. add new App-layer modules
3. rewrite `app_main.c` as scheduler + state machine
4. extend `stepper_tmc2209`
5. update `Debug` build list
6. compile and verify
