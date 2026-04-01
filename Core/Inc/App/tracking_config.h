#ifndef APP_TRACKING_CONFIG_H
#define APP_TRACKING_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#define SYS_BOOT_CALIBRATION_MS              5000U
#define SYS_CONTROL_PERIOD_DEFAULT_MS        1U
#define SYS_TELEMETRY_PERIOD_MS              100U

#define LDR_CHANNEL_COUNT                    4U
#define LDR_BASELINE_MARGIN                  10U
#define LDR_MIN_NOISE_FLOOR                  6U

#define TRACK_VALID_TOTAL_MIN                140U
#define TRACK_DIRECTION_CONTRAST_MIN         28U
#define TRACK_REACQUIRE_CONSECUTIVE_COUNT    2U
#define TRACK_LOST_CONSECUTIVE_COUNT         3U

#define CTRL_ERR_DEADBAND                    0.015f
#define CTRL_ERR_SMALL                       0.055f
#define CTRL_ERR_MEDIUM                      0.140f

#define CTRL_KP_SMALL                        180.0f
#define CTRL_KP_MEDIUM                       360.0f
#define CTRL_KP_LARGE                        620.0f
#define CTRL_KI                              8.0f
#define CTRL_KD                              18.0f

#define CTRL_AXIS1_OUTPUT_GAIN               2.0f
#define CTRL_AXIS2_OUTPUT_GAIN               2.0f

#define CTRL_AXIS1_MAX_STEP_HZ               60000U
#define CTRL_AXIS2_MAX_STEP_HZ               60000U
#define CTRL_AXIS1_RATE_LIMIT_STEP_HZ        16250U
#define CTRL_AXIS2_RATE_LIMIT_STEP_HZ        13750U

#define CTRL_AXIS1_POS_SCALE                 1.10f
#define CTRL_AXIS1_NEG_SCALE                 1.24f
#define CTRL_AXIS2_POS_SCALE                 1.02f
#define CTRL_AXIS2_NEG_SCALE                 1.16f

#define CTRL_AXIS1_ERROR_SIGN                1.0f
#define CTRL_AXIS2_ERROR_SIGN                1.0f

#define SEARCH_HISTORY_LEN                   16U
#define SEARCH_BIAS_STEP_HZ                  900U
#define SEARCH_BIAS_HOLD_MS                  100U
#define SEARCH_HISTORY_BIAS_CYCLES           6U
#define SEARCH_REVISIT_STEP_HZ               650U
#define SEARCH_REVISIT_MAX_MS                1000U
#define SEARCH_REVISIT_TOL_COUNTS            80
#define SEARCH_SWEEP_STEP_HZ                 750U
#define SEARCH_SWEEP_Y_STEP_HZ               420U
#define SEARCH_SWEEP_HOLD_MS                 120U

#define SERIAL_CMD_RX_LINE_MAX               32U
#define SERIAL_CMD_QUEUE_LENGTH              4U

#ifdef __cplusplus
}
#endif

#endif /* APP_TRACKING_CONFIG_H */
