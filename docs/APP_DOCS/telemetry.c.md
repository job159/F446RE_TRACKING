# telemetry.c

來源: `Core/Src/App/telemetry.c`

## 1. 這個檔案的角色

`telemetry.c` 是目前系統主要的觀測輸出層。

它負責把 `TelemetrySnapshot_t` 轉成一行可讀的 UART 訊息，讓你在 terminal 看到：

- mode
- substate
- ADC
- baseline
- delta
- error
- command
- encoder
- manual stage

## 2. 為什麼需要 snapshot

因為如果 telemetry 直接去各模組內部抓資料，會造成：

- 輸出層耦合太深
- 想新增欄位時要跨很多模組
- 很難知道「某一瞬間」的系統整體狀態

目前設計是：

1. `app_main.c` 先組一份 `TelemetrySnapshot_t`
2. `telemetry.c` 只專心把它格式化輸出

這樣觀念比較清楚。

## 3. 主要函式

### `Telemetry_Init()`

保存：

- `huart`
- `period_ms`
- `last_tick_ms`
- `sequence`

### `Telemetry_SendLine()`

這是同步送出一串文字的簡單 helper。

它除了被 `Telemetry_Task()` 間接使用，也被 `app_main.c` 用來送：

- ready 訊息
- help 訊息
- status summary
- calibration summary
- config summary
- manual stage notice

### `Telemetry_Task()`

這是主要的定期輸出函式。

它會：

1. 確認有 UART、snapshot 也有效
2. 檢查是否已到輸出週期
3. 用 `snprintf` 組成單行文字
4. `HAL_UART_Transmit()`
5. 傳送成功後 sequence++

## 4. 輸出內容長什麼樣

目前一行大致包含：

- sequence
- mode
- substate
- calibration done
- valid
- adc[4]
- baseline[4]
- delta[4]
- error x/y
- command x/y
- encoder x/y
- stage

這種格式很適合：

- 手動盯 terminal
- 複製到 log
- 後續拿去簡單分析

## 5. mode / substate 顯示

為了讓輸出更可讀，這裡沒有直接印 enum 整數，而是透過：

- `Telemetry_ModeToText()`
- `Telemetry_SubstateToText()`

把它們轉成：

- `IDLE`
- `TRACK`
- `SEARCH`
- `MANUAL`
- `CAL`
- `WAIT`
- `HBIAS`
- `REVISIT`
- `SWEEP`

## 6. 和舊版 `uart_sequence` 的關係

目前專案裡還保留 `uart_sequence.c`，但現行主流程已改用 `telemetry.c`。

因此：

- `telemetry.c` 是現行正式輸出
- `uart_sequence.c` 是較早期 bring-up 時留下的輸出模組

## 7. 和其他模組的關係

### 上游

- `app_main.c` 組出的 snapshot

### 下游

- log UART

## 8. 最常改的地方

### 想多看一些欄位

要一起改：

- `tracking_types.h` 的 `TelemetrySnapshot_t`
- `app_main.c` 的 `AppMain_UpdateSnapshot()`
- `telemetry.c` 的 format string

### 想改輸出頻率

改：

- `SYS_TELEMETRY_PERIOD_MS`

## 9. 修改時容易踩雷的地方

### 9.1 格式字串很長，欄位增減要小心

增加欄位時要同時確認：

- `%u/%ld` 類型是否對應正確
- buffer 長度是否足夠

### 9.2 `HAL_UART_Transmit()` 是 blocking

如果 telemetry 頻率開太高，主迴圈時間會被吃掉。
