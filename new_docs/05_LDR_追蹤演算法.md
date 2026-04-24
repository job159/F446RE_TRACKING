# 05 — LDR 追蹤演算法

整個系統的核心:從 4 顆 LDR 的光強度推出「光源在哪個方向」,
化為兩軸 error 給 PID 控制器消除。

## 1. 物理排列複習

從鏡頭面正視:

```
        +--------+--------+
        |  LDR0  |  LDR1  |
        | (左上) | (右上) |
        +--------+--------+
        |  LDR3  |  LDR2  |
        | (左下) | (右下) |
        +--------+--------+
```

對應軟體陣列 index `delta[0..3]`。

## 2. 從 raw 到 delta

詳見 [04_ADC_濾波與校正.md](04_ADC_濾波與校正.md)。
最終得到的 `delta[i]` = 第 i 顆 LDR **超過環境光 + 雜訊裕度** 的部分。
若該顆 LDR 的光線等同環境光,delta = 0。

## 3. 誤差計算

[ldr_tracking.c::recompute()](../Core/Src/App/ldr_tracking.c:14):

### Step 3.1 — 累計總光量與對比度
```c
total    = delta[0] + delta[1] + delta[2] + delta[3];
contrast = max(delta) - min(delta);
```

- `total` 衡量「光源整體強度」
- `contrast` 衡量「方向辨識度」

### Step 3.2 — 有效性判定
```c
if (calibration_done
    && total    >= TRACK_VALID_TOTAL_MIN
    && contrast >= TRACK_DIRECTION_CONTRAST_MIN)
{
    // 計算誤差
} else {
    error_x = error_y = 0;
    is_valid = 0;
}
```

兩個門檻在 [tracking_config.h](../Core/Inc/App/tracking_config.h):
- `TRACK_VALID_TOTAL_MIN`        = 140 (4 路加總)
- `TRACK_DIRECTION_CONTRAST_MIN` = 28 (最亮-最暗差)

**為何要有對比門檻?**
散射光(陰天、室內漫射)4 顆讀值差不多,total 高但 contrast 低,
這時無法判斷方向,**寧可不動,等真實光源出現**。

**與 `TRACK_ERR_CAP` 互補**:
這兩個門檻屬於「算 error 之前」的拒絕;算出 error 後還會被 `TRACK_ERR_CAP = 0.7` 再夾一次
(見 [06_PID_控制邏輯.md](06_PID_控制邏輯.md#4-雙層誤差保護)),處理單邊陰影的假極端值。兩層任一觸發都會抑制馬達。

### Step 3.3 — 加總分組計算左右上下
```c
left  = delta[0] + delta[3];   // 左上 + 左下
right = delta[1] + delta[2];   // 右上 + 右下
top   = delta[0] + delta[1];   // 左上 + 右上
bot   = delta[3] + delta[2];   // 左下 + 右下
```

### Step 3.4 — 歸一化誤差(浮點)
```c
error_x = (right - left) / total;   // -1 ~ +1
error_y = (top   - bot) / total;    // -1 ~ +1
```

| `error_x` | 意義 |
|---|---|
| 正(右側亮) | 光源偏右 → 馬達 1 (水平軸) 應**正轉**追上 |
| 0 | 左右等亮 |
| 負(左側亮) | 光源偏左 → 馬達 1 應**反轉** |

`error_y` 同理為仰角軸。

### 為何要除以 total 歸一化?
- 強光時 (total=2000) 偏 50 與弱光時 (total=200) 偏 50 應該觸發**等量校正**
- 不歸一化 PID gain 會跟光源強度耦合,室內室外要重調

## 4. 校正未完成的安全行為

[ldr_tracking.c:25-28](../Core/Src/App/ldr_tracking.c:25):
```c
if (h->frame.calibration_done && h->frame.raw[i] > floor)
    h->frame.delta[i] = h->frame.raw[i] - (uint16_t)floor;
else
    h->frame.delta[i] = 0;
```

校正未完成 → 所有 delta 永遠 0 → total=0 → is_valid=0 → 馬達不動。
這是**故障安全**設計,確保開機未校正時不會亂轉。

## 5. 追蹤主流程(在 app_main.c)

`MODE_TRACKING` 狀態下每 `SYS_CONTROL_PERIOD_MS` (預設 5ms) 跑一次:

```c
case MODE_TRACKING:
  if (!g.ldr.frame.is_valid)
  {
    TrackerController_Reset(&g.tracker);                   // 重置(純 P 無歷史,僅保險)
    MotorControl_StopAll(&g.motor);                        // 停下兩軸
  }
  else
  {
    MotionCommand_t cmd = TrackerController_Run(
        &g.tracker, &g.ldr.frame, g.ctrl_period_ms);
    MotorControl_ApplyCommand(&g.motor, &cmd, g.ctrl_period_ms);
  }
  break;
```

**沒有光源就停**,不執行任何 search / sweep。
有光源就跑純 P 控制(見 [06_PID_控制邏輯.md](06_PID_控制邏輯.md)),輸出 step Hz 給馬達。
`ApplyCommand` 同時會做軟限位 clamp 與位置累加。

## 6. 參數調整指南

### 太敏感(輕微擾動就動)

| 改 | 方向 | 效果 |
|---|---|---|
| `TRACK_VALID_TOTAL_MIN` | ↑ (例如 200) | 弱光忽略 |
| `TRACK_DIRECTION_CONTRAST_MIN` | ↑ (例如 50) | 散射光不追 |
| `PID_ERR_DEADBAND` | ↑ (例如 0.030) | 中央死區放大,微偏不動 |

### 太鈍(明顯偏向也不動)

| 改 | 方向 | 效果 |
|---|---|---|
| `TRACK_VALID_TOTAL_MIN` | ↓ (例如 80) | 弱光也追 |
| `TRACK_DIRECTION_CONTRAST_MIN` | ↓ (例如 15) | 模糊光也追 |
| `PID_ERR_DEADBAND` | ↓ (例如 0.010) | 微小偏差也動 |
| `LDR_BASELINE_MARGIN` | ↓ | 校正後更敏感 |

### 追到目標但小範圍擺動

- 放大 `PID_ERR_DEADBAND`(主要,例 0.020 → 0.030)
- 降低 `M*_KP_SMALL`(細節見 [06_PID_控制邏輯.md](06_PID_控制邏輯.md))

### 追蹤太慢

- 放大 `M*_KP_MEDIUM` / `M*_KP_LARGE`
- 放大 `M*_OUTPUT_GAIN`(1.0 → 1.2)
- 放大 `M*_MAX_STEP_HZ`(預設 60000,通常已經夠大)
- 若 ramp 才是真正的瓶頸,放大 `RAMP_STEP_HZ`(見 [07_馬達細分與速度.md](07_馬達細分與速度.md))

## 7. 方向反了怎麼辦?

如果觀察到「光在右,馬達卻往左跑」,由簡到繁有四種做法:

**選擇 A — 改 `M*_TRACK_DIR`**(最簡單,推薦)
[tracking_config.h](../Core/Inc/App/tracking_config.h):
```c
#define M1_TRACK_DIR   (-1)   // 只翻轉 M1 (axis1) 追蹤方向
#define M2_TRACK_DIR   (-1)   // 只翻轉 M2 (axis2) 追蹤方向
```
**僅影響 TRACKING**,Manual 不受影響。

**選擇 B — 改硬體接線**
對調 4 顆 LDR 的焊點,或對調步進馬達其中一相的 A1/A2。

**選擇 C — 改通道映射**
[app_adc.c::AppAdc_Task()](../Core/Src/App/app_adc.c) 把 `raw[0]` 跟 `raw[1]` 對調,效果是水平翻轉;
對調 `raw[2]` / `raw[3]` 則垂直翻轉。

**選擇 D — TMC2209 GCONF shaft bit**
GCONF bit3 設 1 翻轉該軸方向(但目前兩軸共用 `VAL_GCONF`,要翻單軸需拆成兩個常數)。
詳見 [03_TMC2209_暫存器.md](03_TMC2209_暫存器.md#1-gconf-0x00--全域設定)。

推薦順序: **A > B > C > D**。A 是純軟體、且只影響 TRACKING,最安全。

## 8. 範例 — 從遙測解讀追蹤狀態

```
seq mode:TRACK sub:- cal:1 valid:1 adc:200,1800,180,210 base:150,160,155,148 d:50,1640,25,62 err:843,-72 cmd:6200,-510 stg:255
```

解讀:
- `valid:1` 正在追蹤
- `adc:200,1800,180,210` → ch1 (右上) 1800,其他暗
- `d:50,1640,25,62` → 對應 delta,ch1 主導
- `err:843,-72` → error_x = 0.843 (光在右),error_y = -0.072 (略低)
- `cmd:6200,-510` → 馬達 1 正轉 6200 Hz 衝過去,馬達 2 微調反轉

如果 `valid` 一直 0 → 校正或門檻問題,看 [04 文件](04_ADC_濾波與校正.md#7-取樣速率與控制週期)。
