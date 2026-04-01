# telemetry.h

來源: `Core/Inc/App/telemetry.h`

## 1. 這個檔案的角色

這個 header 定義 telemetry 模組的公開資料結構與函式。

它雖然簡短，但很重要，因為它把「系統狀態輸出」這件事和其他模組隔開。上層只需要提供 snapshot，不需要知道字串格式細節。

## 2. `Telemetry_HandleTypeDef`

handle 內包含：

- `UART_HandleTypeDef *huart`
- `uint32_t period_ms`
- `uint32_t last_tick_ms`
- `uint32_t sequence`

### 這些欄位分別代表什麼

- `huart`: 要送去哪個 UART
- `period_ms`: 多久送一次
- `last_tick_ms`: 上次成功檢查輸出的時間
- `sequence`: 第幾筆 telemetry

也就是說，這個模組自己保存輸出節奏，不需要外部幫它記錄。

## 3. 對外 API

### `Telemetry_Init()`

建立 telemetry handle 和 UART / period 的關聯。

### `Telemetry_Task()`

每圈都可以呼叫，但只有在時間到了才真的輸出。

### `Telemetry_SendLine()`

用來發送非週期性的單行訊息，例如：

- ready 訊息
- help
- status summary
- error notice

## 4. 使用方式

目前系統標準流程是：

1. `AppMain_Init()` 呼叫 `Telemetry_Init()`
2. `AppMain_UpdateSnapshot()` 整理目前資料
3. `AppMain_Task()` 每圈呼叫 `Telemetry_Task()`

## 5. 修改時機

這個 header 通常在以下情況會改：

- 想把 blocking UART 改成 DMA/non-blocking
- 想讓 telemetry handle 記錄錯誤統計
- 想支援多個輸出端口

## 6. 修改時要注意

這個 header 若變動，通常會連動：

- `telemetry.c`
- `app_main.c`
- 任何呼叫 `Telemetry_SendLine()` 的地方
