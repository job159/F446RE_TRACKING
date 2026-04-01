# APP_DOCS

這個資料夾是 `Core/Inc/App` 與 `Core/Src/App` 的逐檔說明文件。

前一版文件偏向「快速摘要」，比較像備忘錄。這一版改成比較接近交接文件的寫法，目標是讓你在不先讀完整個專案的情況下，也能知道：

- 這個檔案存在的目的
- 它在整個 App 層的位置
- 它的資料從哪裡來、往哪裡去
- 每個主要函式實際在做什麼
- 哪些參數最常需要調
- 哪些地方修改時最容易踩雷

## 建議閱讀順序

如果你是第一次接這份專案，建議先讀：

1. `app_main.c.md`
2. `tracking_types.h.md`
3. `tracking_config.h.md`
4. `app_adc.c.md`
5. `app_encoder.c.md`
6. `ldr_tracking.c.md`
7. `tracker_controller.c.md`
8. `search_strategy.c.md`
9. `motor_control.c.md`
10. `stepper_tmc2209.c.md`
11. `telemetry.c.md`

這樣會先建立「主流程 -> 共用資料 -> 感測 -> 判斷 -> 控制 -> 驅動 -> 輸出」的完整脈絡。

## 文件分組

### 1. 系統中樞

- `app_main.c.md`
- `app_main.h.md`

### 2. 感測輸入

- `app_adc.c.md`
- `app_adc.h.md`
- `app_encoder.c.md`
- `app_encoder.h.md`

### 3. 模式切換與控制輸入

- `serial_cmd.c.md`
- `serial_cmd.h.md`
- `manual_control.c.md`
- `manual_control.h.md`

### 4. 追光邏輯

- `ldr_tracking.c.md`
- `ldr_tracking.h.md`
- `tracker_controller.c.md`
- `tracker_controller.h.md`
- `search_strategy.c.md`
- `search_strategy.h.md`

### 5. 馬達與驅動

- `motor_control.c.md`
- `motor_control.h.md`
- `stepper_tmc2209.c.md`
- `stepper_tmc2209.h.md`

### 6. 輸出與觀測

- `telemetry.c.md`
- `telemetry.h.md`
- `uart_sequence.c.md`
- `uart_sequence.h.md`

### 7. 共用定義

- `tracking_types.h.md`
- `tracking_config.h.md`

## 這一包文件怎麼用最有效

### 如果你要改功能

先看：

- `app_main.c.md`
- 目標模組對應的 `.c.md`
- 這個模組的 `.h.md`
- `tracking_types.h.md`
- `tracking_config.h.md`

### 如果你要調手感

優先看：

- `tracking_config.h.md`
- `tracker_controller.c.md`
- `search_strategy.c.md`
- `motor_control.c.md`

如果是想透過 UART 直接切換控制週期，也可以一起看：

- `serial_cmd.c.md`
- `app_main.c.md`

### 如果你懷疑接線或感測方向不對

優先看：

- `app_adc.c.md`
- `ldr_tracking.c.md`
- `tracking_config.h.md`

## 文件寫法原則

每份文件都盡量回答同幾個問題：

- 這個檔案的角色是什麼
- 哪些型別或函式是最重要的
- 呼叫順序是什麼
- 會依賴哪些上游資料
- 會提供哪些下游輸出
- 哪裡是之後最常改的點

如果你之後希望，我也可以再把這一包延伸成：

- 各模組的流程圖版
- 參數調整對照表版
- 面向新手的教學版
- 面向維護者的除錯版
