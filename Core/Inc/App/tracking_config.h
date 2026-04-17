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
 *  控制 - 共用(純比例,沒有記憶:不積分、不微分、不速率限制)
 *  每個 cycle 的馬達速度只看當下誤差,沒有歷史 → 不會來回擺盪
 * ========================================================================= */
#define PID_ERR_DEADBAND             0.025f  /* 誤差小於此值完全不動 */
#define PID_ERR_SMALL                0.055f  /* 切到 KP_SMALL 的門檻 */
#define PID_ERR_MEDIUM               0.140f  /* 切到 KP_MEDIUM 的門檻 */

/* =========================================================================
 *  追蹤方向翻轉(只影響 TRACKING 模式,manual 不受影響)
 *  機構裝反、或想整體調轉時,把對應軸改成 -1 即可
 * ========================================================================= */
#define M1_TRACK_DIR                 (+1)    /* Motor1 (水平軸): +1 正常, -1 反向 */
#define M2_TRACK_DIR                 (+1)    /* Motor2 (垂直軸): +1 正常, -1 反向 */

/* =========================================================================
 *  控制 - Motor1 (緩和版,大約 Motor2 的一半)
 *  輸出 hz = kp × error × OUTPUT_GAIN × (pos_scale 或 neg_scale)
 * ========================================================================= */
#define M1_KP_SMALL   			     150.0f   // 原 90
#define M1_KP_MEDIUM  				 300.0f   // 原 180
#define M1_KP_LARGE   				 500.0f   // 原 310
#define M1_OUTPUT_GAIN               1.0f
#define M1_POS_SCALE                 1.10f
#define M1_NEG_SCALE                 1.24f
#define M1_MAX_STEP_HZ               60000U

/* =========================================================================
 *  控制 - Motor2 (原本數值)
 * ========================================================================= */
#define M2_KP_SMALL                  180.0f
#define M2_KP_MEDIUM                 360.0f
#define M2_KP_LARGE                  620.0f
#define M2_OUTPUT_GAIN               2.0f
#define M2_POS_SCALE                 1.02f
#define M2_NEG_SCALE                 1.16f
#define M2_MAX_STEP_HZ               60000U

/* =========================================================================
 *  串口
 * ========================================================================= */
#define SERIAL_CMD_RX_LINE_MAX       32U
#define SERIAL_CMD_QUEUE_LENGTH      4U

#ifdef __cplusplus
}
#endif

#endif /* APP_TRACKING_CONFIG_H */
