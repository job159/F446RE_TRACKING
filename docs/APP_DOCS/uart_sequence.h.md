# uart_sequence.h

來源: `Core/Inc/App/uart_sequence.h`

## 1. 這個檔案的角色

這個 header 定義舊版 UART sequence 模組的公開介面。

雖然它現在不是現行主流程的一部分，但它仍然說明了專案早期是如何把 ADC、encoder 與 motor mode 組成序列輸出的。

## 2. `UartSequence_HandleTypeDef`

包含：

- `UART_HandleTypeDef *huart`
- `uint32_t period_ms`
- `uint32_t last_tick_ms`
- `uint32_t serial_number`

這個設計和現在的 `Telemetry_HandleTypeDef` 很相似，代表它本來就是 telemetry 之前的較早期版本。

## 3. 對外 API

### `UartSequence_Init()`

建立這個模組和 UART / period 的關聯。

### `UartSequence_Task()`

把 ADC、encoder 與 motor mode 整理成一行 UART 字串輸出。

## 4. 目前在專案中的地位

目前新版 `app_main.c` 已經改用 `telemetry.c`，所以這個 header 雖然還在，但比較偏：

- 歷史保留
- 參考範例
- 極簡輸出模組

## 5. 修改時機

通常以下情況才會動它：

- 想重新接回舊版 sequence 輸出
- 想做一個比 telemetry 更精簡的 log 版本
- 想研究專案演化過程
