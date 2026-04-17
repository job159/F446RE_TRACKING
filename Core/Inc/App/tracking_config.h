#ifndef APP_TRACKING_CONFIG_H
#define APP_TRACKING_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 *  系統
 * ========================================================================= */
#define SYS_BOOT_CALIBRATION_MS      5000U   /* 開機校正時間 */
#define SYS_CONTROL_PERIOD_MS        5U      /* PID 週期,可用 1/2/5 */
#define SYS_TELEMETRY_PERIOD_MS      100U    /* 遙測輸出週期 */

/* =========================================================================
 *  ADC
 * ========================================================================= */
#define ADC_12BIT_MAX                4095U
#define ADC_INVERT                   1       /* 硬體分壓反向時設 1 (亮→值低) */
#define ADC_LPF_ALPHA_NEW            9       /* 低通: new*9 + old*1,除以 10 */

/* =========================================================================
 *  LDR 追蹤判定
 * ========================================================================= */
#define LDR_CHANNEL_COUNT            4U
#define LDR_BASELINE_MARGIN          10U     /* 校正後 noise floor 額外裕度 */
#define LDR_MIN_NOISE_FLOOR          6U
#define TRACK_VALID_TOTAL_MIN        140U    /* 4 路 delta 加總最小值 */
#define TRACK_DIRECTION_CONTRAST_MIN 28U     /* 最亮-最暗差,小於不追蹤 */

/* =========================================================================
 *  PID - 共用
 * ========================================================================= */
#define PID_ERR_DEADBAND             0.015f  /* 誤差小於此值完全不動 */
#define PID_ERR_SMALL                0.055f  /* 切到 KP_SMALL 的門檻 */
#define PID_ERR_MEDIUM               0.140f  /* 切到 KP_MEDIUM 的門檻 */
#define PID_INTEGRATOR_DECAY         0.8f    /* 死區內積分器衰減 */

/* =========================================================================
 *  追蹤方向翻轉(只影響 TRACKING 模式,manual 不受影響)
 *  機構裝反、或想整體調轉時,把對應軸改成 -1 即可
 * ========================================================================= */
#define M1_TRACK_DIR                 (+1)    /* Motor1 (水平軸): +1 正常, -1 反向 */
#define M2_TRACK_DIR                 (+1)    /* Motor2 (垂直軸): +1 正常, -1 反向 */

/* =========================================================================
 *  PID - Motor1 (緩和版,大約 Motor2 的一半)
 * ========================================================================= */
#define M1_KP_SMALL                  90.0f
#define M1_KP_MEDIUM                 180.0f
#define M1_KP_LARGE                  310.0f
#define M1_KI                        4.0f
#define M1_KD                        9.0f
#define M1_OUTPUT_GAIN               1.0f
#define M1_POS_SCALE                 1.10f
#define M1_NEG_SCALE                 1.24f
#define M1_MAX_STEP_HZ               60000U
#define M1_RATE_LIMIT_HZ             8000U   /* 緩和:降低單次變化量 */

/* =========================================================================
 *  PID - Motor2 (原本數值)
 * ========================================================================= */
#define M2_KP_SMALL                  180.0f
#define M2_KP_MEDIUM                 360.0f
#define M2_KP_LARGE                  620.0f
#define M2_KI                        8.0f
#define M2_KD                        18.0f
#define M2_OUTPUT_GAIN               2.0f
#define M2_POS_SCALE                 1.02f
#define M2_NEG_SCALE                 1.16f
#define M2_MAX_STEP_HZ               60000U
#define M2_RATE_LIMIT_HZ             13750U

/* =========================================================================
 *  串口
 * ========================================================================= */
#define SERIAL_CMD_RX_LINE_MAX       32U
#define SERIAL_CMD_QUEUE_LENGTH      4U

#ifdef __cplusplus
}
#endif

#endif /* APP_TRACKING_CONFIG_H */
