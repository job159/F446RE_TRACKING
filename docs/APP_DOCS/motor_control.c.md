# motor_control.c — 雙軸馬達轉接層

來源: `Core/Src/App/motor_control.c`

## 1. 角色

App 層控制邏輯與底層 stepper 驅動之間的轉接層。

上層（app_main / tracker_controller / manual_control）不需要知道哪個 GPIO 是哪軸的 DIR、哪個 UART 是哪軸的 TMC。這個模組把「雙軸步進控制」封裝成高階 API。

---

## 2. 硬體接腳配置

定義在此檔案內部：

| 功能 | GPIO | 說明 |
|------|------|------|
| Axis1 DIR | PC6 | 方向控制 |
| Axis1 EN | PB8 | 使能控制 |
| Axis2 DIR | PC8 | 方向控制 |
| Axis2 EN | PC9 | 使能控制 |
| Axis1 TMC UART | huart_tmc_1 | 由 main.c 傳入 |
| Axis2 TMC UART | huart_tmc_2 | 由 main.c 傳入 |
| Axis1 STEP | htim_step_1, CH1 | PWM |
| Axis2 STEP | htim_step_2, CH1 | PWM |
| TMC slave address | 都是 0x00 | |

---

## 3. Speed Table（手動八檔）

```c
static const uint16_t speed_table[8] = {
    200,  1400,  5000,  7500,    // Stage 0~3: Forward F1~F4
    200,  1400,  5000,  7500     // Stage 4~7: Reverse R1~R4
};
```

| Stage | 命令 | 方向 | 步頻 Hz | 對應速度 |
|-------|------|------|---------|---------|
| 0 | F1 | Forward | 200 | 極慢（微調用） |
| 1 | F2 | Forward | 1400 | 慢速 |
| 2 | F3 | Forward | 5000 | 中速 |
| 3 | F4 | Forward | 7500 | 快速 |
| 4 | R1 | Reverse | 200 | 極慢反轉 |
| 5 | R2 | Reverse | 1400 | 慢速反轉 |
| 6 | R3 | Reverse | 5000 | 中速反轉 |
| 7 | R4 | Reverse | 7500 | 快速反轉 |

**注意：** 目前 manual stage 是「同一個 stage 同時套到兩軸」，不是每軸獨立控制。

---

## 4. 主要 API

### `MotorControl_Init()`
```
初始化兩軸:
  ├── 載入 speed table
  ├── StepperTmc2209_Init(axis1, ...)
  ├── StepperTmc2209_Init(axis2, ...)
  └── StopAll()
```

### `MotorControl_StopAll()`
```
兩軸都 Stop + last_command 歸零
```

### `MotorControl_ApplySignedStepHz(command)` — tracking/search 用
```
axis1 → SetSignedStepRate(command.axis1_step_hz)
axis2 → SetSignedStepRate(command.axis2_step_hz)
成功後更新 last_motion_command
```

### `MotorControl_SetManualStage(stage)` — manual 用
```
axis1 → SetSpeedStage(stage)
axis2 → SetSpeedStage(stage)
成功後更新 manual_stage + last_motion_command
```

### 查詢 API
- `IsManualStageValid()` — manual stage 是否有效
- `GetManualStage()` — 目前 manual stage

---

## 5. `StageToSignedHz()` helper

把 stage index 轉回 signed step rate（給 telemetry 用）：
```
stage 0~3: +speed_hz[stage]   (正值=forward)
stage 4~7: -speed_hz[stage]   (負值=reverse)
```

---

## 6. 調適指南

### 想改 manual 檔位速度

直接改 `speed_table[]` 陣列。前 4 個是 forward，後 4 個是 reverse。

例如想讓 F4 更快：
```c
static const uint16_t speed_table[8] = {
    200, 1400, 5000, 10000,   // F4 改成 10000
    200, 1400, 5000, 10000    // R4 也改
};
```

### 想讓正反不同速

前四格和後四格可以分開設：
```c
static const uint16_t speed_table[8] = {
    200, 1400, 5000, 7500,    // Forward
    300, 2000, 6000, 9000     // Reverse（更快）
};
```

### 想改成兩軸不同 stage

目前 `SetManualStage()` 對兩軸套同一個 stage。如果要每軸獨立，需要改這個函式的設計，可能需要：
- 拆成 `SetManualStageAxis1()` / `SetManualStageAxis2()`
- 或傳入兩個 stage 參數

---

## 7. 上下游關係

```
app_main.c ─────────────→ MotorControl_StopAll()
tracker_controller.c ──→ MotorControl_ApplySignedStepHz()
manual_control.c ──────→ MotorControl_SetManualStage()
                              ↓
                         stepper_tmc2209.c (axis1, axis2)
```

---

## 8. 踩雷提醒

1. **這層不該做模式決策** — 只負責「怎麼執行」，「何時追蹤/手動」是 app_main 的事
2. **tracking 和 manual 是兩條控制路徑** — tracking 走 `ApplySignedStepHz`，manual 走 `SetManualStage`，不要混用
3. **接腳配置寫死在 .c 裡** — 如果換板要改 `MOTOR_CTRL_AXIS*_*` 的 define
