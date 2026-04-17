# 04 — ADC 取樣、低通濾波、校正流程

ADC 是整個追蹤系統的「眼睛」,輸入品質直接決定追蹤穩定度。
這份文件解釋從 raw ADC 到 LDR 校正後 delta 的完整資料流。

## 資料流總覽

```
   ┌────────┐     DMA       ┌─────────┐  反向?  ┌──────┐
   │ ADC 1  ├──────────────▶│ dma_buf1├────────▶│      │
   └────────┘ circular      └─────────┘         │ raw[]│ 邏輯通道排列
   ┌────────┐               ┌─────────┐         │      │
   │ ADC 2  ├──────────────▶│ dma_buf2├────────▶│      │
   └────────┘               └─────────┘         └──┬───┘
                                                   │
                                          ┌────────▼────────┐
                                          │  低通濾波 (LPF) │
                                          └────────┬────────┘
                                                   │ filtered[4]
                                          ┌────────▼─────────┐
                                          │ LdrTracking      │
                                          │  raw → baseline  │
                                          │  → delta         │
                                          │  → error_x/y     │
                                          └──────────────────┘
```

## 1. ADC 連續轉換 + DMA Circular

設定見 [02_週邊初始化.md](02_週邊初始化.md#2-adc1-與-adc2)

啟動方式([app_adc.c:24-27](../Core/Src/App/app_adc.c:24)):
```c
HAL_ADC_Start_DMA(hadc1, (uint32_t *)h->dma_buf1, APP_ADC_CH_PER_DEVICE);
HAL_ADC_Start_DMA(hadc2, (uint32_t *)h->dma_buf2, APP_ADC_CH_PER_DEVICE);
```

啟動後 **CPU 完全不用管**,DMA 會持續把最新值寫入 buffer。
`AppAdc_Task` 在主迴圈每次都從 buffer 讀最新值處理,不會 race 因為:
- ADC 採樣 ~1.4 µs / 整個 sequence
- 主迴圈跑頻率 >> ADC 完成頻率 → 永遠讀到「最近一次完整的 sequence 結果」

## 2. 通道映射(雙 ADC → 4 路)

```c
raw[0] = orient(dma_buf1[0]);   // ADC1 Rank1 → PC3 → LDR 左上
raw[1] = orient(dma_buf2[0]);   // ADC2 Rank1 → PC4 → LDR 右上
raw[2] = orient(dma_buf1[1]);   // ADC1 Rank2 → PC2 → LDR 右下
raw[3] = orient(dma_buf2[1]);   // ADC2 Rank2 → PC1 → LDR 左下
```

**為何要重排?**
DMA buffer 是按 ADC + Rank 順序,但下游的 `error_x/error_y` 計算需要 0=左上, 1=右上, 2=右下, 3=左下 這個邏輯排列。
重排只是換 index,不會增加任何計算成本。

## 3. ADC 反向 (重要!)

LDR 分壓電路如果是 **LDR 接 GND 側**(非常常見的接法),
光亮時 LDR 阻值降低 → 分壓點電壓**降低** → ADC 讀值**降低**。
這與直覺相反(我們希望「亮 → 大數值」)。

### 解決方案
在 [tracking_config.h](../Core/Inc/App/tracking_config.h:14):
```c
#define ADC_INVERT  1   // 1 = 自動反轉,0 = 不反轉
```

[app_adc.c::orient()](../Core/Src/App/app_adc.c:18):
```c
static uint16_t orient(uint16_t raw)
{
#if ADC_INVERT
  return (uint16_t)(ADC_12BIT_MAX - raw);
#else
  return raw;
#endif
}
```

之後所有下游邏輯都用「亮 = 大值」的直覺。

### 怎麼判斷是否需要反向?
1. 把 4 顆 LDR 都用同一光源照亮
2. 開遙測 (`STATUS` 指令) 看 `adc:` 那一欄
3. **正向接法**: 亮時值高(接近 4095),暗時低(接近 0) → `ADC_INVERT 0`
4. **反向接法**: 亮時值低,暗時高 → `ADC_INVERT 1`

## 4. 低通濾波

ADC 直接讀的值會抖動(電源雜訊、量化雜訊、室內燈房 60Hz 漣波)。
直接拿去做誤差計算會造成馬達高頻抖動。

### 演算法

[app_adc.c::low_pass()](../Core/Src/App/app_adc.c:6):
```c
static uint16_t low_pass(uint16_t old_val, uint16_t new_val)
{
  uint32_t denom = ADC_LPF_ALPHA_NEW + 1;             // 預設 10
  uint32_t sum   = old_val + new_val * ADC_LPF_ALPHA_NEW;
  return (uint16_t)((sum + denom/2) / denom);          // +denom/2 為四捨五入
}
```

數學上:
```
filtered_new = (old × 1 + new × α) / (α + 1)
```
這是一階 IIR 濾波。預設 α=9 → 新值佔 90%,舊值佔 10%。
- 反應快、雜訊抑制中等
- 截止頻率約 = 取樣頻率 / (2π × τ),其中 τ = 1/(1-0.9) - 1 = 9 → cutoff ≈ 取樣頻率 × 0.018

### 想調整濾波?

| 想要 | `ADC_LPF_ALPHA_NEW` |
|---|---|
| **更平滑(慢)** | 1, 2, 3 — 新值權重低 |
| **預設** | 9 |
| **更快反應(更吵)** | 19, 49 |
| **完全不濾** | 把 `low_pass()` 改成 `return new_val` |

### 第一次採樣特殊處理

[app_adc.c::AppAdc_Task()](../Core/Src/App/app_adc.c:48) 開機第一輪:
```c
if (!h->seeded)
{
  for (int i = 0; i < APP_ADC_TOTAL_CH; i++) h->filtered[i] = raw[i];
  h->seeded = 1;
  return;
}
```

不直接濾波,因為 filtered 初始 = 0,第一筆會被「拉低」要花很久才追到實際值。
直接 seed 成 raw 值,從第二筆開始正常 LPF。

## 5. LDR Baseline 校正

每顆 LDR 的暗電流不一樣(製造誤差、電阻精度),不校正會造成靜態 offset。

### 流程(完全自動)

開機後系統進入 **MODE_IDLE / IDLE_CALIBRATING** 持續 5 秒,馬達不動。
這 5 秒主迴圈每次都呼叫:
```c
LdrTracking_AccumulateCalibration(&g.ldr);
```

[ldr_tracking.c:90-109](../Core/Src/App/ldr_tracking.c) 累計:
- `cal_sum[i]`  → 總和(算平均用)
- `cal_min[i]`  → 校正期間最小值
- `cal_max[i]`  → 校正期間最大值
- `cal_samples` → 樣本數

5 秒結束呼叫 `LdrTracking_FinalizeCalibration`:
```c
baseline[i]    = cal_sum[i] / cal_samples;          // 平均
span           = cal_max[i] - cal_min[i];           // 抖動範圍
noise_floor[i] = max(span + LDR_BASELINE_MARGIN,
                     LDR_MIN_NOISE_FLOOR);
```

之後每筆 raw 進來:
```c
floor    = baseline[i] + noise_floor[i];
delta[i] = (raw[i] > floor) ? (raw[i] - floor) : 0;
```

只有「超過環境亮度 + 雜訊裕度」的部分才算「有效光增量」。

### 校正參數調整
[tracking_config.h](../Core/Inc/App/tracking_config.h):

| 參數 | 預設 | 用途 |
|---|---|---|
| `SYS_BOOT_CALIBRATION_MS` | 5000 | 校正秒數,環境穩定可降到 2000;有風扇/閃燈可拉到 10000 |
| `LDR_BASELINE_MARGIN` | 10 | 校正後額外裕度,避免 noise 觸發誤動作。值越大越「鈍」 |
| `LDR_MIN_NOISE_FLOOR` | 6 | 最低 noise floor 下限,避免 LDR 太穩定時 floor 算出 0 反而不抗噪 |

### 重新校正

任何時候發 `RECAL` 串口指令(或寫程式呼叫 `start_calibration()`),系統會立刻清空校正數據,進入校正狀態。
**校正期間任何模式切換指令會被「排隊」**,校正完成才生效(避免馬達在校正中亂轉)。

### 校正失敗的徵兆

| 症狀 | 原因 | 修正 |
|---|---|---|
| 校正完馬達狂轉 | 校正時光源不穩定 | 校正時遮光或固定光源 |
| 永遠 valid:0 | 環境光太亮蓋過 LDR 動態 | 加遮光罩、或移到較暗處 |
| 一邊永遠不亮 | 該 LDR 接線斷 / 短路 | 檢查焊接 |
| baseline 全 4095 | ADC_INVERT 設錯 | 對應切換 0/1 |

## 6. 校正後資料查詢

串口指令 `CAL?`:
```
CALDATA base:1234,1230,1240,1235 floor:18,16,20,15 done:1
```
- `base:` 4 顆的 baseline (DC 環境光)
- `floor:` 4 顆的 noise floor (LPF 後抖動範圍 + margin)
- `done:` 1 = 已完成校正

串口指令 `STATUS`:
```
STATUS mode:TRACK idle:- cal:1 valid:1 total:856 contrast:412 cmd:120,-95
```
- `valid:1` 表示目前光源資訊有效,正在追蹤
- `total:` = delta[0]+delta[1]+delta[2]+delta[3] (4 路有效光增量總和)
- `contrast:` = max(delta) - min(delta) (4 路差異)

## 7. 取樣速率與控制週期

| 階段 | 頻率 |
|---|---|
| ADC 自動轉換 | 連續,每 sequence ~1.4 µs |
| `AppAdc_Task` 主迴圈呼叫 | 每次主迴圈跑(無延遲限制),約 100 kHz 等級 |
| `run_control` (含 LDR 計算 + PID) | 每 `SYS_CONTROL_PERIOD_MS` 一次,預設 5ms |

可以說 ADC 採樣是 1 kHz 等效(被控制週期 throttle),這對 LDR 物理響應(秒級)綽綽有餘。
若要降低 CPU 負擔,把 `PERIOD 5` 串口指令送進去,改成 5ms 控制週期。
