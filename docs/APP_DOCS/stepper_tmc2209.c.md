# stepper_tmc2209.c

來源: `Core/Src/App/stepper_tmc2209.c`

## 1. 這個檔案的角色

`stepper_tmc2209.c` 是整個專案最接近硬體的 App 模組。

它負責：

- 設定 TMC2209 UART 寄存器
- 用 PWM timer 產生 step pulse
- 控制 DIR / EN 腳位
- 在變速與反轉時做 ramp
- 提供 stage 模式與 signed step rate 模式

如果說 `motor_control.c` 是雙軸轉接層，那這個檔案就是單軸底層驅動核心。

## 2. 初始化流程

### `StepperTmc2209_Init()`

這個函式會：

1. 保存 timer / UART / GPIO / slave address
2. 載入 speed table
3. 設定預設方向與 disable 狀態
4. 等待 driver wakeup
5. 寫入 TMC2209 幾個重要寄存器
6. enable driver
7. 啟動 PWM
8. 切到 stage 0

### 寫入哪些寄存器

目前包含：

- `GCONF`
- `IHOLD_IRUN`
- `CHOPCONF`
- `PWMCONF`

這代表 microstep、電流與 PWM 模式等核心設定，都是在這裡決定。

## 3. step pulse 是怎麼產生的

不是用軟體一個一個 toggle GPIO，而是利用 timer PWM：

1. 根據目標 `step_hz` 算出 `ARR`
2. 再把 `CCR` 設成大約 50% duty
3. timer 自動輸出 pulse

這是比較適合 MCU 的方式，因為頻率穩定，也不用主迴圈忙著 bit-bang。

## 4. `StepperTmc2209_ApplyStepFrequency()`

這個函式是實際把「多少 Hz」變成 timer 設定值的地方。

流程是：

- 先計算 timer clock
- 再計算 counter clock
- 用目標 `step_hz` 推出 `ARR`
- 設定 compare 與 counter
- 觸發 update event

## 5. 為什麼有 ramp

步進馬達若瞬間從低速跳高速，或瞬間反轉，容易：

- 失步
- 抖動
- 機構衝擊變大

所以這個檔案在很多情況都會用：

- `StepperTmc2209_RampStepFrequency()`

逐步靠近目標速度。

## 6. 兩種控制模式

### 6.1 `SetSpeedStage()`

這是給 manual mode 用的。

上層只需要說：

- 我要第幾檔

這個檔案會自己從 speed table 找出：

- 方向
- 速度

### 6.2 `SetSignedStepRate()`

這是給 tracking / search 用的。

上層直接給：

- 正負號代表方向
- 絕對值代表 step_hz

這是比較細的控制接口。

## 7. 反轉時的處理

反轉不是直接切 `DIR` 就完事。

程式現在的做法是：

1. 先降到某個方向切換安全速度
2. 切 `DIR`
3. 等待 `TMC2209_DIR_SWITCH_SETTLE_MS`
4. 再 ramp 到目標速度

這能顯著降低反轉不穩的機率。

## 8. `current_step_hz` / `current_direction`

這兩個欄位很重要，因為：

- stage 模式要知道目前方向
- signed step 模式也要知道目前方向
- stop 後再啟動時要知道是不是從 0 開始

這使得同一個 stepper handle 能在不同控制模式之間切換而不混亂。

## 9. 和其他模組的關係

### 上游

- `motor_control.c`

### 下游

- TIM PWM
- UART4 / UART5 之類的 TMC UART
- DIR GPIO
- EN GPIO

## 10. 最常改的地方

### 想改 microstep

看：

- `TMC2209_CHOPCONF_*`

### 想改電流

看：

- `TMC2209_IHOLD`
- `TMC2209_IRUN`
- `TMC2209_IHOLDDELAY`

### 想改反轉手感

看：

- `TMC2209_RAMP_STEP_HZ`
- `TMC2209_RAMP_DELAY_MS`
- `TMC2209_DIR_SWITCH_SETTLE_MS`
- `motor_control.c` 的 speed table

## 11. 修改時容易踩雷的地方

### 11.1 速度公式和手感不只取決於 timer

很多人會先去改 `main.c` timer 初值，但真正跑起來時的頻率其實主要是這個檔案重算的。

### 11.2 stage 與 signed step rate 兩條路都會用到同一組狀態

如果只改一邊、不改另一邊，很容易出現某個模式正常、另一個模式怪怪的情況。

### 11.3 硬體極性很重要

`DIR_FORWARD / DIR_REVERSE`、`ENABLE / DISABLE` 的定義若和硬體相反，會整體顛倒。這時不應該只在上層亂乘負號。
