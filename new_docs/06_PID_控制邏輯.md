# 06 — 控制律：純比例 + 三段增益 + 雙層飽和

> **TL;DR**
> 本系統**不是標準 PID**，而是 **P-only** 控制：
> `死區 → 依 |err| 三段選 KP → out = kp × err × GAIN → 方向不對稱修正 → ±MAX_HZ clamp`。
> 沒有積分、沒有微分、沒有速率限制、沒有任何歷史狀態。
> 每個 cycle 都重新從「當下誤差」算起，天然不會擺盪。

程式碼：[tracker_controller.c](../Core/Src/App/tracker_controller.c)。
所有可調參數：[tracking_config.h](../Core/Inc/App/tracking_config.h)。

---

## 1. 為何不用完整 PID？

傳統 PID 適合「連續量、目標值固定」的系統。太陽追蹤場景有兩個特性讓 I 與 D 項反而會幫倒忙：

| PID 項 | 傳統用途 | 本系統為何不用 |
|---|---|---|
| **積分 I** | 消除穩態誤差 | 死區 (0.020) 以內本來就不動；LDR 雜訊讓積分慢慢飄，帶來假性偏移 |
| **微分 D** | 抑制振盪 | LDR error 高頻雜訊嚴重，微分會把雜訊放大成馬達抖動 |
| **速率限制** | 保護機構 | 馬達端已有 ramp (每 1 ms 最多 800 Hz)，不必再在 PID 端限速 |

所以整個控制律只保留 **P 項** + 保護層，乾淨、可預測、好調參。

---

## 2. 控制律完整流程（對應 `run_axis()`）

```
輸入: error (已被 saturate_err 夾到 ±0.7，見第 4 節)

┌─────────────────────────────────────────┐
│ if |error| ≤ PID_ERR_DEADBAND (0.020) : │  死區，馬達直接停
│     return 0                            │
└─────────────────────────────────────────┘
                │
                ▼
┌─────────────────────────────────────────┐
│ 依 |error| 選擇 kp：                     │
│   |err| ≤ 0.060 (SMALL)  → KP_SMALL     │
│   |err| ≤ 0.150 (MEDIUM) → KP_MEDIUM    │
│   else                   → KP_LARGE     │
└─────────────────────────────────────────┘
                │
                ▼
        out = kp × error × OUTPUT_GAIN
                │
                ▼
┌─────────────────────────────────────────┐
│ 方向不對稱修正：                          │
│   out ≥ 0 → out *= POS_SCALE             │
│   out < 0 → out *= NEG_SCALE             │
└─────────────────────────────────────────┘
                │
                ▼
        clamp(out, ±MAX_STEP_HZ)
                │
                ▼
            return (int32_t)out
```

對應程式碼（已摘去註解）：

```c
static int32_t run_axis(const AxisParams_t *p, float error)
{
  float abs_e = (error < 0) ? -error : error;
  if (abs_e <= PID_ERR_DEADBAND) return 0;

  float kp  = pick_kp(p, abs_e);
  float out = kp * error * p->output_gain;
  out *= (out >= 0) ? p->pos_scale : p->neg_scale;

  if (out >  (float)p->max_step_hz) out =  (float)p->max_step_hz;
  if (out < -(float)p->max_step_hz) out = -(float)p->max_step_hz;
  return (int32_t)out;
}
```

---

## 3. 三段 KP 的直覺

一個 KP 走天下會有兩難：KP 大時微小誤差下會抖；KP 小時大誤差下追得慢。三段式把誤差分成三個區間，各給一個增益：

| 區間 | |error| | 用意 | M1 KP | M2 KP |
|---|---|---|---|---|
| **死區** | ≤ 0.020 | 完全不動 | 0 | 0 |
| **SMALL** | 0.020 ~ 0.060 | 細修、慢慢靠 | 100 | 60 |
| **MEDIUM** | 0.060 ~ 0.150 | 中速追過去 | 400 | 280 |
| **LARGE** | > 0.150 | 大誤差快衝 | 800 | 560 |

門檻在 [tracking_config.h](../Core/Inc/App/tracking_config.h)：

```c
#define PID_ERR_DEADBAND   0.020f
#define PID_ERR_SMALL      0.060f
#define PID_ERR_MEDIUM     0.150f
```

---

## 4. 雙層誤差保護

### 4.1 total / contrast 門檻（算 error 之前）

見 [05_LDR_追蹤演算法.md](05_LDR_追蹤演算法.md#step-32--有效性判定)。若 total 或 contrast 不達門檻，直接 `error = 0, is_valid = 0`，馬達停。

### 4.2 誤差飽和 `TRACK_ERR_CAP`（進入 run_axis 之前）

陰影場景下，某側 LDR 讀 0、另一側爆亮，error 會逼近 ±1.0，造成馬達全速衝向「陰影邊緣」而非真實光源。`TrackerController_Run` 先夾制：

```c
ex = clamp(error_x, −TRACK_ERR_CAP, +TRACK_ERR_CAP);   // 0.7
ey = clamp(error_y, −TRACK_ERR_CAP, +TRACK_ERR_CAP);
```

這層是 4.1 的備援；兩層任一觸發都能保護機構。

---

## 5. 兩軸參數對照

[tracking_config.h](../Core/Inc/App/tracking_config.h)：

| 參數 | M1 (axis1) | M2 (axis2) | 說明 |
|---|---|---|---|
| `KP_SMALL`     | 100.0 | 60.0  | 小誤差增益 |
| `KP_MEDIUM`    | 400.0 | 280.0 | 中誤差增益 |
| `KP_LARGE`     | 800.0 | 560.0 | 大誤差增益 |
| `OUTPUT_GAIN`  | 1.0   | 1.0   | 整體輸出倍率 |
| `POS_SCALE`    | 1.10  | 1.02  | 正轉補償 |
| `NEG_SCALE`    | 1.24  | 1.16  | 反轉補償 |
| `MAX_STEP_HZ`  | 60000 | 60000 | 輸出上限 |
| `TRACK_DIR`    | +1    | +1    | ±1，反向可設 −1 |

**軸對應**（注意，這裡的「M1 / M2」是軟體軸名，不是機構軸）：

```c
/* tracker_controller.c 91-94 */
/* [軸對調] 硬體上 M1 = 垂直(tilt/上下), M2 = 水平(azimuth/左右)
 *          所以 error_y 驅動 axis1, error_x 驅動 axis2. */
cmd.axis1_step_hz = M1_TRACK_DIR * run_axis(&M1_PARAMS, ey);
cmd.axis2_step_hz = M2_TRACK_DIR * run_axis(&M2_PARAMS, ex);
```

若要反向某一軸：把 `M1_TRACK_DIR` 或 `M2_TRACK_DIR` 改成 −1 即可，Manual 不受影響。

---

## 6. POS_SCALE / NEG_SCALE 的用途

機構兩向阻力常不對稱（皮帶張力、重力偏心、齒隙方向性）。程式輸出同樣的 hz，正反向實際動到的角速度不同。這兩個倍率就是做事後補償：

```c
out *= (out >= 0) ? POS_SCALE : NEG_SCALE;
```

調校方法：
1. TRACKING 時觀察 `err:` 走向；若偏移速度單邊慢，該邊乘更大。
2. 由 1.00 往上 1.05、1.10、1.15 每次 5 % 的試。
3. 兩個值都 1.0 時表示機構很對稱（理想情況）。

---

## 7. 控制週期如何影響

`SYS_CONTROL_PERIOD_MS` 預設 5 ms（可用串口指令 `PERIOD 1|2|5` 動態改）。本系統是**純 P**，沒有 dt 項，所以週期大小**不會改變控制律本身**，只影響：

- 響應延遲（週期短 → 更快反應到光源變化）
- CPU 負擔（週期短 → ramp / UART 佔比大）
- 位置估算更新頻率（`axis_pos += hz × dt`）

實測 5 ms 是穩定甜區；1 ms 會讓 ramp 佔去大部分 CPU，button / UART 回應會變遲。

---

## 8. 調參順序

從最保守的設定開始，往上加：

1. **確認 ADC 反向、校正都對** → `STATUS` 看 `valid:1` 穩定。
2. **試最小 KP_SMALL**：例如 M2 改 40，看「微小偏移」時有沒有開始動。
3. **逐步加 KP_LARGE**：讓大誤差能快速收斂但不 overshoot。
4. **加 KP_MEDIUM**：通常取 `KP_SMALL × 4 ~ 5`。
5. **若機構兩向不對稱**：調 `POS_SCALE` / `NEG_SCALE`。
6. **若追到目標仍在死區邊緣抖**：放大 `PID_ERR_DEADBAND`（例如 0.020 → 0.030）。
7. **若超過 MAX_STEP_HZ 太常發生**：代表 KP 過大或 MAX_HZ 太小，兩者擇一處理。

### 即時調參流程

```
1. 改 tracking_config.h
2. rebuild & flash
3. 串口送 STATUS / RECAL 確認狀態
4. 手電筒模擬光源
5. 回到 1，反覆
```

---

## 9. 延伸（目前未實作）

如果後續需要穩態誤差修正，建議 **在 P 之上疊一層條件式 I**（只在 |error| 在死區～SMALL 之間時積，避免 windup），而不是改成標準 PID。原因是大誤差區的 KP 已經很大，再加 I 容易震盪。
