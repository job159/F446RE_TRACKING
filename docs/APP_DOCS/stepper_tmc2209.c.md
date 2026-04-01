# stepper_tmc2209.c — 單軸步進馬達驅動核心

來源: `Core/Src/App/stepper_tmc2209.c`

## 1. 角色

整個專案最接近硬體的 App 模組。負責：

- 透過 UART 寫入 TMC2209 寄存器（microstep、電流、StealthChop）
- 用 TIM PWM 產生 step pulse
- 控制 DIR / EN 腳位
- 變速與反轉時做 ramp
- 提供 8 檔 speed stage 與 signed step rate 兩種控制模式

---

## 2. TMC2209 寄存器設定

初始化時寫入 4 個寄存器：

| 寄存器 | 地址 | 值 | 說明 |
|--------|------|-----|------|
| `GCONF` | 0x00 | `PDN_DISABLE \| MSTEP_REG_SELECT` | 啟用 UART 控制 microstep |
| `IHOLD_IRUN` | 0x10 | hold=6, run=16, delay=4 | 靜止電流 / 運行電流 / 衰減延遲 |
| `CHOPCONF` | 0x6C | base=0x10000053, MRES=1/16 | **1/16 微步** + chopper 設定 |
| `PWMCONF` | 0x70 | 0xC10D0024 | StealthChop PWM 參數 |

**電流設定（IHOLD_IRUN）：**
- `IHOLD = 6` → 靜止電流約 6/31 × Vref
- `IRUN = 16` → 運行電流約 16/31 × Vref
- `IHOLDDELAY = 4` → 從 IRUN 降到 IHOLD 的過渡速度

**微步設定：**
- `MRES = 0x04` → 1/16 微步
- 若馬達是 200 步/圈，實際每圈 3200 微步

**UART 通訊格式：**
```
[SYNC=0x05] [SLAVE_ADDR] [REG|0x80] [DATA_MSB..LSB] [CRC8]
共 8 bytes，timeout 30ms
```

---

## 3. Step Pulse 產生方式

不是軟體 toggle GPIO，而是 **TIM PWM 自動輸出**：

```
目標: step_hz (例如 5000 Hz)

timer_clock = GetTimerClock()   // 自動判斷 APB1/APB2
counter_clock = timer_clock / (prescaler + 1)
ARR = counter_clock / step_hz - 1    // 最小 3
CCR = (ARR + 1) / 2                   // 約 50% duty

→ Timer 自動輸出穩定的 step pulse
```

**為什麼用 PWM：** 頻率穩定、不佔 CPU、不需要在主迴圈 bit-bang。

---

## 4. 反轉與 Ramp 處理

步進馬達瞬間變速或反轉容易失步。程式的處理：

### 4.1 同方向變速
```
RampStepFrequency(current_hz, target_hz)
  每次步進 TMC2209_RAMP_STEP_HZ = 800 Hz
  每步等待 TMC2209_RAMP_DELAY_MS = 1 ms
```

### 4.2 反轉（方向改變）
```
1. Ramp 降速到 direction_switch_hz (speed_table[0] 或 speed_table[4])
2. 切 DIR GPIO
3. 等待 TMC2209_DIR_SWITCH_SETTLE_MS = 2 ms
4. Ramp 升速到 target_hz
```

### 4.3 從停止啟動
```
1. 設 DIR
2. 等待 2ms settle
3. 直接 ApplyStepFrequency(target_hz)  // 不 ramp
```

### Ramp 參數

| 參數 | 值 | 說明 |
|------|-----|------|
| `TMC2209_RAMP_STEP_HZ` | 800 | 每步增減量 |
| `TMC2209_RAMP_DELAY_MS` | 1 | 每步等待時間 |
| `TMC2209_DIR_SWITCH_SETTLE_MS` | 2 | 反轉後穩定時間 |

**注意：** Ramp 用的是 `HAL_Delay()`（blocking），所以大幅變速會短暫佔用主迴圈。

---

## 5. 兩種控制模式

### 5.1 Speed Stage 模式（manual 用）

```
SetSpeedStage(stage)     // stage 0~7
  Stage 0~3: Forward 四檔
  Stage 4~7: Reverse 四檔
```

上層只需要說「第幾檔」，底層自動處理方向和速度。

### 5.2 Signed Step Rate 模式（tracking / search 用）

```
SetSignedStepRate(signed_step_hz)
  正值 → DIR_FORWARD + |value| Hz
  負值 → DIR_REVERSE + |value| Hz
  零值 → Stop()
  magnitude 上限 65535
```

---

## 6. 預設 Speed Table

```
Stage 0 (F1): 200 Hz     Forward 低速
Stage 1 (F2): 1400 Hz    Forward 中低速
Stage 2 (F3): 5000 Hz    Forward 中高速
Stage 3 (F4): 7500 Hz    Forward 高速
Stage 4 (R1): 200 Hz     Reverse 低速
Stage 5 (R2): 1400 Hz    Reverse 中低速
Stage 6 (R3): 5000 Hz    Reverse 中高速
Stage 7 (R4): 7500 Hz    Reverse 高速
```

Speed table 在 `motor_control.c` 的 `MotorControl_Init()` 定義，傳入 `StepperTmc2209_Init()`。

---

## 7. Stop 行為

```
Stop()
  CCR = 0        // 停止 PWM 輸出
  counter = 0
  generate update event
  current_step_hz = 0
```

不關 timer，只是把 compare 設 0 讓 PWM 不產生 pulse。

---

## 8. 初始化完整流程

```
StepperTmc2209_Init()
  ├── 保存 timer / UART / GPIO / slave_address
  ├── 載入 speed table
  ├── 設 DIR = FORWARD
  ├── 設 EN = DISABLE
  ├── HAL_Delay(2ms) — driver wakeup
  ├── 寫 GCONF
  ├── 寫 IHOLD_IRUN
  ├── 寫 CHOPCONF
  ├── 寫 PWMCONF
  ├── 設 EN = ENABLE
  ├── HAL_TIM_PWM_Start()
  └── SetSpeedStage(0) — 停在 F1 低速
```

---

## 9. 調適指南

### 想改微步

改 `TMC2209_CHOPCONF_MRES_1_16`：
| 值 | 微步 |
|----|------|
| 0x08 | 全步 (1/1) |
| 0x07 | 1/2 |
| 0x06 | 1/4 |
| 0x05 | 1/8 |
| 0x04 | 1/16（目前） |
| 0x03 | 1/32 |
| 0x02 | 1/64 |
| 0x01 | 1/128 |
| 0x00 | 1/256 |

微步越高越安靜、越平滑，但扭矩越小。

### 想改運行電流

改 `TMC2209_IRUN`（目前 16）：
- 範圍 0~31
- 越大電流越大、扭矩越大、發熱越多
- 16 約為 50% 最大電流

### 想改反轉手感

| 改什麼 | 效果 |
|--------|------|
| `TMC2209_RAMP_STEP_HZ` (800) | 加大→反轉更快但更可能失步 |
| `TMC2209_RAMP_DELAY_MS` (1) | 加大→反轉更平滑但更慢 |
| `TMC2209_DIR_SWITCH_SETTLE_MS` (2) | 加大→切方向更穩但延遲更大 |

### 想改 manual 檔位速度

在 `motor_control.c` 的 `speed_table[]` 裡改，不用動這個檔案。

---

## 10. 上下游關係

```
motor_control.c
    ↓ SetSpeedStage() / SetSignedStepRate()
stepper_tmc2209.c
    ↓
├── TIM PWM (step pulse)
├── DIR GPIO
├── EN GPIO
└── TMC2209 UART (寄存器寫入)
```

---

## 11. 踩雷提醒

1. **速度公式不取決於 main.c timer 初值** — 這個檔案會自己重算 ARR，timer prescaler 是唯一從 CubeMX 帶進來的
2. **stage 和 signed step rate 共用同一組狀態** — `current_step_hz / current_direction`，切換模式時要注意狀態一致
3. **DIR/EN 極性** — `DIR_FORWARD = GPIO_PIN_RESET`，`ENABLE = GPIO_PIN_RESET`（低有效）。若硬體相反要改 header 定義
4. **Ramp 是 blocking** — 大幅變速時會佔用主迴圈數十 ms
