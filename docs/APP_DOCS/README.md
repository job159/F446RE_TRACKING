# APP_DOCS — F446RE 太陽追蹤器 App 層完整文件

這個資料夾是 `Core/Inc/App` 與 `Core/Src/App` 的逐檔說明文件。

目標是讓你在不先讀完整個專案的情況下，也能知道：

- 系統整體架構與資料流向
- 每個檔案存在的目的與它在 App 層的位置
- 如何調適追蹤手感（PID、門檻、速度）
- 如何設計新功能或修改現有邏輯
- 哪些地方修改時最容易踩雷

---

## 系統架構總覽

```
┌─────────────────────────────────────────────────────┐
│                    main.c (HAL)                      │
│  提供: hadc1/2, htim_step/enc, huart_log/tmc        │
└───────────────────────┬─────────────────────────────┘
                        │ AppMain_Init() / AppMain_Task()
                        ▼
┌─────────────────────────────────────────────────────┐
│                  app_main.c  (系統中樞)               │
│  狀態機: IDLE ↔ TRACKING ↔ MANUAL                    │
│  排程: 1ms 控制週期 + 100ms telemetry                 │
│  指令: UART serial_cmd + B1 按鈕                      │
└──┬──────┬──────┬──────┬──────┬──────┬───────────────┘
   │      │      │      │      │      │
   ▼      ▼      ▼      ▼      ▼      ▼
┌─────┐┌─────┐┌──────┐┌───────┐┌──────┐┌──────────┐
│ ADC ││ ENC ││ LDR  ││Tracker││Motor ││Telemetry │
│app_ ││app_ ││track ││Contrl ││Contrl││          │
│adc.c││enc.c││ing.c ││er.c   ││.c    ││.c        │
└──┬──┘└──┬──┘└──┬───┘└──┬────┘└──┬───┘└──────────┘
   │      │      │       │        │
   │      │      │       │        ▼
   │      │      │       │  ┌───────────┐
   │      │      │       │  │stepper_   │
   │      │      │       │  │tmc2209.c  │
   │      │      │       │  └─────┬─────┘
   ▼      ▼      ▼       ▼        ▼
 [ADC   [TIM   [LDR   [PID     [TMC2209
  DMA]   ENC]  Frame]  Output]   UART+PWM]
```

### 資料流向（一個控制週期內）

```
ADC DMA → app_adc (濾波) → ldr_tracking (校正+誤差計算)
                                    ↓
                          LdrTrackingFrame_t {error_x, error_y, is_valid}
                                    ↓
                          tracker_controller (PID + gain scheduling)
                                    ↓
                          MotionCommand_t {axis1_step_hz, axis2_step_hz}
                                    ↓
                          motor_control → stepper_tmc2209 (PWM + DIR)
```

---

## 狀態機

```
         開機
          │
          ▼
    ┌──────────┐   5 秒校正完成     ┌───────────┐
    │   IDLE   │ ─────────────────→ │ TRACKING  │
    │CALIBRATE │                    │           │
    └──────────┘                    └─────┬─────┘
          │                               │
     UART/Button                     失追 → 停住
          │                          (不進 SEARCH)
          ▼                               │
    ┌──────────┐                          │
    │   IDLE   │ ←────────────────────────┘
    │ WAIT_CMD │          IDLE 指令
    └────┬─────┘
         │ MANUAL / MAN Fx / MAN Rx
         ▼
    ┌──────────┐
    │  MANUAL  │  F1→F2→F3→F4→R1→R2→R3→R4→F1...
    └──────────┘
```

- `MODE_SEARCH` enum 仍保留在型別中，但 `app_main` 已不主動進入
- 失追時直接 `TrackerController_Reset()` + `MotorControl_StopAll()`

---

## 建議閱讀順序

如果你是第一次接這份專案，建議先讀：

| 順序 | 檔案 | 重點 |
|------|------|------|
| 1 | `tracking_types.h.md` | 所有共用 struct / enum — 先知道資料長什麼樣 |
| 2 | `tracking_config.h.md` | 所有可調參數 — **調適的起點** |
| 3 | `app_main.c.md` | 狀態機、排程、指令處理 |
| 4 | `app_adc.c.md` | ADC DMA → 4 路邏輯通道 + 濾波 |
| 5 | `ldr_tracking.c.md` | 校正、誤差計算、有效性判斷 |
| 6 | `tracker_controller.c.md` | **PID 控制器 — 調適核心** |
| 7 | `motor_control.c.md` | 雙軸轉接 + speed table |
| 8 | `stepper_tmc2209.c.md` | TMC2209 UART 寫入 + PWM step |
| 9 | `app_encoder.c.md` | 累積 count + 角度換算 |
| 10 | `serial_cmd.c.md` | UART 命令解析 |
| 11 | `telemetry.c.md` | 觀測輸出格式 |

這樣會建立「共用資料 → 參數 → 主流程 → 感測 → 判斷 → 控制 → 驅動 → 輸出」的完整脈絡。

---

## 常見調適場景速查

### 場景 1：追蹤太慢，追不上光源

| 優先調 | 參數 | 目前值 | 調法 |
|--------|------|--------|------|
| ★★★ | `CTRL_AXIS*_OUTPUT_GAIN` | 2.0 | 加大（例如 3.0） |
| ★★ | `CTRL_KP_LARGE` | 620.0 | 加大 |
| ★★ | `CTRL_AXIS*_MAX_STEP_HZ` | 60000 | 要跟著 gain 一起拉高 |
| ★ | `CTRL_AXIS*_RATE_LIMIT_STEP_HZ` | 16250/13750 | 加大，讓加速更快 |

### 場景 2：追蹤時抖動（振盪）

| 優先調 | 參數 | 目前值 | 調法 |
|--------|------|--------|------|
| ★★★ | `CTRL_ERR_DEADBAND` | 0.015 | 加大（例如 0.03） |
| ★★ | `CTRL_KP_SMALL` | 180.0 | 降低 |
| ★★ | `CTRL_KD` | 18.0 | 降低（減少微分放大雜訊） |
| ★ | `CTRL_AXIS*_RATE_LIMIT_STEP_HZ` | 16250/13750 | 降低，讓速度變化更平滑 |
| ★ | ADC 濾波權重 | 10% prev / 90% new | 提高 prev 比重讓感測更穩 |

### 場景 3：有光但判定為「無效」（is_valid = 0）

| 優先調 | 參數 | 目前值 | 調法 |
|--------|------|--------|------|
| ★★★ | `TRACK_VALID_TOTAL_MIN` | 140 | 降低門檻 |
| ★★ | `TRACK_DIRECTION_CONTRAST_MIN` | 28 | 降低門檻 |
| ★ | `LDR_BASELINE_MARGIN` | 10 | 降低（讓更多光通過校正基線） |
| ★ | `LDR_MIN_NOISE_FLOOR` | 6 | 降低 |

### 場景 4：追蹤方向反了

1. 先檢查 `CTRL_AXIS*_ERROR_SIGN`（目前都是 `1.0f`）→ 改成 `-1.0f` 反轉該軸
2. 若只有 LDR 象限對不上，看 `ldr_tracking.c` 的 `LDR_IDX_*` 映射
3. 若馬達正反轉反了，看 `stepper_tmc2209.h` 的 `DIR_FORWARD / DIR_REVERSE` 極性

### 場景 5：想改控制週期

```
UART 送: PERIOD 2MS    (切成 2ms)
UART 送: PERIOD 5MS    (切成 5ms)
UART 送: PERIOD 1MS    (切回預設 1ms)
```

注意：週期越長，微分項越鈍、積分累積越大，手感會明顯不同。

---

## 文件分組

### 1. 系統中樞
- `app_main.c.md` / `app_main.h.md`

### 2. 感測輸入
- `app_adc.c.md` / `app_adc.h.md`
- `app_encoder.c.md` / `app_encoder.h.md`

### 3. 追光邏輯
- `ldr_tracking.c.md` / `ldr_tracking.h.md`
- `tracker_controller.c.md` / `tracker_controller.h.md`
- `search_strategy.c.md` / `search_strategy.h.md`（legacy，目前主流程不使用）

### 4. 馬達與驅動
- `motor_control.c.md` / `motor_control.h.md`
- `stepper_tmc2209.c.md` / `stepper_tmc2209.h.md`

### 5. 模式切換與控制輸入
- `serial_cmd.c.md` / `serial_cmd.h.md`
- `manual_control.c.md` / `manual_control.h.md`

### 6. 輸出與觀測
- `telemetry.c.md` / `telemetry.h.md`
- `uart_sequence.c.md` / `uart_sequence.h.md`（legacy）

### 7. 共用定義
- `tracking_types.h.md`
- `tracking_config.h.md`

---

## 硬體接腳速查

| 功能 | GPIO | 備註 |
|------|------|------|
| Axis1 DIR | PC6 | 方向 |
| Axis1 EN | PB8 | 使能（低有效） |
| Axis2 DIR | PC8 | 方向 |
| Axis2 EN | PC9 | 使能（低有效） |
| Axis1 STEP | TIM PWM CH1 | timer 由 main.c 傳入 |
| Axis2 STEP | TIM PWM CH1 | timer 由 main.c 傳入 |
| Encoder1 | TIM2 | x4 mode, 1000 pulse/rev |
| Encoder2 | TIM5 | x4 mode, 1000 pulse/rev |
| ADC1 | 2 ch scan+DMA | continuous |
| ADC2 | 2 ch scan+DMA | continuous |
| TMC2209 UART | UART4 / UART5 | slave addr 0x00 |
| Log UART | 同 serial_cmd | telemetry + 指令 |
| B1 按鈕 | B1_GPIO_Port/Pin | polling + 180ms debounce |

---

## UART 命令速查

### 主命令（建議使用）

| 命令 | 功能 |
|------|------|
| `IDLE` | 切到 IDLE (停止) |
| `TRACK` | 切到 TRACKING |
| `MANUAL` | 切到 MANUAL |
| `MAN 1`~`MAN 8` | 手動指定 stage（1-4=正轉, 5-8=反轉） |
| `MAN F1`~`F4` | 正轉 1~4 檔 |
| `MAN R1`~`R4` | 反轉 1~4 檔 |
| `PERIOD 1MS\|2MS\|5MS` | 切換控制週期 |
| `RECAL` | 重新校正（5 秒） |
| `STATUS` | 查詢目前狀態 |
| `CALDATA` | 查詢校正資料 |
| `CONFIG` | 查詢目前設定 |
| `HELP` | 列出可用命令 |

### 相容舊命令

| 舊命令 | 對應 |
|--------|------|
| `MODE 0` / `0` | IDLE |
| `MODE 1` / `1` | TRACK |
| `MODE 2` / `2` | MANUAL |
| `F1`~`F4`, `R1`~`R4` | MAN F1~R4 |
| `STAGE 0`~`7` | MAN 1~8 |
| `CTL 1\|2\|5` | PERIOD |
| `STAT?` | STATUS |
| `CAL?` | CALDATA |
| `CFG?` | CONFIG |

---

## 設計新功能的建議步驟

1. **定義資料格式** → 在 `tracking_types.h` 加 struct / enum
2. **設定可調參數** → 在 `tracking_config.h` 加 `#define`
3. **實作邏輯** → 在對應模組的 `.c` 實作
4. **接入主流程** → 在 `app_main.c` 的 `RunControl()` 或 `HandleCommand()` 呼叫
5. **加入觀測** → 在 `TelemetrySnapshot_t` 加欄位，`telemetry.c` 加格式化
6. **加入指令** → 在 `SerialCmdId_t` 加 enum，`serial_cmd.c` 加解析
