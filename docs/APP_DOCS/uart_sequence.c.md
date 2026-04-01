# uart_sequence.c

來源: `Core/Src/App/uart_sequence.c`

## 1. 這個檔案的角色

`uart_sequence.c` 是較早期版本使用的序列輸出模組。

它的概念和現在的 `telemetry.c` 很像，都是定期把系統資料輸出成一行 UART 文字；差別在於它比較早期、比較手工、也比較偏 bring-up 階段需求。

目前新版主流程已不再直接使用它，但它還保留在專案內，對理解專案演化過程有幫助。

## 2. 這個模組的特徵

和 `telemetry.c` 相比，它的特色是：

- 幾乎不用 `snprintf`
- 自己寫很多 append helper
- 一段一段把數字和字串塞進 buffer

這種寫法在嵌入式環境很常見，優點是可控、直觀，缺點是維護起來比較繁瑣。

## 3. 主要 helper

### `UartSequence_AppendText()`

把字串拷進 buffer。

### `UartSequence_AppendU32()`

把 unsigned 整數轉成字元。

### `UartSequence_AppendS32()`

處理 signed 整數。

### `UartSequence_AppendFixedWidthU32()`

把數字補成固定寬度，主要用於角度小數部分。

### `UartSequence_AppendAngleX10000()`

把固定小數格式角度轉成 `12.3456` 這種字串。

## 4. `UartSequence_Task()`

這是主要輸出函式。

它會依照 period：

- 檢查是否到時間
- 逐段 append 各欄位
- 最後 `HAL_UART_Transmit()`

目前可輸出的欄位包含：

- serial number
- m1 / m2 mode
- adc1~adc4
- enc1 / enc2
- ang1 / ang2

## 5. 為什麼現在不用它

目前專案演進後，系統需要輸出更多追光狀態，例如：

- calibration done
- valid
- baseline / delta
- error
- search substate

因此新版主流程改用 `TelemetrySnapshot_t + telemetry.c` 的設計，比較符合現在系統的資料量與維護需求。

## 6. 這個檔案還有沒有價值

有，主要在兩個地方：

- 理解專案的早期 bring-up 方式
- 當成極簡 UART 輸出範例

如果未來你想做一個非常輕量的除錯輸出版本，它仍然可以參考。

## 7. 修改時容易踩雷的地方

### 7.1 這裡是舊路徑，不要和現行 telemetry 混用卻沒說清楚

若之後又想重新接回來，最好明確決定：

- 是完全替換 `telemetry.c`
- 還是兩者並行

### 7.2 append helper 很容易有長度邊界問題

如果增加欄位，要小心 buffer capacity。
