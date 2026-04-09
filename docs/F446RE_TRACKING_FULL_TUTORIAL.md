# STM32F446RE 光源追蹤系統 — 完整設計教學

> **硬體平台**：STM32F446RE Nucleo-64
> **開發環境**：STM32CubeIDE + HAL Library
> **馬達驅動**：TMC2209 (UART 模式) × 2
> **編碼器**：1000 pulse/rev 磁編碼器 × 2
> **感測器**：LDR 光敏電阻 × 4 (四象限排列)

---

## 目錄

1. [系統總覽](#1-系統總覽)
2. [時脈樹 (Clock Tree)](#2-時脈樹-clock-tree)
3. [周邊配置對應表](#3-周邊配置對應表)
4. [ADC + DMA 設計](#4-adc--dma-設計)
5. [低通濾波器](#5-低通濾波器)
6. [LDR 四象限追蹤演算法](#6-ldr-四象限追蹤演算法)
7. [校正流程 (Calibration)](#7-校正流程-calibration)
8. [PID 控制器設計](#8-pid-控制器設計)
9. [TMC2209 UART 協議與暫存器設定](#9-tmc2209-uart-協議與暫存器設定)
10. [步進馬達控制邏輯](#10-步進馬達控制邏輯)
11. [Encoder 編碼器讀取](#11-encoder-編碼器讀取)
12. [搜尋策略 (Search Strategy)](#12-搜尋策略-search-strategy)
13. [狀態機設計](#13-狀態機設計)
14. [串口指令系統](#14-串口指令系統)
15. [遙測 (Telemetry)](#15-遙測-telemetry)
16. [程式碼架構總覽](#16-程式碼架構總覽)
17. [調參指南](#17-調參指南)

---

## 1. 系統總覽

### 1.1 系統做什麼？

這套系統用 4 個光敏電阻 (LDR) 感測光源方向，透過 PID 控制器計算出兩軸步進馬達的轉速指令，讓機構持續追蹤最亮的光源。

整個控制流程是：

```
LDR × 4 ──> ADC (DMA) ──> 低通濾波 ──> 誤差計算 ──> PID ──> 步進馬達 Hz 指令
                                                                    │
                                                               TMC2209 UART
                                                                    │
                                                              TIM PWM (step脈衝)
```

### 1.2 主迴圈架構

`main.c` 裡面只做兩件事：

```c
AppMain_Init(...);    // 初始化所有模組
while (1) {
    AppMain_Task();   // 每圈執行一次
}
```

`AppMain_Task()` 每圈的流程：

1. **讀按鈕** — 偵測 B1 按鈕的下降沿
2. **讀串口** — polling 方式收 UART 字元，湊成一行後解析指令
3. **更新 ADC** — 從 DMA buffer 讀出最新值，做低通濾波
4. **更新 Encoder** — 讀 TIM counter 差值，累加到位移
5. **處理指令** — 依序取出指令 queue 裡的命令
6. **跑控制** — 依照當前模式，跑 IDLE/TRACKING/MANUAL/SEARCH 的邏輯
7. **更新遙測** — 把系統狀態打包成快照，定時透過 UART 送出

控制迴圈的週期由 `ctrl_period_ms` 決定，預設 1ms。

---

## 2. 時脈樹 (Clock Tree)

### 2.1 為什麼要搞懂時脈？

因為 **Timer 的頻率**、**ADC 的取樣率**、**UART 的 Baud Rate** 全部都從時脈算出來。搞錯時脈，馬達轉速就是錯的。

### 2.2 本專案的時脈設定

在 `SystemClock_Config()` 裡面可以看到：

```
HSI (16 MHz)  ──>  PLL  ──>  SYSCLK = 84 MHz
                    │
                    ├── PLLM = 16  → VCO input = 16/16 = 1 MHz
                    ├── PLLN = 336 → VCO output = 1 × 336 = 336 MHz
                    └── PLLP = 4   → SYSCLK = 336/4 = 84 MHz
```

> **注意**：使用 `PWR_REGULATOR_VOLTAGE_SCALE3`，所以最大 SYSCLK = 120 MHz。84 MHz 在安全範圍內。

匯流排分頻：

| 匯流排 | 分頻 | 頻率 | Timer 倍頻 | Timer 時脈 |
|--------|------|------|-----------|-----------|
| AHB    | /1   | 84 MHz | -       | -         |
| APB1   | /2   | 42 MHz | ×2      | **84 MHz** |
| APB2   | /1   | 84 MHz | ×1      | **84 MHz** |

**關鍵重點**：APB1 分頻不是 /1，所以掛在 APB1 上的 Timer 時脈會自動乘 2，最終還是 84 MHz。這代表不管是 TIM1(APB2) 還是 TIM3(APB1)，Timer 時脈都是 84 MHz。

### 2.3 時脈確認方法

程式裡 `stepper_tmc2209.c` 的 `get_timer_clock()` 就是在做這件事：

```c
static uint32_t get_timer_clock(const TIM_HandleTypeDef *htim)
{
  if (is_apb2_timer(htim->Instance))
  {
    uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE2) == RCC_CFGR_PPRE2_DIV1) return pclk2;
    return pclk2 * 2;
  }
  uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
  if ((RCC->CFGR & RCC_CFGR_PPRE1) == RCC_CFGR_PPRE1_DIV1) return pclk1;
  return pclk1 * 2;
}
```

**思路**：ARM 的規則是，如果 APBx 分頻 > 1，Timer 時脈就是 APBx × 2。上面這段程式碼直接讀 RCC 暫存器來判斷。

---

## 3. 周邊配置對應表

| 周邊 | 用途 | 關鍵設定 |
|------|------|---------|
| **TIM1** | 軸1 step 脈衝 | APB2, PSC=83, PWM CH1 |
| **TIM3** | 軸2 step 脈衝 | APB1, PSC=83, PWM CH1 |
| **TIM2** | 編碼器1 | 32-bit, Encoder TI1, Period=4294967295 |
| **TIM5** | 編碼器2 | 32-bit, Encoder TI1, Period=4294967295 |
| **ADC1** | LDR CH13(rank1), CH12(rank2) | 12-bit, Scan, Continuous, DMA Circular |
| **ADC2** | LDR CH14(rank1), CH11(rank2) | 12-bit, Scan, Continuous, DMA Circular |
| **UART4** | TMC2209 軸1 (UART) | 115200, 8N1 |
| **UART5** | TMC2209 軸2 (UART) | 115200, 8N1 |
| **USART2** | ST-Link VCP (串口指令 + 遙測) | 115200, 8N1 |
| **USART1** | 備用 | 115200, 8N1 |
| **B1 Button** | 模式切換 / 手動 stage 循環 | GPIO Input |
| **PC6/PB8** | 軸1 DIR / EN | GPIO Output |
| **PC8/PC9** | 軸2 DIR / EN | GPIO Output |

### 3.1 TIM1 / TIM3 (step 脈衝) 頻率計算

```
Timer 時脈 = 84 MHz
PSC = 83
Counter clock = 84 MHz / (83+1) = 1 MHz = 1,000,000 Hz
```

要產生 f Hz 的 step 脈衝：

```
ARR = (Counter clock / f) - 1
CCR = (ARR + 1) / 2        ← 50% duty cycle
```

例如要 5000 Hz 的 step 脈衝：
```
ARR = 1,000,000 / 5000 - 1 = 199
CCR = 200 / 2 = 100
```

程式碼在 `set_step_freq()` 裡面做這件事：

```c
uint32_t arr = counter_clk / hz;
if (arr < 4) arr = 4;      // 最小週期保護
arr -= 1;
__HAL_TIM_SET_AUTORELOAD(h->htim_step, arr);
__HAL_TIM_SET_COMPARE(h->htim_step, h->step_channel, (arr + 1) / 2);
```

### 3.2 TIM2 / TIM5 (Encoder) 設定解說

```
Prescaler = 0     (不分頻，每個edge都算)
Period = 4294967295  (0xFFFFFFFF，32-bit全開)
EncoderMode = TI1  (只看 TI1 邊沿，配合TI2方向 → x2 計數，但我們的馬達是1000 pulse encoder)
```

> **為什麼 Period 設成最大值？**
>
> Encoder 模式下，counter 會自動往上或往下數。如果 Period 太小，counter 很快就會 wrap around，我們無法分辨是正轉還是溢出。設成 0xFFFFFFFF (32-bit max)，就像一個超大的計數器，正常使用不可能數滿。

> **為什麼用 TI1 模式而不是 TI12？**
>
> `TIM_ENCODERMODE_TI1` 表示只在 Channel 1 的邊沿觸發計數 (x2 解析度)。如果用 `TIM_ENCODERMODE_TI12` 就是兩個 channel 都觸發 (x4 解析度)。本專案用 TI1 模式，所以 1000 pulse 的 encoder 在程式中定義 `ENCODER_COUNTS_PER_REV = 4000`，代表每圈 4000 counts。

**累計角度的計算方法**：

```c
static uint32_t count_to_angle(int32_t count)
{
  int32_t norm = count % ENCODER_COUNTS_PER_REV;   // 取一圈內的餘數
  if (norm < 0) norm += ENCODER_COUNTS_PER_REV;    // 確保正數

  uint64_t deg = (uint64_t)(uint32_t)norm * 3600000ULL;  // × 360.0000 × 10000
  uint32_t angle = (uint32_t)((deg + ENCODER_COUNTS_PER_REV / 2) / ENCODER_COUNTS_PER_REV);

  if (angle >= 3600000UL) angle = 0;
  return angle;  // 單位: 0.0001 度, 範圍 0 ~ 3599999
}
```

**思路**：用整數運算避免 float，乘以 3600000 (= 360.0000 × 10000) 來保持小數精度，最後除以 4000 就得到角度。`+ ENCODER_COUNTS_PER_REV/2` 是四捨五入。

---

## 4. ADC + DMA 設計

### 4.1 為什麼用兩顆 ADC？

STM32F446RE 有 3 顆 ADC，每顆各自有獨立的轉換器。如果只用 1 顆 ADC 掃 4 個 channel，4 個 channel 是 **時間上交替** 轉換的，不是同時的。

用 2 顆 ADC 各掃 2 channel：

- **ADC1**：CH13 (rank1) + CH12 (rank2)
- **ADC2**：CH14 (rank1) + CH11 (rank2)

兩顆 ADC **同時各自跑** (各自 continuous mode)，所以相鄰的 LDR 值取樣時間更接近，追蹤精度更高。

### 4.2 DMA Circular 模式

```c
hadc1.Init.ContinuousConvMode = ENABLE;      // 自動連續轉換
hadc1.Init.DMAContinuousRequests = ENABLE;    // 每次轉換完就DMA搬
hadc1.Init.NbrOfConversion = 2;               // 掃2個channel
```

搭配 DMA circular mode：

```c
HAL_ADC_Start_DMA(hadc, (uint32_t *)buf, 2);
```

DMA 會自動把每次轉換結果填到 `buf[0]` 和 `buf[1]`，循環不停。程式只需要隨時去讀 buffer 就好，不需要任何中斷。

### 4.3 關閉 DMA 中斷

```c
__HAL_DMA_DISABLE_IT(hadc->DMA_Handle, DMA_IT_HT);   // 關半滿中斷
__HAL_DMA_DISABLE_IT(hadc->DMA_Handle, DMA_IT_TC);   // 關全滿中斷
```

**為什麼要關？** Continuous + DMA circular 模式下，ADC 不停轉換，DMA 不停搬，每搬完 2 個就觸發一次 TC 中斷。以 ADC 的速度 (幾 μs 一次)，中斷頻率非常高，反而浪費 CPU。我們的做法是 **polling**：主迴圈每圈自己去讀 DMA buffer，這樣完全不需要中斷。

### 4.4 通道對應

DMA buffer 填值的順序是按照 scan 的 rank 順序：

```
ADC1: buf[0] = CH13 (rank1), buf[1] = CH12 (rank2)
ADC2: buf[0] = CH14 (rank1), buf[1] = CH11 (rank2)
```

在 `AppAdc_Task()` 裡面重新排列成 4 路 LDR：

```c
raw[0] = dma_buf1[0];  // ADC1 CH13
raw[1] = dma_buf2[0];  // ADC2 CH14
raw[2] = dma_buf1[1];  // ADC1 CH12
raw[3] = dma_buf2[1];  // ADC2 CH11
```

對應到 LDR 的四象限位置（從正面看）：

```
    0=左上    1=右上
    3=左下    2=右下
```

---

## 5. 低通濾波器

### 5.1 為什麼需要？

ADC 原始值會有雜訊跳動，如果直接拿來算誤差，PID 輸出會不穩定、馬達會抖。低通濾波可以平滑掉高頻雜訊。

### 5.2 演算法

```c
filtered = (prev × 100 + sample × 900 + 500) / 1000
```

等效於指數移動平均 (EMA)，α = 0.9：

```
filtered = 0.1 × prev + 0.9 × sample
```

- `ADC_LPF_OLD_WEIGHT = 100` → 舊值佔 10%
- `ADC_LPF_NEW_WEIGHT = 900` → 新值佔 90%
- `ADC_LPF_SCALE = 1000` → 總比例 (千分比)
- `+ 500` → 四捨五入 (SCALE/2)

### 5.3 為什麼 90% 新值？

新值權重高 = 濾波器反應快，適合追蹤場景 (光源可能快速移動)。如果調成 50%/50%，平滑效果更好但反應會變慢。

### 5.4 第一筆特殊處理

```c
if (h->seeded[i] == 0) {
    h->filtered[i] = raw[i];   // 第一筆直接採用
    h->seeded[i] = 1;
} else {
    h->filtered[i] = low_pass(h->filtered[i], raw[i]);
}
```

因為 `filtered` 初始值是 0，如果第一筆就做濾波，會得到 `(0 × 100 + sample × 900) / 1000`，嚴重偏低。所以第一筆直接設成原始值。

---

## 6. LDR 四象限追蹤演算法

### 6.1 原理

4 個 LDR 排成十字形的四個象限：

```
        光源
         ↓
    ┌─────────┐
    │ 0    1  │   0=左上, 1=右上
    │ (左上)(右上)│
    │         │
    │ 3    2  │   3=左下, 2=右下
    │ (左下)(右下)│
    └─────────┘
```

當光源偏右時，右邊的 LDR 會比左邊亮 (ADC 值大)。用差值除以總和就能得到 -1 ~ +1 之間的歸一化誤差：

```
error_x = (右邊 - 左邊) / 總和 = ((delta[1]+delta[2]) - (delta[0]+delta[3])) / total
error_y = (上面 - 下面) / 總和 = ((delta[0]+delta[1]) - (delta[3]+delta[2])) / total
```

### 6.2 Delta 的計算

不是直接用 ADC 原始值，而是扣掉 **baseline** (遮光時的基準值) 和 **noise floor** (雜訊範圍)：

```c
floor = baseline[i] + noise_floor[i];
if (raw[i] > floor)
    delta[i] = raw[i] - floor;
else
    delta[i] = 0;
```

**思路**：
- `baseline` = 校正時測到的「無光源」ADC 平均值
- `noise_floor` = 校正期間 ADC 值的跳動範圍 + margin
- `floor = baseline + noise_floor` = 「超過這個才算是有光源照到」的門檻

這樣做的好處：
1. 消除環境光的影響 (被 baseline 扣掉)
2. 消除雜訊的假訊號 (被 noise_floor 吃掉)

### 6.3 有效性判定

不是所有情況都應該追蹤。需要兩個條件同時滿足：

```c
if (total >= TRACK_VALID_TOTAL_MIN &&        // 140: 至少有足夠的光
    contrast >= TRACK_DIRECTION_CONTRAST_MIN)  // 28: delta之間有足夠差異
{
    // 計算 error_x, error_y → is_valid = 1
}
```

- **total (總光量)** 太低 → 可能只是雜訊，不是真正的光源
- **contrast (最大delta - 最小delta)** 太低 → 光很均勻照在四個 LDR 上，分不出方向

---

## 7. 校正流程 (Calibration)

### 7.1 目的

測出「沒有光源時」每個 LDR 的 ADC 基準值 (baseline)。有了 baseline 才能計算 delta。

### 7.2 流程

系統開機後自動進入校正，持續 **5 秒** (`SYS_BOOT_CALIBRATION_MS = 5000`)。

每一圈控制迴圈都會呼叫 `LdrTracking_AccumulateCalibration()`：

```c
void LdrTracking_AccumulateCalibration(LdrTracking_HandleTypeDef *h)
{
  for (int i = 0; i < LDR_CHANNEL_COUNT; i++)
  {
    uint16_t v = h->frame.raw[i];
    h->cal_sum[i] += v;           // 累加

    // 追蹤最大最小值
    if (v < h->cal_min[i]) h->cal_min[i] = v;
    if (v > h->cal_max[i]) h->cal_max[i] = v;
  }
  h->cal_samples++;
}
```

5 秒後呼叫 `LdrTracking_FinalizeCalibration()`：

```c
baseline[i] = cal_sum[i] / cal_samples;        // 平均值
noise_floor[i] = max(span + margin, 最小值);    // span = max - min
```

其中：
- `LDR_BASELINE_MARGIN = 10` → span 再多加 10 的安全餘量
- `LDR_MIN_NOISE_FLOOR = 6` → noise floor 最少也要 6 (避免太靈敏)

### 7.3 重新校正

隨時可以透過串口指令 `RECAL` 或 `CAL` 觸發重新校正。也可以在執行期間按按鈕。

---

## 8. PID 控制器設計

### 8.1 整體架構

兩軸獨立 PID。每一軸的 PID 輸入是歸一化誤差 (-1 ~ +1)，輸出是步進馬達的 Hz (帶正負號)。

```
error → [PID] → output_hz → [rate limit] → [direction scale] → 馬達
```

### 8.2 可變 Kp (三段式)

固定 Kp 的問題：
- Kp 太大 → 小誤差時會震盪
- Kp 太小 → 大誤差時追得太慢

解法：依照誤差大小用不同的 Kp：

```c
static float pick_kp(float abs_err)
{
  if (abs_err <= 0.055)  return 180.0;   // 小誤差：輕輕追
  if (abs_err <= 0.140)  return 360.0;   // 中誤差：正常追
  return 620.0;                           // 大誤差：快速追
}
```

| 誤差範圍 | Kp | 意義 |
|---------|-----|------|
| ≤ 0.055 | 180 | 快對準了，慢慢修 |
| ≤ 0.140 | 360 | 偏了一些，正常速度追 |
| > 0.140 | 620 | 偏很多，全速追 |

### 8.3 死區 (Deadband)

```c
if (abs_e <= CTRL_ERR_DEADBAND)   // 0.015
{
    ax->integrator *= CTRL_INTEGRATOR_DECAY;  // 0.8
    ax->prev_output_hz = 0;
    return 0;   // 不動
}
```

**為什麼要死區？**
- 誤差 ≤ 0.015 表示已經幾乎對準了
- 如果還繼續修正，馬達會來回抖動 (jitter)
- 死區內讓 integrator 慢慢衰減 (`× 0.8`)，避免累積的積分值導致啟動時突然跳一下

### 8.4 積分器 (Ki) 設計

```c
if (abs_e <= CTRL_ERR_MEDIUM)     // 只在中小誤差時積分
    ax->integrator += error * CTRL_KI * dt;
```

**為什麼大誤差時不積分？**
- 大誤差通常是光源快速移動，Kp 已經很大了
- 如果同時還在積分，integrator 會迅速膨脹
- 等到追上的時候 integrator 太大，會嚴重 overshoot

只在「快對準但還差一點」的時候積分，就是為了消除穩態誤差。

### 8.5 微分器 (Kd) 設計

```c
float deriv = (error - ax->prev_error) / dt;
out = kp * error + ax->integrator + CTRL_KD * deriv;
```

- `CTRL_KD = 18.0`
- 微分項 = 誤差變化率 × Kd
- 作用：當誤差快速變化時「提前煞車」，抑制震盪

### 8.6 輸出增益與方向補償

```c
out *= gain;            // 軸增益 (2.0)

if (out >= 0)
    out *= pos_scale;   // 正轉補償
else
    out *= neg_scale;   // 反轉補償
```

**為什麼要方向補償？**

實際機構往往不對稱。例如：
- 受重力影響，往上轉比往下轉困難
- 齒輪間隙 (backlash) 在正反轉時不同

透過 `pos_scale` 和 `neg_scale` 來補償：

| 參數 | 軸1 | 軸2 |
|------|-----|-----|
| pos_scale | 1.10 | 1.02 |
| neg_scale | 1.24 | 1.16 |

軸1 反轉需要 1.24 倍的輸出才能達到跟正轉同樣的效果，代表反轉方向的摩擦力或負載更大。

### 8.7 速率限制 (Rate Limit)

```c
int32_t delta = hz - ax->prev_output_hz;
if (delta > rate_limit)
    hz = ax->prev_output_hz + rate_limit;
else if (delta < -rate_limit)
    hz = ax->prev_output_hz - rate_limit;
```

- 軸1: `CTRL_AXIS1_RATE_LIMIT_STEP_HZ = 16250`
- 軸2: `CTRL_AXIS2_RATE_LIMIT_STEP_HZ = 13750`

每個控制週期，輸出最多只能變化這麼多 Hz。避免 PID 突然跳到很大的值，導致馬達失步或機構受衝擊。

### 8.8 完整 PID 公式

```
output = (Kp × error + integrator + Kd × deriv) × gain × direction_scale
```

受到以下限制：
1. 死區：|error| ≤ 0.015 → output = 0
2. 限幅：|output| ≤ max_hz (60000)
3. 速率限制：|Δoutput| ≤ rate_limit

---

## 9. TMC2209 UART 協議與暫存器設定

### 9.1 TMC2209 簡介

TMC2209 是 Trinamic 的靜音步進馬達驅動IC。支援兩種控制方式：
- **STEP/DIR**：用 GPIO 控制方向，用 PWM 脈衝控制步數
- **UART**：透過單線 UART 配置內部暫存器

本專案同時用兩種：
- UART 配置暫存器 (電流、微步、靜音模式等)
- STEP/DIR 控制實際運動

### 9.2 UART 協議格式

TMC2209 UART 寫入框架為 8 bytes：

```
[SYNC] [ADDR] [REG|0x80] [DATA3] [DATA2] [DATA1] [DATA0] [CRC8]
```

| 位元組 | 值 | 說明 |
|--------|-----|------|
| SYNC | 0x05 | 固定同步字元 |
| ADDR | 0x00 | 從站地址 (本專案兩顆都用 0x00) |
| REG | reg \| 0x80 | 暫存器地址，bit7=1 代表寫入 |
| DATA[3:0] | 32-bit big-endian | 暫存器資料 |
| CRC8 | 計算值 | CRC-8，多項式 0x07 |

### 9.3 CRC8 計算

```c
static uint8_t calc_crc8(const uint8_t *data, uint8_t len)
{
  uint8_t crc = 0;
  for (uint8_t i = 0; i < len; i++)
  {
    uint8_t byte = data[i];
    for (uint8_t b = 0; b < 8; b++)
    {
      if (((crc >> 7) ^ (byte & 0x01)) != 0)
        crc = (crc << 1) ^ 0x07;
      else
        crc <<= 1;
      byte >>= 1;
    }
  }
  return crc;
}
```

**思路**：
- TMC2209 用的 CRC 多項式是 `0x07` (CRC-8/SMBUS 變體)
- 注意 bit 順序：`byte & 0x01` 取最低位，`byte >>= 1` 右移，代表 **LSB first**
- `(crc >> 7)` 取 CRC 最高位，跟資料 bit XOR
- 如果結果是 1，CRC 左移後 XOR 多項式

CRC 只計算前 7 bytes (不含 CRC 本身)，填到第 8 byte。

### 9.4 暫存器設定詳解

初始化時依序寫入 4 個暫存器：

#### GCONF (0x00) — 全域配置

```c
#define GCONF_VAL   ((1UL << 6) | (1UL << 7))
```

| Bit | 值 | 名稱 | 說明 |
|-----|---|------|------|
| 6 | 1 | pdn_disable | 禁用 PDN_UART pin 的自動省電功能，讓 UART 持續運作 |
| 7 | 1 | mstep_reg_select | 微步解析度由暫存器控制 (而非 MS1/MS2 pin) |

**為什麼要設？** 如果不設 `pdn_disable`，IC 會自動進入省電模式，UART 通訊會斷。不設 `mstep_reg_select`，微步就會被 PCB 上的 MS1/MS2 pin 決定，無法透過軟體改。

#### IHOLD_IRUN (0x10) — 電流設定

```c
#define IHOLD_IRUN_VAL   ((4UL << 16) | (16UL << 8) | 6UL)
```

| 欄位 | Bit 範圍 | 值 | 說明 |
|------|---------|---|------|
| IHOLDDELAY | [19:16] | 4 | 從 IRUN 衰減到 IHOLD 的延遲速度 |
| IRUN | [12:8] | 16 | 運轉電流 (0~31，約為 Imax × IRUN/32) |
| IHOLD | [4:0] | 6 | 靜止保持電流 (0~31) |

**思路**：
- `IRUN = 16` ≈ 50% 最大電流，適合中等扭矩需求
- `IHOLD = 6` ≈ 19% 最大電流，靜止時低電流省電、減少發熱
- `IHOLDDELAY = 4` → 停止後逐步降低電流，不會突然失力

#### CHOPCONF (0x6C) — 斬波器配置

```c
#define CHOPCONF_VAL   0x10000053UL

// 設定 1/16 微步
#define CHOPCONF_MRES_MASK   (0x0FUL << 24)
#define CHOPCONF_MRES_16     (0x04UL << 24)
#define CHOPCONF_FINAL       ((CHOPCONF_VAL & ~CHOPCONF_MRES_MASK) | CHOPCONF_MRES_16)
```

CHOPCONF 的 bit[27:24] 是 MRES (微步解析度)：

| MRES 值 | 微步 | 每步脈衝 |
|---------|------|---------|
| 0000 | 1/256 | 256 |
| 0001 | 1/128 | 128 |
| 0010 | 1/64 | 64 |
| 0011 | 1/32 | 32 |
| **0100** | **1/16** | **16** |
| 0101 | 1/8 | 8 |
| 0110 | 1/4 | 4 |
| 0111 | 1/2 | 2 |
| 1000 | full step | 1 |

本專案用 **1/16 微步**，所以 MRES = `0x04`。

其餘 bit 設定 (`0x53` 的低位部分)：
- `TOFF = 3` (bit[3:0]) → 斬波器 off time
- `HSTRT/HEND` → 斬波器的 hysteresis 設定
- 這些是 TMC2209 預設的 SpreadCycle 斬波參數

#### PWMCONF (0x70) — StealthChop 配置

```c
#define PWMCONF_VAL   0xC10D0024UL
```

StealthChop 是 TMC2209 的靜音模式，透過 PWM 斬波讓馬達運轉更安靜。這個值設定了自動調整的參數，包括：

- PWM 頻率
- 自動電流調整 (pwm_autoscale)
- PWM 梯度

> **實務上**：如果不需要靜音，可以把這個暫存器留預設值。本專案寫入這個值是為了確保 StealthChop 正確運作。

### 9.5 初始化順序

```c
static HAL_StatusTypeDef config_registers(const StepperTmc2209_HandleTypeDef *h)
{
  write_reg(h, REG_GCONF, GCONF_VAL);       HAL_Delay(1);
  write_reg(h, REG_IHOLD_IRUN, IHOLD_IRUN_VAL);  HAL_Delay(1);
  write_reg(h, REG_CHOPCONF, CHOPCONF_FINAL);      HAL_Delay(1);
  write_reg(h, REG_PWMCONF, PWMCONF_VAL);
}
```

每次寫入後 `HAL_Delay(1)` 等 1ms，讓 TMC2209 內部處理完畢。

完整初始化流程：
1. **Disable** driver (EN pin HIGH)
2. 設定 **DIR** pin
3. 等 2ms
4. 寫 4 個暫存器
5. **Enable** driver (EN pin LOW)
6. 啟動 TIM PWM
7. 設定初始速度 (stage 0)

---

## 10. 步進馬達控制邏輯

### 10.1 速度分級 (Speed Stage)

8 個速度級別，前 4 個正轉 (F1~F4)，後 4 個反轉 (R1~R4)：

```c
static const uint16_t speed_table[TMC_SPEED_STAGE_COUNT] = {
    200, 1400, 5000, 7500,    // F1 ~ F4 (正轉)
    200, 1400, 5000, 7500     // R1 ~ R4 (反轉)
};
```

| Stage | 名稱 | 方向 | 頻率 (Hz) |
|-------|------|------|----------|
| 0 | F1 | 正轉 | 200 |
| 1 | F2 | 正轉 | 1400 |
| 2 | F3 | 正轉 | 5000 |
| 3 | F4 | 正轉 | 7500 |
| 4 | R1 | 反轉 | 200 |
| 5 | R2 | 反轉 | 1400 |
| 6 | R3 | 反轉 | 5000 |
| 7 | R4 | 反轉 | 7500 |

`TMC_DIR_SPLIT_STAGE = 4`，stage < 4 是正轉，≥ 4 是反轉。

### 10.2 加減速 (Ramp)

直接突然改變步進頻率會造成失步。`ramp_freq()` 每次最多跳 `RAMP_STEP_HZ = 800` Hz，每跳一次等 `RAMP_DELAY_MS = 1` ms：

```c
while (cur != target)
{
    if (cur < target) {
        gap = min(target - cur, RAMP_STEP_HZ);
        cur += gap;
    } else {
        gap = min(cur - target, RAMP_STEP_HZ);
        cur -= gap;
    }
    set_step_freq(h, cur);
    if (cur != target) HAL_Delay(1);
}
```

例如從 200 Hz 加速到 5000 Hz：
```
200 → 1000 → 1800 → 2600 → 3400 → 4200 → 5000
共 6 步，耗時約 5ms
```

### 10.3 方向切換

切方向不能在高速時直接反轉，必須：

1. 先 ramp 降到 base 速度 (本方向的最低速 stage)
2. 切 DIR pin
3. 等 2ms (`DIR_SETTLE_MS`)
4. 再 ramp 到目標速度

```c
if (h->current_dir != want_dir) {
    ramp_freq(h, h->current_hz, base_hz);  // 先降速
    set_dir(h, want_dir);                    // 切方向
    HAL_Delay(DIR_SETTLE_MS);                // 等穩定
    ramp_freq(h, base_hz, want_hz);          // 再加速
}
```

### 10.4 帶正負號的 Hz 控制

追蹤模式用 `StepperTmc2209_SetSignedHz()`：

- `signed_hz > 0` → DIR = GPIO_PIN_RESET (正轉)
- `signed_hz < 0` → DIR = GPIO_PIN_SET (反轉)
- `signed_hz == 0` → 停止

這個函式的邏輯跟 `SetSpeedStage` 一樣：靜止→直接啟動；同方向→ramp；反方向→降速+切方向+加速。

### 10.5 停止

```c
HAL_StatusTypeDef StepperTmc2209_Stop(StepperTmc2209_HandleTypeDef *h)
{
  __HAL_TIM_SET_COMPARE(h->htim_step, h->step_channel, 0);  // CCR=0, 輸出恆LOW
  __HAL_TIM_SET_COUNTER(h->htim_step, 0);
  HAL_TIM_GenerateEvent(h->htim_step, TIM_EVENTSOURCE_UPDATE);
  h->current_hz = 0;
  return HAL_OK;
}
```

不是停 TIM，而是把 CCR 設成 0，讓 PWM 輸出維持 LOW。TIM 還是在跑，只是不會產生脈衝。這樣重新啟動時不需要再 `HAL_TIM_PWM_Start()`。

---

## 11. Encoder 編碼器讀取

### 11.1 原理

使用 TIM2 和 TIM5 的 **Encoder Mode**。Timer 硬體自動解碼 A/B 相訊號，正轉 counter 加、反轉 counter 減。軟體只需要讀 counter 值。

### 11.2 累計方式

```c
static void update_one(TIM_HandleTypeDef *htim, uint32_t *prev, int32_t *accum)
{
  uint32_t now = __HAL_TIM_GET_COUNTER(htim);
  *accum += (int32_t)(now - *prev);
  *prev = now;
}
```

**為什麼不直接讀 counter 當位移？**

因為 counter 是 32-bit unsigned，會 wrap around。但差值 `(now - prev)` 在 unsigned 減法下永遠正確（只要兩次讀取之間轉不超過 2^31 counts，這在實際情況下不可能發生）。

把差值累加到 `int32_t accum`，就得到從開機以來的總位移。

### 11.3 角度計算

見第 3.2 節的 `count_to_angle()` 說明。

---

## 12. 搜尋策略 (Search Strategy)

### 12.1 什麼時候需要搜尋？

當追蹤模式下光源消失 (`is_valid == 0`)，系統進入搜尋模式，嘗試重新找到光源。

> **注意**：目前程式碼中 `MODE_SEARCH` 狀態直接呼叫 `enter_tracking()` + `run_tracking()`，搜尋策略模組已完成但尚未在主迴圈中啟用。這是預留的擴充功能。

### 12.2 三階段搜尋

搜尋分三個 phase，依序嘗試：

#### Phase 1: HISTORY_BIAS (歷史偏向)

根據最近幾次追蹤成功時的馬達指令方向，繼續往那個方向移動。

```
bias_hz = avg(最近4筆有效歷史的指令)
如果 |bias_hz| < SEARCH_BIAS_STEP_HZ(900)，就用 900
```

每 100ms 一個 cycle，共跑 6 個 cycle (600ms)。

**思路**：光源消失通常是因為移動太快追丟了，往最後追蹤的方向繼續走，很可能就找回來。

#### Phase 2: REVISIT_LAST_GOOD (回到最後有效位置)

用 encoder 的位置資訊，嘗試把機構移回最後一次追到光源時的位置。

```c
if (enc1 < last_good_enc1 - 80)       // 離目標太遠（負方向）
    cmd.axis1_step_hz = 650;           // 正轉移回去
else if (enc1 > last_good_enc1 + 80)   // 離目標太遠（正方向）
    cmd.axis1_step_hz = -650;          // 反轉移回去
```

容差 = ±80 counts (`SEARCH_REVISIT_TOL_COUNTS`)。兩軸都到位 或 超過 1 秒就進 Phase 3。

**思路**：如果光源沒有移動，只是被暫時遮擋，回到原位就能找回來。

#### Phase 3: SWEEP_SCAN (掃描)

像灑水器一樣左右掃，偶數 phase 加 Y 軸移動：

```c
cmd.axis1_step_hz = sweep_dx * 750;
if (sweep_phase & 1)
    cmd.axis2_step_hz = sweep_dy * 420;
```

每 120ms 切一次方向，X 軸每次反向，Y 軸每兩次反向。

**思路**：如果前兩個 phase 都找不到，就全方位慢慢掃。Y 軸速度比 X 軸慢 (420 vs 750)，因為通常光源在水平面移動的機會比較大。

### 12.3 追蹤歷史環形緩衝

`TrackingHistory` 用環形 buffer 存最近 16 筆追蹤紀錄：

```c
typedef struct {
  TrackingHistoryEntry_t entries[16];  // 環形 buffer
  uint8_t head;                        // 下一個寫入位置
  uint8_t count;                       // 有效筆數
} TrackingHistory_HandleTypeDef;
```

每次追蹤成功就 push 一筆，記錄 encoder 位置、指令方向、光量等。搜尋策略從這裡取出歷史資訊。

---

## 13. 狀態機設計

### 13.1 四個主要模式

```
         ┌──────────────────────────────────┐
         │                                  │
  ┌──────▼──────┐     ┌──────────┐    ┌─────┴─────┐
  │    IDLE     │────▶│ TRACKING │───▶│  SEARCH   │
  │  (校正/等待) │     │  (追蹤)   │    │  (搜尋)   │
  └──────┬──────┘     └──────────┘    └───────────┘
         │
         │            ┌──────────┐
         └───────────▶│  MANUAL  │
                      │  (手動)   │
                      └──────────┘
```

### 13.2 IDLE 模式

兩個子狀態：
- **IDLE_CALIBRATING**：正在做 5 秒校正，馬達停止
- **IDLE_WAIT_CMD**：校正完成，等待使用者指令

校正完成後自動切到 `mode_after_cal` 指定的模式 (預設 TRACKING)。

### 13.3 TRACKING 模式

```c
static void run_tracking(void)
{
  if (!g.ldr.frame.is_valid) {
    TrackerController_Reset(&g.tracker);
    MotorControl_StopAll(&g.motor);
    return;       // 光源無效 → 停止
  }
  MotionCommand_t cmd = TrackerController_Run(&g.tracker, &g.ldr.frame, g.ctrl_period_ms);
  MotorControl_ApplyCommand(&g.motor, &cmd);
}
```

- 光源有效 → PID 計算 → 驅動馬達
- 光源無效 → reset PID 狀態 → 停止馬達

### 13.4 MANUAL 模式

按按鈕或串口指令設定一個 speed stage (F1~F4, R1~R4)，兩軸以相同速度和方向運轉。

`ManualControl_Task()` 負責把 pending stage 套用到馬達：

```c
void ManualControl_Task(ManualControl_HandleTypeDef *h, MotorControl_HandleTypeDef *motor)
{
  if (!h->pending_valid) return;
  if (MotorControl_SetManualStage(motor, h->pending_stage) == HAL_OK) {
    h->active_stage = h->pending_stage;
    h->active_valid = 1;
  }
  h->pending_valid = 0;
}
```

### 13.5 模式切換的安全處理

切換模式時一定要：
1. **停止馬達** (`MotorControl_StopAll`)
2. **Reset PID** (`TrackerController_Reset`)
3. **清除手動 stage** (`MotorControl_ClearManualStage`)

```c
static void enter_tracking(void)
{
  if (!g.ldr.frame.calibration_done) {
    g.mode_after_cal = MODE_TRACKING;   // 校正還沒完 → 排隊
    return;
  }
  MotorControl_StopAll(&g.motor);
  MotorControl_ClearManualStage(&g.motor);
  TrackerController_Reset(&g.tracker);
  g.req_stage_valid = 0;
  g.mode = MODE_TRACKING;
}
```

**如果校正還沒完成**，不能直接進入 TRACKING (因為沒有 baseline 數據)，而是設定 `mode_after_cal`，等校正完自動切過去。

---

## 14. 串口指令系統

### 14.1 設計思路

使用 USART2 (ST-Link VCP)，同一個 UART 做指令收發和遙測輸出。

**為什麼不用中斷收？** 用 polling 的 `UART_FLAG_RXNE` 更簡單。中斷版需要處理 buffer、flag、race condition，polling 版只需要在主迴圈裡讀就好。反正主迴圈跑得夠快 (1ms)，115200 baud 下 1ms 最多收 ~11 bytes，不會漏。

### 14.2 接收流程

```c
void SerialCmd_PollRx(SerialCmd_HandleTypeDef *h)
{
  // 1. 清錯誤 flag (ORE, NE, FE)
  // 2. 逐 byte 讀取
  while (__HAL_UART_GET_FLAG(h->huart, UART_FLAG_RXNE))
  {
    uint8_t ch = h->huart->Instance->DR & 0xFF;

    if (ch == '\r' || ch == '\n')
      parse_line(h, h->rx_buf, h->rx_len);   // 換行 → 解析
    else if (ch == '\b' || ch == 0x7F)
      h->rx_len--;                             // 退格
    else
      h->rx_buf[h->rx_len++] = ch;            // 存入 buffer
  }
}
```

**為什麼要清 ORE/NE/FE？** 如果 UART 發生 overrun (ORE)、noise (NE)、framing error (FE)，這些 flag 會「鎖住」UART，RXNE 不會再置位。清掉才能繼續收。

### 14.3 指令列表

所有指令不分大小寫，自動轉大寫：

| 指令 | 功能 | 別名 |
|------|------|------|
| `IDLE` | 切到 IDLE 模式 | `0`, `MODE 0` |
| `TRACK` | 切到追蹤模式 | `1`, `MODE 1` |
| `MANUAL` | 切到手動模式 | `2`, `MODE 2` |
| `MAN F2` | 手動 forward stage 2 | `F2` |
| `MAN R3` | 手動 reverse stage 3 | `R3` |
| `MAN 5` | 手動 stage 5 (= R1) | `STAGE 4` |
| `RECAL` | 重新校正 | `CAL` |
| `STATUS` | 查詢狀態 | `STAT?` |
| `CALDATA` | 查詢校正數據 | `CAL?` |
| `CONFIG` | 查詢系統配置 | `CFG?` |
| `PERIOD 5MS` | 設定控制週期 | `CTL 5MS` |
| `HELP` | 顯示說明 | - |

### 14.4 指令 Queue

```c
#define SERIAL_CMD_QUEUE_LENGTH  4

typedef struct {
  SerialCmd_t queue[4];
  uint8_t q_head, q_tail, q_count;
} SerialCmd_HandleTypeDef;
```

用環形 queue 緩衝指令。為什麼不直接執行？因為解析和執行在不同時機點：
- `SerialCmd_PollRx()` 在主迴圈最前面
- `handle_cmd()` 在中間
- 如果收到多行，可以排隊一個一個處理

---

## 15. 遙測 (Telemetry)

### 15.1 設計

每 100ms (`SYS_TELEMETRY_PERIOD_MS`) 送一行文字到 UART：

```
序號 mode:TRACK sub:- cal:1 valid:1 adc:1234,1245,1220,1210 base:100,102,98,101 d:1134,1143,1122,1109 err:25,-18 cmd:3200,-2800 enc:12345,-6789 stg:255
```

### 15.2 欄位說明

| 欄位 | 說明 |
|------|------|
| 序號 | 遞增計數 |
| mode | 當前模式 (IDLE/TRACK/SEARCH/MANUAL) |
| sub | 子狀態 (CAL/WAIT/HBIAS/REVISIT/SWEEP/-) |
| cal | 校正是否完成 (0/1) |
| valid | 光源是否有效 (0/1) |
| adc | 4 路 ADC 濾波值 |
| base | 4 路 baseline |
| d | 4 路 delta |
| err | X/Y 誤差 × 1000 |
| cmd | 兩軸指令 Hz |
| enc | 兩軸 encoder count |
| stg | 手動 stage (255=無效) |

### 15.3 實作方式

使用 `snprintf` 格式化到 buffer，再用 `HAL_UART_Transmit` 阻塞送出。

```c
void Telemetry_Task(Telemetry_HandleTypeDef *h, const TelemetrySnapshot_t *snap)
{
  uint32_t now = HAL_GetTick();
  if ((now - h->last_tick) < h->period_ms) return;  // 還沒到時間
  h->last_tick = now;

  char buf[256];
  int len = snprintf(buf, sizeof(buf), "...", ...);

  if (HAL_UART_Transmit(h->huart, buf, len, 30) == HAL_OK)
    h->seq++;   // 只有送成功才加序號
}
```

`TX_TIMEOUT_MS = 30` → 最多等 30ms。115200 baud 送 256 bytes 大約需要 22ms，30ms 夠用。

---

## 16. 程式碼架構總覽

### 16.1 檔案結構

```
Core/
├── Inc/App/
│   ├── tracking_types.h      ← 共用型別定義 (enum, struct)
│   ├── tracking_config.h     ← 可調參數集中管理
│   ├── app_main.h            ← 主程式進入點
│   ├── app_adc.h             ← ADC + DMA + 濾波
│   ├── app_encoder.h         ← 編碼器讀取
│   ├── ldr_tracking.h        ← LDR 四象限演算法
│   ├── tracker_controller.h  ← PID 控制器
│   ├── stepper_tmc2209.h     ← TMC2209 驅動
│   ├── motor_control.h       ← 雙軸馬達管理
│   ├── manual_control.h      ← 手動模式
│   ├── search_strategy.h     ← 搜尋策略
│   ├── serial_cmd.h          ← 串口指令解析
│   ├── telemetry.h           ← 遙測輸出
│   └── uart_sequence.h       ← 簡易序列輸出
│
├── Src/App/
│   ├── app_main.c            ← 狀態機 + 主迴圈
│   ├── app_adc.c             ← ADC 實作
│   ├── app_encoder.c         ← Encoder 實作
│   ├── ldr_tracking.c        ← LDR 校正/誤差計算
│   ├── tracker_controller.c  ← PID 實作
│   ├── stepper_tmc2209.c     ← TMC2209 UART + PWM 控制
│   ├── motor_control.c       ← 馬達初始化/Stage控制
│   ├── manual_control.c      ← 手動 stage 管理
│   ├── search_strategy.c     ← 三階段搜尋
│   ├── serial_cmd.c          ← 指令 parsing
│   ├── telemetry.c           ← 遙測格式化
│   └── uart_sequence.c       ← 序列輸出
│
└── Src/main.c                ← CubeMX 生成，只呼叫 AppMain_Init/Task
```

### 16.2 設計原則

1. **CubeMX 生成的 `main.c` 只做最基本的呼叫**，所有邏輯都在 `App/` 裡面。這樣 CubeMX 重新生成不會覆蓋邏輯。

2. **每個模組一個 handle struct**，像 HAL 一樣用 `XXX_HandleTypeDef` 風格。所有狀態都在 struct 裡，沒有散落的全域變數。

3. **tracking_config.h 集中管理所有可調參數**。要調參數只需要改一個檔案，不用到處找 magic number。

4. **tracking_types.h 定義共用型別**。避免頭檔之間互相 include 造成循環依賴。

5. **模組之間透過函式呼叫通訊**，不直接存取對方的內部資料。唯一的例外是 `g.ldr.frame` 被多處讀取，因為它是唯讀的感測資料。

### 16.3 資料流

```
                    ┌─────────────┐
ADC DMA ──────────▶│  AppAdc     │──▶ filtered[4]
                    └─────────────┘
                          │
                    ┌─────▼───────┐
                    │ LdrTracking │──▶ error_x, error_y, is_valid
                    └─────────────┘
                          │
              ┌───────────┼───────────┐
              │           │           │
        ┌─────▼─────┐ ┌──▼──────┐ ┌──▼──────────┐
        │ Tracker   │ │ Search  │ │ Manual      │
        │ Controller│ │ Strategy│ │ Control     │
        │ (PID)     │ │         │ │             │
        └─────┬─────┘ └────┬────┘ └──────┬──────┘
              │             │             │
              └──────┬──────┘             │
                     │                    │
              ┌──────▼──────┐             │
              │ Motor       │◀────────────┘
              │ Control     │
              └──────┬──────┘
                     │
              ┌──────▼──────┐
              │ Stepper     │──▶ TIM PWM (step)
              │ TMC2209     │──▶ UART (config)
              └─────────────┘──▶ GPIO (dir, en)
```

---

## 17. 調參指南

### 17.1 追蹤靈敏度

**問題**：追蹤太靈敏，雜訊就會亂跳；太不靈敏，光源稍微偏就追不到。

| 參數 | 作用 | 建議 |
|------|------|------|
| `TRACK_VALID_TOTAL_MIN` (140) | 最低光量門檻 | 環境光強 → 調高；微弱光源 → 調低 |
| `TRACK_DIRECTION_CONTRAST_MIN` (28) | 方向辨識門檻 | 光源很近 → 調高；光源很遠 → 調低 |
| `LDR_BASELINE_MARGIN` (10) | 校正安全餘量 | 環境不穩 → 調高 |
| `LDR_MIN_NOISE_FLOOR` (6) | 最低雜訊門檻 | ADC 很乾淨 → 可調低到 3~4 |

### 17.2 PID 調參

**步驟**：

1. 先把 `CTRL_KI` 和 `CTRL_KD` 設為 0，只用 P 控制
2. 調 `CTRL_KP_SMALL`：光源對準時，機構應該穩定不抖
3. 調 `CTRL_KP_LARGE`：光源快速移動時，機構能跟上
4. 加入 `CTRL_KD`：抑制震盪 (oscillation)
5. 加入 `CTRL_KI`：消除穩態偏移

| 參數 | 現值 | 效果 |
|------|------|------|
| `CTRL_ERR_DEADBAND` (0.015) | 加大 → 更穩但精度低 | 減小 → 更精準但可能抖 |
| `CTRL_KP_SMALL` (180) | 近距離精調 | 太大會震盪 |
| `CTRL_KP_LARGE` (620) | 遠距離快追 | 太大會失步 |
| `CTRL_KI` (8.0) | 消除穩態誤差 | 太大會 overshoot |
| `CTRL_KD` (18.0) | 抑制震盪 | 太大會反應遲鈍 |
| `CTRL_INTEGRATOR_DECAY` (0.8) | 死區內積分器衰減 | 接近 1.0 → 積分保留久 |

### 17.3 方向補償

如果機構在某個方向追蹤總是慢半拍：

```
CTRL_AXIS1_POS_SCALE = 1.10   // 正轉增益
CTRL_AXIS1_NEG_SCALE = 1.24   // 反轉增益
```

scale > 1 代表「這個方向需要更大的輸出」。測試方法：
1. 讓光源往正方向移動，觀察追蹤是否跟得上
2. 往反方向移動，觀察是否一樣快
3. 如果反方向慢，就增大 neg_scale

### 17.4 搜尋策略

| 參數 | 現值 | 調整方向 |
|------|------|---------|
| `SEARCH_BIAS_STEP_HZ` (900) | 歷史偏向的移動速度 | 太快可能衝過頭 |
| `SEARCH_HISTORY_BIAS_CYCLES` (6) | 偏向搜尋的次數 | 加大 → 搜尋更久 |
| `SEARCH_REVISIT_STEP_HZ` (650) | 回訪的移動速度 | 太快會定位不準 |
| `SEARCH_REVISIT_TOL_COUNTS` (80) | 回訪的容差 | 加大 → 容易「到達」但位置不精確 |
| `SEARCH_SWEEP_STEP_HZ` (750) | 掃描 X 軸速度 | 太快可能掃過光源 |
| `SEARCH_SWEEP_Y_STEP_HZ` (420) | 掃描 Y 軸速度 | 通常比 X 軸慢 |

### 17.5 低通濾波

```
ADC_LPF_OLD_WEIGHT = 100   (10%)
ADC_LPF_NEW_WEIGHT = 900   (90%)
```

- 想要更平滑 → 增大 OLD_WEIGHT (例如 300/700)
- 想要更快反應 → 增大 NEW_WEIGHT (例如 50/950)
- 兩者之和必須等於 `ADC_LPF_SCALE` (1000)

---

## 附錄 A：硬體接線對應

| STM32 Pin | 功能 | 對應模組 |
|-----------|------|---------|
| PA0 (TIM2_CH1) | Encoder 1 A相 | 軸1 編碼器 |
| PA1 (TIM2_CH2) | Encoder 1 B相 | 軸1 編碼器 |
| PA0/PA1 (TIM5) | Encoder 2 A/B | 軸2 編碼器 |
| PA8 (TIM1_CH1) | Step 脈衝 | 軸1 TMC2209 |
| PA6 (TIM3_CH1) | Step 脈衝 | 軸2 TMC2209 |
| PC6 | DIR | 軸1 方向 |
| PB8 | EN | 軸1 使能 |
| PC8 | DIR | 軸2 方向 |
| PC9 | EN | 軸2 使能 |
| PA2 (USART2_TX) | ST-Link VCP TX | 遙測 + 指令 |
| PA3 (USART2_RX) | ST-Link VCP RX | 遙測 + 指令 |
| UART4 TX/RX | TMC2209 UART | 軸1 驅動配置 |
| UART5 TX/RX | TMC2209 UART | 軸2 驅動配置 |
| ADC1 CH11/12/13, ADC2 CH14 | 類比輸入 | 4 路 LDR |
| PC13 (B1) | 按鈕 | 模式/stage 切換 |

## 附錄 B：常見問題

### Q: 馬達不轉？
1. 檢查 EN pin 是否 LOW (LOW = enable)
2. 檢查 UART4/5 的 TX 是否有連到 TMC2209 的 PDN_UART
3. 用示波器量 step pin 是否有 PWM 輸出
4. 送 `STATUS` 指令看 `cmd` 欄位是否有值

### Q: 追蹤一直抖動？
1. 送 `CALDATA` 看 baseline 和 noise floor 是否合理
2. 死區太小 → 增大 `CTRL_ERR_DEADBAND`
3. Kp 太大 → 減小 `CTRL_KP_SMALL`
4. 低通濾波不夠 → 增大 `ADC_LPF_OLD_WEIGHT`

### Q: 校正值不準？
1. 校正 5 秒期間必須**遮擋光源**
2. 如果環境光變化大，送 `RECAL` 重新校正
3. 增大 `LDR_BASELINE_MARGIN` 增加容忍度

### Q: 遙測輸出亂碼？
1. 確認 UART baud rate 是 115200
2. 確認終端機設定 8N1 (8 data bits, no parity, 1 stop bit)
3. 用 ST-Link 的 VCP (USART2)

---

> **最後**：這份文件對應的是 `Core/Src/App/` 和 `Core/Inc/App/` 中的程式碼。所有可調參數都在 `tracking_config.h` 裡面。修改前建議先讀懂對應的程式碼段落，再用串口遙測觀察效果。
