#ifndef APP_TRACKING_CONFIG_H
#define APP_TRACKING_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 系統時間參數 ---- */
#define SYS_BOOT_CALIBRATION_MS           5000U
#define SYS_CONTROL_PERIOD_DEFAULT_MS     1U
#define SYS_TELEMETRY_PERIOD_MS           100U

/* ---- ADC低通濾波 (千分比) ---- */
#define ADC_LPF_OLD_WEIGHT                100U    /* 舊值權重 */
#define ADC_LPF_NEW_WEIGHT                900U    /* 新值權重 */
#define ADC_LPF_SCALE                     1000U   /* 總比例 */

/* ---- ADC硬體參數 ---- */
#define ADC_12BIT_MAX                     4095U

/* ---- LDR校正 ---- */
#define LDR_CHANNEL_COUNT                 4U
#define LDR_BASELINE_MARGIN               10U
#define LDR_MIN_NOISE_FLOOR               6U

/* ---- 追蹤判定門檻 ---- */
#define TRACK_VALID_TOTAL_MIN             140U
#define TRACK_DIRECTION_CONTRAST_MIN      28U

/* ---- PID參數 ---- */
#define CTRL_INTEGRATOR_DECAY             0.8f    /* 死區內積分器衰減比例 */
#define CTRL_ERR_DEADBAND                 0.015f
#define CTRL_ERR_SMALL                    0.055f
#define CTRL_ERR_MEDIUM                   0.140f
#define CTRL_KP_SMALL                     180.0f
#define CTRL_KP_MEDIUM                    360.0f
#define CTRL_KP_LARGE                     620.0f
#define CTRL_KI                           8.0f
#define CTRL_KD                           18.0f

/* ---- 軸輸出增益與限制 ---- */
#define CTRL_AXIS1_OUTPUT_GAIN            2.0f
#define CTRL_AXIS2_OUTPUT_GAIN            2.0f
#define CTRL_AXIS1_MAX_STEP_HZ            60000U
#define CTRL_AXIS2_MAX_STEP_HZ            60000U
#define CTRL_AXIS1_RATE_LIMIT_STEP_HZ     16250U
#define CTRL_AXIS2_RATE_LIMIT_STEP_HZ     13750U

/* ---- 軸方向補償 ---- */
#define CTRL_AXIS1_POS_SCALE              1.10f
#define CTRL_AXIS1_NEG_SCALE              1.24f
#define CTRL_AXIS2_POS_SCALE              1.02f
#define CTRL_AXIS2_NEG_SCALE              1.16f
#define CTRL_AXIS1_ERROR_SIGN             1.0f
#define CTRL_AXIS2_ERROR_SIGN             1.0f

/* ---- 搜尋策略 ---- */
#define SEARCH_HISTORY_LEN                16U
#define SEARCH_BIAS_STEP_HZ               900U
#define SEARCH_BIAS_HOLD_MS               100U
#define SEARCH_HISTORY_BIAS_CYCLES        6U
#define SEARCH_REVISIT_STEP_HZ            650U
#define SEARCH_REVISIT_MAX_MS             1000U
#define SEARCH_REVISIT_TOL_COUNTS         80
#define SEARCH_SWEEP_STEP_HZ              750U
#define SEARCH_SWEEP_Y_STEP_HZ            420U
#define SEARCH_SWEEP_HOLD_MS              120U

/* ---- 串口指令 ---- */
#define SERIAL_CMD_RX_LINE_MAX            32U
#define SERIAL_CMD_QUEUE_LENGTH           4U

#ifdef __cplusplus
}
#endif

#endif /* APP_TRACKING_CONFIG_H */
