# serial_cmd.h

來源: `Core/Inc/App/serial_cmd.h`

## 1. 這個檔案的角色

定義 serial command parser 對外需要公開的型別與 API。

它本身不包含解析邏輯，但決定了 parser 模組對外怎麼被使用。

## 2. `SerialCmd_HandleTypeDef`

這個 handle 內包含三塊資訊：

### 2.1 UART 來源

- `UART_HandleTypeDef *huart`

### 2.2 正在組裝中的輸入行

- `rx_line[]`
- `rx_length`

### 2.3 已解析好的命令 queue

- `queue[]`
- `queue_head`
- `queue_tail`
- `queue_count`

這讓單一 handle 就能完整保存 parser 的 runtime 狀態。

## 3. 對外函式

### `SerialCmd_Init()`

初始化 parser handle，建立和 UART 的關聯。

### `SerialCmd_PollRx()`

讓 parser 去 UART 讀目前可用資料，並在需要時組成命令。

### `SerialCmd_Dequeue()`

讓上層逐筆取出已解析好的命令。

## 4. 使用方式

目前標準用法是：

1. `AppMain_Init()` 呼叫 `SerialCmd_Init()`
2. `AppMain_Task()` 每圈呼叫 `SerialCmd_PollRx()`
3. 然後在 while 迴圈裡持續 `SerialCmd_Dequeue()`

## 5. 這個 header 最常在什麼情況修改

- queue 長度需求變更
- parser handle 需要記更多狀態
- 對外 API 要增加清空 queue 或 flush 之類功能
