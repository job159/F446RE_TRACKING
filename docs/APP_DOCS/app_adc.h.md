# app_adc.h

來源: `Core/Inc/App/app_adc.h`

## 1. 這個檔案的角色

這個 header 定義 ADC 應用層對外需要知道的資料結構與函式。

它的重點不是演算法，而是「把 ADC 模組的外部介面定義清楚」。

## 2. 主要常數

### `APP_ADC_DEVICE_CHANNEL_COUNT`

代表每顆 ADC 實際掃描幾個 channel。

目前是 `2`，所以：

- ADC1 有兩個 sample
- ADC2 有兩個 sample

### `APP_ADC_LOGICAL_CHANNEL_COUNT`

代表整理後對外提供幾個邏輯通道。

目前是 `4`，也就是：

- `adc1`
- `adc2`
- `adc3`
- `adc4`

## 3. `AppAdc_HandleTypeDef` 內容

這個 handle 同時保存：

- `ADC_HandleTypeDef *hadc1`
- `ADC_HandleTypeDef *hadc2`
- `dma_adc1[]`
- `dma_adc2[]`
- `raw_adc[]`
- `filtered_adc[]`
- `adc_seeded[]`

### 為什麼這樣設計

因為 ADC 模組不是只有「最新值」，它還要記住：

- DMA 原始來源
- 整理後的邏輯值
- 濾波後的輸出值
- 每個通道是否已經有初值

這樣 `AppAdc_Task()` 才能在每圈更新狀態。

## 4. 對外函式

### `AppAdc_Init()`

用途：

- 建立這個模組和實際 HAL ADC handle 的連結
- 啟動 DMA

### `AppAdc_Task()`

用途：

- 週期性更新 ADC buffer 與 filtered 值

### `AppAdc_GetFilteredAdc1()` ~ `AppAdc_GetFilteredAdc4()`

用途：

- 提供上層模組讀取最新濾波結果

## 5. 這個 header 的修改時機

通常只有以下情況才需要改它：

- ADC 通道數改變
- 要增加 raw 值讀取 API
- handle 需要保存更多狀態

如果只是調濾波權重，通常去改 `app_adc.c` 就夠了。
