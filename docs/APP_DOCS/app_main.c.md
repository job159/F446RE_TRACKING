# app_main.c — 系統中樞

來源: `Core/Src/App/app_main.c`

## 1. 角色

App 層主控中樞，負責：

- 初始化所有子模組
- 管理狀態機（IDLE / TRACKING / MANUAL）
- 以固定週期執行控制回圈
- 處理 UART 指令與 B1 按鈕
- 組裝 telemetry snapshot

`main.c` 只需要呼叫 `AppMain_Init()` 和 `AppMain_Task()`，不需要知道 App 層內部的任何細節。

---

## 2. 內部資料結構

`AppMain_Context_t g_app` 是全域唯一的系統狀態容器：

```c
typedef struct {
  AppAdc_HandleTypeDef          adc;
  AppEncoder_HandleTypeDef      encoder;
  SerialCmd_HandleTypeDef       serial;
  LdrTracking_HandleTypeDef     ldr;
  TrackerController_HandleTypeDef tracker;
  MotorControl_HandleTypeDef    motor;
  ManualControl_HandleTypeDef   manual;
  Telemetry_HandleTypeDef       telemetry;
  TelemetrySnapshot_t           snapshot;
  SystemMode_t                  mode;
  SystemMode_t                  requested_mode_after_calibration;
  IdleSubstate_t                idle_substate;
  uint32_t calibration_start_tick_ms;
  uint32_t control_period_ms;      // runtime 可調 (1/2/5 ms)
  uint32_t last_control_tick_ms;
  uint32_t last_button_tick_ms;
  uint8_t  requested_manual_stage;
  uint8_t  requested_manual_stage_valid;
  GPIO_PinState last_button_state;
} AppMain_Context_t;
```

---

## 3. 開機流程

```
AppMain_Init()
  ├── memset(&g_app, 0)
  ├── AppAdc_Init()           // 啟動 ADC DMA
  ├── AppEncoder_Init()       // 啟動 encoder timer
  ├── SerialCmd_Init()        // 初始化 UART parser
  ├── LdrTracking_Init()      // 初始化 LDR
  ├── LdrTracking_ForceRecalibration()  // 進入校正模式
  ├── TrackerController_Init()
  ├── ManualControl_Init()
  ├── Telemetry_Init()
  ├── MotorControl_Init()     // 初始化 TMC2209 (寫 UART 寄存器)
  │     ├── 寫 GCONF / IHOLD_IRUN / CHOPCONF / PWMCONF
  │     ├── 啟動 PWM
  │     └── StopAll()
  ├── mode = MODE_IDLE
  ├── idle_substate = IDLE_CALIBRATING
  ├── requested_mode_after_calibration = MODE_TRACKING
  ├── control_period_ms = 1
  └── 送出 "APP READY DUAL MODE" 或 "APP INIT ERROR"
```

開機預設進 IDLE + CALIBRATING，5 秒後自動進 TRACKING。
若校正期間收到 `MANUAL` 或 `MAN Fx` 指令，校正後改進 MANUAL。

---

## 4. 主迴圈排程

```
AppMain_Task()  ← main.c while(1) 每圈呼叫
  ├── now_ms = HAL_GetTick()
  ├── PollButton(now_ms)          // B1 按鈕 polling
  ├── SerialCmd_PollRx()          // UART 字元接收
  ├── AppAdc_Task()               // 更新 ADC 濾波值
  ├── AppEncoder_Task()           // 更新 encoder count
  ├── while(SerialCmd_Dequeue)    // 逐筆處理命令
  │     └── HandleCommand()
  ├── if (now_ms - last_control >= control_period_ms)  // 控制週期到了
  │     ├── RunControl(now_ms)    // ★ 核心控制回圈
  │     └── last_control = now_ms
  ├── UpdateSnapshot(now_ms)      // 組裝 telemetry 資料
  └── Telemetry_Task()            // 100ms 輸出一次
```

**重點：** ADC/Encoder 每圈都更新，但 RunControl 只在控制週期到了才執行。

---

## 5. 控制回圈 (`RunControl`)

```
RunControl(now_ms)
  ├── LdrTracking_UpdateFrame(filtered_adc1~4)  // 更新 LDR frame
  └── switch(mode)
        ├── IDLE      → RunIdle()
        ├── TRACKING  → RunTracking()
        ├── SEARCH    → 強制轉 TRACKING（legacy 相容）
        ├── MANUAL    → RunManual()
        └── default   → EnterIdleWait()
```

### RunIdle
```
RunIdle(now_ms)
  ├── MotorControl_StopAll()         // 確保馬達停止
  └── if CALIBRATING:
        ├── LdrTracking_AccumulateCalibration()
        └── if 時間到 5 秒:
              └── FinalizeCalibration()
                    ├── LdrTracking_FinalizeCalibration()
                    └── 依 requested_mode → 進 TRACKING 或 MANUAL
```

### RunTracking
```
RunTracking(now_ms)
  ├── if frame.is_valid == 0:      // 失追
  │     ├── TrackerController_Reset()
  │     └── MotorControl_StopAll()   // 直接停住
  │     (不進 SEARCH)
  └── else:                         // 有效追蹤
        ├── command = TrackerController_Run(frame, period_ms)
        └── MotorControl_ApplySignedStepHz(command)
```

### RunManual
```
RunManual()
  └── ManualControl_Task()           // 將 pending stage 套用到馬達
```

---

## 6. 指令處理

### `HandleCommand()` 命令分派

| 命令 | 行為 |
|------|------|
| `SERIAL_CMD_MODE_IDLE` | 停止馬達，進 IDLE_WAIT_CMD |
| `SERIAL_CMD_MODE_TRACKING` | 校正中→記錄延後；否則進 TRACKING |
| `SERIAL_CMD_MODE_MANUAL` | 校正中→記錄延後；否則進 MANUAL |
| `SERIAL_CMD_MANUAL_STAGE` | 呼叫 RequestManualStage() |
| `SERIAL_CMD_RECALIBRATE` | 重新校正（停馬達、重設控制器、5 秒） |
| `SERIAL_CMD_CONTROL_PERIOD` | 切換 1/2/5 ms 週期 |
| `SERIAL_CMD_STATUS_QUERY` | 回覆 `STATUS mode:... valid:... cmd:...` |
| `SERIAL_CMD_CAL_QUERY` | 回覆 `CALDATA base:... floor:... done:...` |
| `SERIAL_CMD_CONFIG_QUERY` | 回覆 `CONFIG period_ms:... deadband:...` |
| `SERIAL_CMD_HELP` | 回覆命令清單 |

### 校正期間的命令處理

校正期間收到 `TRACK` / `MANUAL` / `MAN Fx` → 不立刻執行，而是記錄在 `requested_mode_after_calibration`，校正完成後自動切入。

---

## 7. B1 按鈕行為

```
PollButton()
  ├── 讀 GPIO (每圈都讀)
  ├── 偵測下降沿 (SET → RESET)
  └── HandleButtonPress()
        ├── debounce 180ms
        ├── if 校正中: 將 stage 排進 pending (QUEUED)
        ├── if 已在 MANUAL: next_stage = (current + 1) % 8
        └── else: next_stage = 0, 切進 MANUAL
```

Stage 輪切順序：`F1→F2→F3→F4→R1→R2→R3→R4→F1...`

---

## 8. 模式切換函式

| 函式 | 行為 |
|------|------|
| `EnterIdleWait()` | StopAll → mode=IDLE, substate=WAIT_CMD |
| `StartCalibration()` | StopAll → Reset 控制器 → ForceRecalibration → 5 秒計時 |
| `EnterTracking()` | 需 calibration_done；StopAll → Reset 控制器 |
| `EnterManual()` | 需 calibration_done；StopAll → Reset 控制器 → 套用 pending stage |
| `FinalizeCalibration()` | FinalizeCalibration → 依 requested_mode 切換 |

---

## 9. Telemetry Snapshot 組裝

`UpdateSnapshot()` 每圈執行，從各模組 handle 收集當前狀態：

- `snapshot.mode / idle_substate / search_substate`
- `snapshot.calibration_done / source_valid`
- `snapshot.adc[4] / baseline[4] / delta[4]`
- `snapshot.total_light / contrast`
- `snapshot.error_x_x1000 / error_y_x1000`（float → int × 1000）
- `snapshot.enc1_count / enc2_count / angle_x10000`
- `snapshot.cmd_axis1_hz / cmd_axis2_hz`
- `snapshot.manual_stage / manual_stage_valid`

---

## 10. 常改區域

| 要改什麼 | 在哪裡改 |
|----------|----------|
| 模式切換規則 | `HandleCommand()`, `RunControl()` |
| 失追策略（恢復 search？） | `RunTracking()` |
| 週期設定 | `SetControlPeriod()`, `IsControlPeriodSupported()` |
| 狀態回覆格式 | `SendStatusSummary()`, `SendCalibrationSummary()`, `SendConfigSummary()` |
| 開機預設模式 | `AppMain_Init()` 裡的 `requested_mode_after_calibration` |
| 按鈕行為 | `HandleButtonPress()` |

## 11. 注意事項

- `MODE_SEARCH` enum 仍保留，但 `app_main` 若進入 SEARCH 會立刻轉成 TRACKING
- `search_strategy` 模組仍存在專案中，若要恢復需重新接回 `RunControl()`
- `HAL_UART_Transmit` 是 blocking，telemetry 和 status reply 都會短暫佔用主迴圈
