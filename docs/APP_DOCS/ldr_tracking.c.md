# ldr_tracking.c — 追光判斷核心

來源: `Core/Src/App/ldr_tracking.c`

## 1. 角色

把 4 路 ADC 轉成「控制器可以直接用的追光誤差」。

這是把「感測值」轉成「追光誤差」的關鍵轉換層。控制器不需要知道 ADC、校正、門檻的細節，只需要看 `LdrTrackingFrame_t`。

---

## 2. 輸入 / 輸出

### 輸入
```
adc1_value, adc2_value, adc3_value, adc4_value  (uint16_t, 濾波後)
```

### 輸出
```
LdrTrackingFrame_t frame
  ├── raw[4]             // 保存輸入值
  ├── baseline[4]        // 校正基線
  ├── noise_floor[4]     // 雜訊寬度
  ├── delta[4]           // 有效差值
  ├── total              // delta 總和
  ├── contrast           // max_delta - min_delta
  ├── error_x            // 歸一化左右誤差 [-1, +1]
  ├── error_y            // 歸一化上下誤差 [-1, +1]
  ├── is_valid           // 可否追蹤
  └── calibration_done   // 校正完成否
```

---

## 3. LDR 實體排列

```
         前方（光源方向）
    ┌──────────┬──────────┐
    │  1 (TL)  │  2 (TR)  │
    │  raw[0]  │  raw[1]  │
    ├──────────┼──────────┤
    │  4 (BL)  │  3 (BR)  │
    │  raw[3]  │  raw[2]  │
    └──────────┴──────────┘
```

順時鐘編號：1→2→3→4，對應 raw[0]→raw[1]→raw[2]→raw[3]。

**程式內的索引定義：**
```c
#define LDR_IDX_TOP_LEFT      0U
#define LDR_IDX_TOP_RIGHT     1U
#define LDR_IDX_BOTTOM_RIGHT  2U
#define LDR_IDX_BOTTOM_LEFT   3U
```

**如果 LDR 實體位置和這個不同，追光方向就會錯。**

---

## 4. 校正流程

### 4.1 強制重校正：`ForceRecalibration()`

清除所有校正資料：
```
calibration_sum[4] = 0
calibration_min[4] = 0xFFFF  (最大值)
calibration_max[4] = 0
calibration_samples = 0
calibration_done = 0
is_valid = 0
```

### 4.2 累積校正資料：`AccumulateCalibration()`

每個控制週期呼叫一次（校正期間）：
```
for each channel i:
    calibration_sum[i] += raw[i]
    calibration_min[i] = min(calibration_min[i], raw[i])
    calibration_max[i] = max(calibration_max[i], raw[i])
calibration_samples++
```

以 1ms 週期、5 秒校正為例：累積約 5000 個 sample。

### 4.3 完成校正：`FinalizeCalibration()`

```
for each channel i:
    baseline[i] = calibration_sum[i] / calibration_samples   // 平均值
    noise_span  = calibration_max[i] - calibration_min[i]    // 雜訊寬度
    noise_floor[i] = max(noise_span + LDR_BASELINE_MARGIN, LDR_MIN_NOISE_FLOOR)
    // LDR_BASELINE_MARGIN = 10, LDR_MIN_NOISE_FLOOR = 6

calibration_done = 1
Recompute()  // 立即用新 baseline 重算一次
```

**校正的意義：** 把環境背景亮度（日光、室內燈）扣掉，只對「明顯高於背景」的光做反應。

---

## 5. 追光計算：`Recompute()`

每次 `UpdateFrame()` 都會觸發，是核心數學。

### Step 1：計算 delta

```
for each channel i:
    effective_baseline = baseline[i] + noise_floor[i]
    effective_baseline = min(effective_baseline, 4095)  // 12-bit ADC 上限

    if calibration_done AND raw[i] > effective_baseline:
        delta[i] = raw[i] - effective_baseline
    else:
        delta[i] = 0
```

### Step 2：計算 total 與 contrast

```
total = delta[0] + delta[1] + delta[2] + delta[3]
contrast = max(delta) - min(delta)
```

### Step 3：計算誤差或判定無效

```
sum_left  = delta[TL] + delta[BL]    // 左側兩顆
sum_right = delta[TR] + delta[BR]    // 右側兩顆
sum_top   = delta[TL] + delta[TR]    // 上側兩顆
sum_bottom = delta[BL] + delta[BR]   // 下側兩顆

if calibration_done
   AND total >= TRACK_VALID_TOTAL_MIN (140)
   AND contrast >= TRACK_DIRECTION_CONTRAST_MIN (28):

    error_x = (sum_right - sum_left) / total    // 歸一化
    error_y = (sum_top - sum_bottom) / total     // 歸一化
    is_valid = 1
else:
    error_x = 0, error_y = 0
    is_valid = 0
```

### 數值範例

假設陽光在右上方：
```
raw:    [200, 500, 350, 150]   (TL=200, TR=500, BR=350, BL=150)
baseline: [120, 115, 118, 122]
noise_floor: [15, 14, 16, 13]
effective_baseline: [135, 129, 134, 135]
delta:  [65,  371, 216, 15]
total = 667
contrast = 371 - 15 = 356

sum_left = 65 + 15 = 80
sum_right = 371 + 216 = 587
sum_top = 65 + 371 = 436
sum_bottom = 15 + 216 = 231

error_x = (587 - 80) / 667 = +0.760   → 右側更亮
error_y = (436 - 231) / 667 = +0.307   → 上側更亮
is_valid = 1  (total=667 >= 140, contrast=356 >= 28)
```

---

## 6. 歸一化誤差的好處

`error_x / error_y` 除以 `total`，所以：

- 不同環境總亮度變化時，誤差尺度穩定
- 陰天和晴天，控制器行為接近
- 誤差範圍約在 [-1, +1]

---

## 7. 有效性判斷的設計意義

三個條件缺一不可：

| 條件 | 防止的問題 |
|------|----------|
| `calibration_done` | 開機還沒校正就追 |
| `total >= 140` | 太暗（可能只是雜訊） |
| `contrast >= 28` | 雖然亮但四顆差不多（沒有方向資訊） |

**太寬（門檻太低）：** 容易誤追雜光
**太窄（門檻太高）：** 有光但判定無效，無法追蹤

---

## 8. 調適指南

### 追不到弱光

```
降低 TRACK_VALID_TOTAL_MIN（例如 80）
降低 TRACK_DIRECTION_CONTRAST_MIN（例如 15）
降低 LDR_BASELINE_MARGIN（例如 5）
```

### 校正後背景扣除不乾淨（delta 一直有值）

```
提高 LDR_BASELINE_MARGIN（例如 20）
提高 LDR_MIN_NOISE_FLOOR（例如 12）
或者延長校正時間 SYS_BOOT_CALIBRATION_MS
```

### error 方向錯

1. 先確認 LDR 實體排列是否符合程式的 TL/TR/BR/BL
2. 若只是 X 反了 → 交換左右定義或改 `CTRL_AXIS1_ERROR_SIGN = -1.0`
3. 若只是 Y 反了 → 改 `CTRL_AXIS2_ERROR_SIGN = -1.0`
4. 若象限完全亂了 → 改 `LDR_IDX_*` 的定義

### 確認校正品質

送 `CALDATA` → 看 baseline 和 noise_floor：
```
CALDATA base:380,375,382,377 floor:15,14,16,13 done:1
```
- baseline 四顆應該接近（差距 < 50 表示 LDR 品質一致）
- noise_floor 通常 10~30，太大可能是接線問題

---

## 9. 上下游關係

```
app_adc.c (濾波後 ADC)
    ↓
ldr_tracking.c (校正 + 誤差計算)
    ↓
LdrTrackingFrame_t
    ↓
├── tracker_controller.c (控制器讀 error_x/y)
├── app_main.c (讀 is_valid 決定是否追蹤)
└── telemetry.c (輸出觀測)
```

---

## 10. 踩雷提醒

1. **ADC 順序 ≠ 物理方位** — `adc1~adc4` 是電路接線順序，`raw[0]~raw[3]` 是邏輯象限順序，兩者在 `UpdateFrame()` 裡是直接對應（adc1→raw[0]），但 LDR 的實體位置由 `LDR_IDX_*` 定義
2. **不要用 raw 算控制** — 必須先扣 baseline + noise_floor，用 delta 才有意義
3. **有效性門檻改錯影響很大** — 太寬→誤追，太窄→追不到
