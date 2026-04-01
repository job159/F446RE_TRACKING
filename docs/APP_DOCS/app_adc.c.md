# app_adc.c

來源: `Core/Src/App/app_adc.c`

## 1. 這個檔案的角色

`app_adc.c` 是 ADC 應用層的整理模組。

它不負責設定 ADC channel 本身，那部分是 CubeMX 產生的 `main.c` 與 HAL 初始化在做；它負責的是：

- 驗證目前 ADC 設定是否符合 App 的期待
- 啟動 DMA
- 持續從 DMA buffer 取值
- 整理成 App 使用的 4 路邏輯輸入
- 做簡單濾波，讓上層不要直接看到太抖的原始值

## 2. 為什麼需要這一層

如果沒有這一層，`app_main.c` 或 `ldr_tracking.c` 就得自己知道：

- ADC1/ADC2 各掃幾路
- DMA buffer 的順序是什麼
- 哪一路對應 `adc1`、哪一路對應 `adc4`
- 濾波要怎麼做

這會把感測細節散到很多地方。`app_adc.c` 的存在，就是把這些事情收斂起來。

## 3. 目前的資料來源配置

這一版是：

- `ADC1` 掃 2 個 channel
- `ADC2` 掃 2 個 channel

在 `AppAdc_Task()` 裡被整理成：

- `raw_adc[0] = dma_adc1[0]`
- `raw_adc[1] = dma_adc2[0]`
- `raw_adc[2] = dma_adc1[1]`
- `raw_adc[3] = dma_adc2[1]`

對外就是：

- `adc1`
- `adc2`
- `adc3`
- `adc4`

## 4. 初始化流程：`AppAdc_Init()`

初始化時主要做幾件事：

1. 保存 `hadc1` / `hadc2`
2. 清空 DMA buffer
3. 清空 raw 與 filtered buffer
4. 重設 seeded 狀態
5. 分別對兩顆 ADC 呼叫 `AppAdc_StartDmaChannel()`

### `AppAdc_StartDmaChannel()` 在做什麼

這個 helper 會：

- 檢查 ADC 是否為 scan mode
- 檢查是否 continuous mode
- 檢查是否 DMA continuous
- 檢查 `NbrOfConversion` 是否符合預期
- 呼叫 `HAL_ADC_Start_DMA()`
- 關掉 DMA half-transfer / transfer-complete 中斷

### 為什麼把 DMA IRQ 關掉

因為這裡的需求是持續拿最新數值，而不是每次 sample 都用中斷通知。關掉 HT/TC 可以減少中斷負擔，維持簡單的 polling 架構。

## 5. 執行流程：`AppAdc_Task()`

每次主迴圈呼叫 `AppAdc_Task()` 時，主要做兩件事：

### 5.1 整理 DMA 值

把兩個實體 DMA 陣列重新映射成 4 個邏輯通道。

這一步很重要，因為後面的 `ldr_tracking.c` 完全不關心硬體是哪顆 ADC，只關心 `adc1~adc4`。

### 5.2 對每個通道做 low-pass filter

透過 `AppAdc_UpdateFilter()`：

- 第一次值直接當成初值
- 後續值使用 `APP_ADC_FILTER_WEIGHT_PREV` / `APP_ADC_FILTER_WEIGHT_NEW` 做加權平均

目前預設權重是：

- 前值 70%
- 新值 30%

這樣可以去掉一些抖動，但還保留追光需要的反應速度。

## 6. 對外 API 的用途

### `AppAdc_GetFilteredAdc1()` ~ `GetFilteredAdc4()`

這些函式只回傳 filtered 值，不回傳 raw 值。

這表示在目前設計裡，上層的預設用法就是：

- 永遠吃濾過的 ADC

如果之後要做更進階除錯，例如同時看 raw / filtered，可能要補 API 或直接從 handle 取值。

## 7. 這個模組的上下游

### 上游

- `main.c` 內的 ADC 初始化
- DMA 硬體持續寫入 `dma_adc1` / `dma_adc2`

### 下游

- `app_main.c`
- `ldr_tracking.c`
- telemetry snapshot

## 8. 最常調的地方

### 想讓感測更穩

可以調：

- `APP_ADC_FILTER_WEIGHT_PREV`
- `APP_ADC_FILTER_WEIGHT_NEW`

前值比重越高，越穩，但反應越慢。

### 想改實體通道對應

要看：

- `AppAdc_Task()` 裡 `raw_adc[]` 的重排順序
- `main.c` 的 ADC channel 配置
- `ldr_tracking.c` 的 LDR 位置映射

## 9. 修改時容易踩雷的地方

### 9.1 DMA 與 logical channel 是兩層不同概念

不要把 `dma_adc1[0]` 直接當成「第一顆 LDR」的固定真理。真正給追光邏輯看的順序，是 `raw_adc[]` 重排後的順序。

### 9.2 濾波太重會讓追光變鈍

如果你大幅拉高前值權重，LDR 反應會看起來更平滑，但控制器會變得比較慢。

### 9.3 這個檔案不負責象限意義

`app_adc.c` 只保證 4 路資料有序、穩定。哪一路是左上、右下，那是 `ldr_tracking.c` 的責任，不應該在這裡混進去。
