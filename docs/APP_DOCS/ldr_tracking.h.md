# ldr_tracking.h

來源: `Core/Inc/App/ldr_tracking.h`

## 1. 這個檔案的角色

宣告 LDR tracking 模組的 handle 與 API。

## 2. `LdrTracking_HandleTypeDef`

這個 handle 同時保存兩種東西：

### 2.1 執行後的輸出

- `LdrTrackingFrame_t frame`

### 2.2 校正中的中間資料

- `calibration_sum[4]`
- `calibration_min[4]`
- `calibration_max[4]`
- `calibration_samples`

這代表校正不是一個獨立模組，而是直接內建在 LDR tracking handle 內。

## 3. 對外 API

### 初始化與重設

- `LdrTracking_Init()`
- `LdrTracking_ForceRecalibration()`

### 更新 frame

- `LdrTracking_UpdateFrame()`

### 校正流程

- `LdrTracking_AccumulateCalibration()`
- `LdrTracking_FinalizeCalibration()`

## 4. 使用方式

目前標準使用順序是：

1. `Init()`
2. `ForceRecalibration()`
3. 校正期間持續 `UpdateFrame()` + `AccumulateCalibration()`
4. 時間到後 `FinalizeCalibration()`
5. 正常運作期間持續 `UpdateFrame()`

## 5. 修改時機

通常以下情況才會改這個 header：

- frame 內容需要增加欄位
- 校正流程需要保存更多狀態
- API 要增加 manual baseline 或 debug 介面
