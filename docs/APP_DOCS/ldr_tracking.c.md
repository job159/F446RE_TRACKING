# ldr_tracking.c

來源: `Core/Src/App/ldr_tracking.c`

## 1. 這個檔案的角色

`ldr_tracking.c` 是追光判斷的數學核心。

它把 4 路 ADC 轉成「控制器可以理解的資訊」，包含：

- 哪些值屬於背景
- 哪些值屬於有效光源
- 現在光源偏左還偏右
- 偏上還偏下
- 當前資料到底可不可靠

簡單說，它是把「感測值」轉成「追光誤差」的那一層。

## 2. 這個模組輸入與輸出是什麼

### 輸入

- `adc1_value`
- `adc2_value`
- `adc3_value`
- `adc4_value`

### 輸出

輸出集中在 `LdrTrackingFrame_t frame` 內：

- `raw`
- `baseline`
- `noise_floor`
- `delta`
- `total`
- `contrast`
- `error_x`
- `error_y`
- `is_valid`
- `calibration_done`

所以控制器不需要再自己回頭理解 ADC，只要看 frame 即可。

## 3. LDR 實體排列假設

目前這版已依你的實體編號修正成順時鐘配置：

```text
1(TL)  2(TR)
4(BL)  3(BR)
```

對應到程式內的邏輯索引是：

- `raw[0]` -> top-left
- `raw[1]` -> top-right
- `raw[2]` -> bottom-right
- `raw[3]` -> bottom-left

### 為什麼這件事很重要

因為左右、上下誤差的計算完全依賴這個象限對應。

如果 3、4 顆放反，`error_x` 就可能朝錯的方向變化，造成追光越追越偏。

## 4. 主要流程

### 4.1 `LdrTracking_UpdateFrame()`

這是上層每個控制週期會呼叫的入口。

它做兩件事：

1. 更新 `frame.raw[]`
2. 呼叫 `LdrTracking_Recompute()`

### 4.2 `LdrTracking_Recompute()`

這是核心計算函式。

它會：

1. 對每顆 LDR 計算 `effective_baseline = baseline + noise_floor`
2. 若 raw 大於有效基準，取 `delta = raw - effective_baseline`
3. 累加得到 `total`
4. 找出 `min_delta` / `max_delta`
5. 計算 `contrast = max_delta - min_delta`
6. 計算左右與上下總和
7. 若亮度與對比達門檻，算出 `error_x` / `error_y`
8. 否則把誤差歸零並標記 `is_valid = 0`

## 5. 校正流程

這個模組本身也管理 startup calibration。

### `LdrTracking_ForceRecalibration()`

作用：

- 清空累積 sum / min / max
- 清掉 `calibration_done`
- 清掉當前 valid / error / total / contrast

### `LdrTracking_AccumulateCalibration()`

在校正期間被呼叫，用來累積：

- 每顆 LDR 的總和
- 最小值
- 最大值
- sample 數

### `LdrTracking_FinalizeCalibration()`

校正結束時：

- `baseline = average`
- `noise_floor = max(noise_span + margin, minimum_floor)`

這樣之後就能把環境背景亮度扣掉，只對明顯高於背景的光做反應。

## 6. `error_x` / `error_y` 的意義

目前定義是：

- `error_x > 0` 代表右側比較亮
- `error_y > 0` 代表上側比較亮

公式概念：

```text
error_x = (right - left) / total
error_y = (top - bottom) / total
```

所以它是歸一化誤差，不是原始亮度差。

這樣做的好處是：不同環境總亮度變化時，控制器拿到的誤差尺度比較穩定。

## 7. 有效性判斷

`is_valid` 不是只看總亮度。

它同時要求：

- `total >= TRACK_VALID_TOTAL_MIN`
- `contrast >= TRACK_DIRECTION_CONTRAST_MIN`
- `calibration_done != 0`

這個設計是在避免「雖然亮，但四顆亮得差不多」時，系統錯把它當成有明確方向的光源。

## 8. 和其他模組的關係

### 上游

- `app_adc.c`
- `tracking_config.h`

### 下游

- `tracker_controller.c`
- `search_strategy.c`
- `telemetry.c`
- `app_main.c`

## 9. 最常改的地方

### 想改 LDR 方向定義

通常改這裡的索引 mapping。

### 想讓系統更容易判定「有光」

通常改：

- `TRACK_VALID_TOTAL_MIN`
- `TRACK_DIRECTION_CONTRAST_MIN`
- `LDR_BASELINE_MARGIN`
- `LDR_MIN_NOISE_FLOOR`

## 10. 修改時容易踩雷的地方

### 10.1 ADC 順序和物理方位是兩回事

先有 `adc1~adc4` 的輸入順序，後有它們在 `ldr_tracking.c` 裡的象限意義。不要把這兩層概念混在一起。

### 10.2 不要直接用 raw 算控制

真正要給控制器看的應該是扣過 baseline / noise floor 後的結果，而不是 raw ADC。

### 10.3 valid 條件太寬或太窄都會出問題

太寬：

- 容易誤追
- 容易追雜光

太窄：

- 容易進 search
- 很難重新追上光源
