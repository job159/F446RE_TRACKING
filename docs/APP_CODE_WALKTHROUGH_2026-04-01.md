# App Code Walkthrough

Date: 2026-04-01

## 變更註記（2026-04-01 最新）

以下幾點是本文件原始內容之後的最新行為，優先以這裡為準：

- `app_main.c` 已停用 search 介入路徑：追蹤失效時會直接停住馬達，不再進 `MODE_SEARCH` 掃描。
- 控制週期預設值改為 `1 ms`（`SYS_CONTROL_PERIOD_DEFAULT_MS = 1U`），可用 UART `PERIOD 1MS|2MS|5MS` 切換。
- serial 命令主集合更新為：
  `IDLE | TRACK | MANUAL | MAN 1..8 | PERIOD 1MS|2MS|5MS | RECAL | STATUS | CALDATA | CONFIG | HELP`
- 追蹤輸出步幅目前設定為 `2.0f`（兩軸），步頻上限則維持較高值以保留後續調參空間。

## 目的

這份文件是針對目前 `Core/Inc/App` 與 `Core/Src/App` 這一層的程式做現況說明，重點不是描述理想架構，而是解釋「現在這版程式實際怎麼寫、怎麼跑、模組怎麼互相配合」。

目前專案採用：

- STM32CubeMX 產生的 `main.c`
- 沒有 RTOS 的 `super loop`
- 單一全域 `g_app` context 管理應用層狀態
- 以 HAL 為基礎的同步式控制流程

如果之後要維護、擴充或重構，建議先看完這份文件，再去讀 `app_main.c`。

## 1. 程式入口怎麼接到 App

整個程式的硬體初始化仍然在 `Core/Src/main.c`。

流程是：

1. `HAL_Init()`
2. `SystemClock_Config()`
3. CubeMX 產生的 GPIO / DMA / ADC / UART / TIM 初始化
4. 呼叫 `AppMain_Init(...)`
5. 進入 `while (1)`，持續呼叫 `AppMain_Task()`

也就是說，`main.c` 負責「把硬體開起來」，`app_main.c` 負責「把系統跑起來」。

可以把目前架構理解成：

```text
main.c
  -> AppMain_Init(...)
  -> while (1)
       AppMain_Task()
```

## 2. App 層的寫法風格

目前 App 層的寫法有幾個很固定的模式：

- 每個子模組都有自己的 `*_HandleTypeDef`
- 幾乎每個模組都有 `Init()`，有些模組再加 `Task()`、`Run()`、`Reset()` 或 `Get*()`
- 模組之間盡量透過資料結構互動，不直接互相操作內部狀態
- 共用 enum / struct 集中在 `tracking_types.h`
- 調參常數集中在 `tracking_config.h`
- 不使用動態記憶體，所有狀態都放在靜態或結構內

這代表目前程式偏向嵌入式常見的「可預測、平鋪、易除錯」寫法，而不是事件驅動或物件導向風格。

## 3. 核心控制中樞：`app_main.c`

`app_main.c` 是整個 App 層的核心。它做了三件事：

1. 持有全系統 context
2. 管理模式切換的狀態機
3. 當 scheduler，把各子模組串起來

### 3.1 全域 context

`AppMain_Context_t` 把所有模組 instance 和系統狀態放在一起，包含：

- ADC / Encoder / Serial / Telemetry handle
- LDR tracking frame
- Tracker controller
- Search history 與 search strategy
- Motor control 與 manual control
- 當前 mode / idle substate
- calibration 與 control 的時間戳
- lost / reacquire counter

這種寫法的好處是：

- 主流程狀態集中，排查問題時很好追
- `AppMain_Task()` 不需要到處傳很長的參數

代價是：

- `app_main.c` 會變成系統耦合中心
- 之後如果模式再變複雜，這個檔案會繼續膨脹

### 3.2 `AppMain_Init()` 在做什麼

初始化順序大致如下：

1. 清空 `g_app`
2. 初始化 ADC 與 Encoder
3. 初始化 Serial command parser
4. 初始化 LDR tracking 並強制進入重新校正狀態
5. 初始化 tracker / history / search / manual / telemetry
6. 初始化 motor control
7. 設定初始 mode 為 `MODE_IDLE`
8. 初始 idle substate 為 `IDLE_CALIBRATING`

因此系統上電後不是直接追光，而是先進入 5 秒校正期。

### 3.3 `AppMain_Task()` 每圈做什麼

`AppMain_Task()` 是 super loop 的主工作函式。每次執行時，順序是：

```text
1. 讀取 UART 指令輸入
2. 更新 ADC 濾波值
3. 更新 Encoder 計數
4. 把已收到的命令逐筆 dequeue 並處理
5. 依目前設定的控制週期執行一次控制回圈
6. 更新 telemetry snapshot
7. 每 100 ms 輸出一次 telemetry
```

這裡最重要的是第 5 步。

`AppMain_Task()` 不會每圈都直接跑控制，而是用 `HAL_GetTick()` 判斷是否已經到目前的 control period。上電預設值來自 `SYS_CONTROL_PERIOD_DEFAULT_MS`，目前預設是 `1 ms`，也可以透過 UART 指令切成 `2 ms` 或 `5 ms`。這樣控制頻率可在約 `1000 Hz`、`500 Hz`、`200 Hz` 間切換。

## 4. 系統狀態機

目前主模式定義在 `tracking_types.h`：

- `MODE_IDLE`
- `MODE_TRACKING`
- `MODE_SEARCH`
- `MODE_MANUAL`

另外還有兩組子狀態：

- `IDLE_CALIBRATING`
- `IDLE_WAIT_CMD`

以及 search 子狀態：

- `SEARCH_HISTORY_BIAS`
- `SEARCH_REVISIT_LAST_GOOD`
- `SEARCH_SWEEP_SCAN`

### 4.1 啟動流程

上電後流程如下：

1. 先進入 `MODE_IDLE`
2. idle 子狀態先是 `IDLE_CALIBRATING`
3. 在這 5 秒內持續收集四顆 LDR 的 baseline 與 noise floor
4. 5 秒到後執行 `AppMain_FinalizeCalibration()`
5. 若使用者在校正期間已要求切到 manual，校正結束後會優先進 manual
6. 若沒有被 manual 覆蓋，校正完成後預設直接進入 `MODE_TRACKING`

### 4.2 Tracking 到 Search 的切換

在 `MODE_TRACKING` 裡：

- 只要光源判定無效，不會立刻 search
- 會先累積 `lost_counter`
- 連續失敗達到 `TRACK_LOST_CONSECUTIVE_COUNT` 後才切進 `MODE_SEARCH`

這是為了避免單次雜訊或瞬間陰影就讓模式來回跳動。

### 4.3 Search 回到 Tracking 的切換

在 `MODE_SEARCH` 裡：

- 只要 LDR frame 又恢復有效，不會立刻回 tracking
- 會先累積 `reacquire_counter`
- 連續有效達到 `TRACK_REACQUIRE_CONSECUTIVE_COUNT` 後才回 `MODE_TRACKING`

這和 tracking -> search 一樣，目的也是做去抖動。

### 4.4 Manual 模式

`MODE_MANUAL` 不跑追光控制器，也不跑搜尋策略。

它只接受人工輸入的 speed stage，然後交給 `motor_control` 去套用。

## 5. 資料流是怎麼串的

目前實際資料流如下：

```text
ADC DMA + Encoder Timer
  -> app_adc / app_encoder
  -> ldr_tracking 產生 frame
  -> app_main 依 mode 決定要進 tracking / search / manual
  -> tracker_controller 或 search_strategy 產生命令
  -> motor_control
  -> stepper_tmc2209

同時：
UART log RX
  -> serial_cmd
  -> app_main 處理命令

系統狀態
  -> telemetry snapshot
  -> telemetry UART TX
```

## 6. 各模組怎麼寫

### 6.1 `app_adc.[hc]`

角色：管理 ADC1 / ADC2 的 DMA 採樣，整理成 4 路邏輯輸入。

目前特性：

- `ADC1` 與 `ADC2` 都是 scan + continuous + DMA continuous
- 每顆 ADC 各跑 2 個 channel
- `AppAdc_Task()` 把兩個 DMA buffer 重排成 `adc1 ~ adc4`
- 每個通道再做一次簡單 low-pass filter

這一層只做「取樣與平滑」，不做光學意義判斷。

### 6.2 `app_encoder.[hc]`

角色：讀兩組編碼器的 timer counter，累積成長期 count。

目前做法：

- 使用 `HAL_TIM_Encoder_Start()`
- 每次 `AppEncoder_Task()` 讀當前 counter
- 用本次 counter 和上次 counter 的差值累加到 `count_enc1` / `count_enc2`
- 額外提供角度換算函式，輸出 `x10000` 格式的 degree

這一層只負責位置回授，不碰控制邏輯。

### 6.3 `serial_cmd.[hc]`

角色：從 log UART 讀使用者命令並轉成結構化資料。

目前寫法重點：

- 直接 polling `RXNE` flag，不用中斷或 DMA
- 收到 `\r` 或 `\n` 才視為一筆完整命令
- 命令先轉成大寫、去頭尾空白，再做解析
- 解析成功後放進固定長度 queue
- `app_main` 再逐筆 dequeue 處理

支援的主要命令（建議用法）：

- `IDLE`
- `TRACK`
- `MANUAL`
- `MAN 1..8`
- `PERIOD 1MS|2MS|5MS`
- `RECAL`
- `STATUS`
- `CALDATA`
- `CONFIG`
- `HELP`

相容舊命令（仍可用）：

- `MODE 0/1/2`
- `F1..F4`、`R1..R4`、`STAGE 0..7`
- `CAL`、`STAT?`、`CAL?`、`CFG?`
- `CTL 1|2|5`

這種 queue 化寫法的好處是：UART 收命令與系統狀態切換是分開的，不會在收字元當下就直接改整個系統。

### 6.4 `ldr_tracking.[hc]`

角色：把四路 LDR 數值轉成「能不能追」以及「要往哪邊追」。

這一層是追光判斷的數學核心。

主要流程：

1. `LdrTracking_UpdateFrame()` 更新四路 raw ADC
2. `LdrTracking_Recompute()` 依 baseline 與 noise floor 計算 `delta`
3. 計算總亮度 `total`
4. 計算對比度 `contrast`
5. 算出左右誤差 `error_x`
6. 算出上下誤差 `error_y`
7. 根據門檻判定 `is_valid`

其中：

- `baseline` 來自開機 5 秒校正平均值
- `noise_floor` 來自校正期間的振幅範圍加上保護 margin
- 只有 `calibration_done != 0` 時，delta 與 valid 才有實際意義

目前控制邏輯採用的 LDR 實體排列是：

```text
adc1(TL)  adc2(TR)
adc4(BL)  adc3(BR)
```

所以：

- `error_x > 0` 代表右側比較亮
- `error_y > 0` 代表上方比較亮

也就是說，程式現在假設實體編號是 `1 -> 2 -> 3 -> 4` 順時鐘繞一圈：

```text
1(TL)  2(TR)
4(BL)  3(BR)
```

### 6.5 `tracker_controller.[hc]`

角色：把 `error_x` / `error_y` 轉成兩軸馬達 step rate。

這一層不是純 PID，而是帶有幾個工程化處理：

- deadband
- 依誤差大小切換不同 `Kp`
- 小到中誤差才累積 integrator
- 加 derivative
- 輸出飽和限制
- 變化率限制 rate limit
- 正反方向可用不同 scale 補償機構不對稱

輸出結果是：

```c
MotionCommand_t
{
  axis1_step_hz,
  axis2_step_hz
}
```

這裡的 step rate 已經是可直接送去驅動層的命令，不再是抽象控制量。

### 6.6 `search_strategy.[hc]`

角色：追光失敗後，決定如何找回光源。

這一層分成兩部分：

- `TrackingHistory_*`：保存最近有效追蹤資料
- `SearchStrategy_*`：定義 search 狀態機

search 有三段：

1. `SEARCH_HISTORY_BIAS`
2. `SEARCH_REVISIT_LAST_GOOD`
3. `SEARCH_SWEEP_SCAN`

含義如下：

- `SEARCH_HISTORY_BIAS`
  先沿用最近幾次追蹤時的平均命令方向，假設光源只是短暫偏離
- `SEARCH_REVISIT_LAST_GOOD`
  再試著回到最後一次有效追蹤的位置
- `SEARCH_SWEEP_SCAN`
  如果還是找不到，就做固定節奏的掃描

這表示目前 search 不是隨機亂找，而是先利用已知歷史，再退化成掃描。

### 6.7 `motor_control.[hc]`

角色：把上層的抽象馬達命令轉成雙軸實際驅動。

這一層做的事：

- 初始化兩顆 `StepperTmc2209_HandleTypeDef`
- 提供 `StopAll()`
- 接收 `MotionCommand_t` 並套用到兩軸
- 支援 manual stage 模式
- 保存最後一次成功下達的 motion command

這裡的角色很像「App 控制邏輯」和「驅動層」之間的 adapter。

### 6.8 `manual_control.[hc]`

角色：管理 manual mode 下的 stage 指令。

它的設計很簡單：

- `SetStage()` 只先記錄 pending stage
- 真正套用發生在 `ManualControl_Task()`
- 套用成功後才更新 active stage

這種分兩段的寫法可以避免在命令解析當下直接碰硬體。

### 6.9 `telemetry.[hc]`

角色：定期把系統快照送到 log UART。

做法是：

1. `app_main` 先把目前系統資料整理成 `TelemetrySnapshot_t`
2. `Telemetry_Task()` 每隔固定時間輸出一行文字

輸出的內容包含：

- mode / substate
- calibration 狀態
- valid 與 ADC / baseline / delta
- error
- command
- encoder count
- manual stage

另外 `Telemetry_SendLine()` 也被 `app_main` 用來輸出簡短回覆，例如：

- `APP READY DUAL MODE`
- `STAT ...`
- `CAL ...`
- `CFG ...`
- `ERR ...`

### 6.10 `stepper_tmc2209.[hc]`

角色：最低層的雙模步進驅動控制。

這一層直接接：

- PWM timer
- TMC2209 UART 設定
- DIR / EN GPIO

它負責：

- 寫入 TMC2209 register
- 設定 PWM 頻率來產生 step pulse
- 切換方向
- 在變速或換向時做頻率 ramp
- 提供 signed step rate API 給上層使用

所以目前系統真正驅動馬達的最終路徑是：

```text
tracker/search/manual
  -> motor_control
  -> StepperTmc2209_SetSignedStepRate()
```

## 7. 重要共用資料結構

### 7.1 `tracking_types.h`

這個檔案是 App 層的共用語言，裡面定義了：

- mode enum
- idle/search substate enum
- serial command enum 與 payload
- `LdrTrackingFrame_t`
- `AxisController_t`
- `MotionCommand_t`
- `TrackingHistoryEntry_t`
- `TelemetrySnapshot_t`

也就是說，模組之間互通的資料格式幾乎都集中在這裡。

### 7.2 `tracking_config.h`

這個檔案集中所有「現在這版」的重要參數，例如：

- boot calibration 時間
- control period
- telemetry period
- LDR 判定門檻
- PI/PID 係數
- 各軸最大 step rate
- search 參數
- serial buffer 長度

這種做法的好處是：調參時不必每個 `.c` 檔案到處找 magic number。

## 8. 目前程式的實作特性

以下不是 bug 清單，而是目前版本的實際寫法特徵。

### 8.1 單執行緒 super loop

整個 App 目前沒有 RTOS，也沒有複雜排程器。

所有事情都發生在：

- `while (1)`
- `AppMain_Task()`

這讓程式流程容易追，但同時也代表任何阻塞式操作都會影響主迴圈延遲。

### 8.2 多處使用 blocking HAL API

目前這版使用了不少同步式呼叫，例如：

- `HAL_UART_Transmit()`
- `HAL_Delay()`

特別是 `stepper_tmc2209.c` 內的 ramp 與方向切換，屬於明確的阻塞式操作。這代表這一版比較偏向功能驗證與 bring-up 友善，而不是高即時性架構。

### 8.3 模組責任切分算清楚，但中樞集中在 `app_main.c`

現在的設計其實已經把感測、判斷、控制、搜尋、輸出拆成獨立模組。

但是 mode transition、控制時序、命令反應仍然全部集中在 `app_main.c`。因此後續如果再增加模式或例外流程，第一個需要整理的檔案通常也會是它。

### 8.4 `uart_sequence` 像是較舊版本的遺留輸出模組

專案裡還有 `uart_sequence.[hc]`，但目前 `app_main.c` 沒有使用它，現行輸出已改由 `telemetry.[hc]` 負責。

可以把它視為較早期 bring-up 階段留下的序列輸出模組。

## 9. 如果之後要改功能，建議怎麼接

### 9.1 想新增一種 mode

建議順序：

1. 先在 `tracking_types.h` 增加 mode enum
2. 在 `AppMain_HandleCommand()` 決定怎麼切進去
3. 在 `AppMain_RunControl()` 補上新 mode 分支
4. 如果需要新的子模組，再新增 `Init()` / `Task()` / `Run()` 介面
5. 最後補 telemetry 顯示

### 9.2 想調追光靈敏度

優先看 `tracking_config.h`：

- `TRACK_VALID_TOTAL_MIN`
- `TRACK_DIRECTION_CONTRAST_MIN`
- `CTRL_ERR_DEADBAND`
- `CTRL_KP_*`
- `CTRL_KI`
- `CTRL_KD`
- `CTRL_AXIS*_MAX_STEP_HZ`
- `CTRL_AXIS*_RATE_LIMIT_STEP_HZ`

### 9.3 想改 LDR 方向定義

優先看 `ldr_tracking.c` 內的索引定義：

- `LDR_IDX_TOP_LEFT`
- `LDR_IDX_TOP_RIGHT`
- `LDR_IDX_BOTTOM_LEFT`
- `LDR_IDX_BOTTOM_RIGHT`

以及 `tracker_controller.c` 內的：

- `CTRL_AXIS1_ERROR_SIGN`
- `CTRL_AXIS2_ERROR_SIGN`

前者影響誤差怎麼算，後者影響誤差對應到馬達正反方向的符號。

## 10. 一句話總結目前這版

目前這個 App 層是「以 `app_main.c` 為核心的 super-loop 狀態機架構」：

- `app_adc` / `app_encoder` 提供感測資料
- `ldr_tracking` 產生追光誤差
- `tracker_controller` 與 `search_strategy` 決定馬達命令
- `motor_control` / `stepper_tmc2209` 執行命令
- `serial_cmd` 提供模式切換入口
- `telemetry` 把系統狀態輸出到 UART

如果要快速理解目前程式，最推薦的閱讀順序是：

1. `Core/Src/main.c`
2. `Core/Src/App/app_main.c`
3. `Core/Inc/App/tracking_types.h`
4. `Core/Inc/App/tracking_config.h`
5. `ldr_tracking.c`
6. `tracker_controller.c`
7. `search_strategy.c`
8. `motor_control.c`
9. `stepper_tmc2209.c`

這樣會最容易從「整體流程」一路走到「底層驅動」。
