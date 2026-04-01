# tracking_types.h

來源: `Core/Inc/App/tracking_types.h`

## 1. 這個檔案的角色

`tracking_types.h` 是整個 App 層共用的資料語言。

很多 App 模組彼此之間不直接知道對方的內部實作，但會共享相同的 enum 與 struct。這個檔案的價值就在於：

- 讓 mode 定義集中
- 讓 command 格式集中
- 讓追光 frame 結構集中
- 讓 telemetry snapshot 格式集中

如果 `tracking_config.h` 比較像「可調參數總表」，那 `tracking_types.h` 就比較像「資料格式總表」。

## 2. 模式相關 enum

### `SystemMode_t`

定義主模式：

- `MODE_IDLE`
- `MODE_TRACKING`
- `MODE_SEARCH`
- `MODE_MANUAL`

這個 enum 幾乎是整個 App 狀態機的核心座標。  
注意：目前 `app_main.c` 的主流程已不主動切入 `MODE_SEARCH`，失追時會直接停住；`MODE_SEARCH` 現在主要是保留相容型別。

### `IdleSubstate_t`

定義 idle 模式內部的兩個細分狀態：

- `IDLE_CALIBRATING`
- `IDLE_WAIT_CMD`

### `SearchSubstate_t`

定義 search 的三個階段：

- `SEARCH_HISTORY_BIAS`
- `SEARCH_REVISIT_LAST_GOOD`
- `SEARCH_SWEEP_SCAN`

目前這組型別屬於 legacy 保留，供 telemetry / 型別相容使用。

## 3. serial command 相關

### `SerialCmdId_t`

定義 parser 解析出來的命令種類，包括：

- mode 切換
- manual stage
- recalibrate
- 各種 query
- control period 切換
- help

### `SerialCmd_t`

這是 serial parser 輸出的結構化命令。

內容包含：

- `id`
- `arg0`
- `arg1`

目前大多數命令只用到 `arg0`，但保留 `arg1` 可以讓未來命令格式更有擴充性。

## 4. 追光與控制資料

### `LdrTrackingFrame_t`

這是 LDR 模組對外最重要的輸出格式，包含：

- `raw[4]`
- `baseline[4]`
- `noise_floor[4]`
- `delta[4]`
- `total`
- `contrast`
- `error_x`
- `error_y`
- `is_valid`
- `calibration_done`

也就是說，一個 frame 不只是「四個 ADC 值」，而是已經包含追光控制需要的完整判斷結果。

### `AxisController_t`

保存單軸控制器狀態：

- `prev_error`
- `integrator`
- `prev_output_hz`

這使得 controller 能跨週期維持狀態，而不是每圈都像純比例控制一樣重算後歸零。

### `MotionCommand_t`

這是 App 層往馬達輸出的標準命令格式：

- `axis1_step_hz`
- `axis2_step_hz`

目前兩軸都用 signed step rate 表示方向與速度。

## 5. search 與歷史資料（legacy）

### `TrackingHistoryEntry_t`

這是先前 search 策略使用的資料格式，現在主流程已停用 search 介入，但 struct 仍保留於共用型別。

這個 struct 保存的就是那種歷史資料：

- `tick_ms`
- `error_x`, `error_y`
- `axis1_cmd_hz`, `axis2_cmd_hz`
- `enc1_count`, `enc2_count`
- `total_light`
- `valid`

## 6. telemetry 資料

### `TelemetrySnapshot_t`

這是輸出層與主控制層之間的資料橋樑。

`app_main.c` 會把目前整體系統狀態整理成一份 snapshot，`telemetry.c` 再把它格式化成 UART 字串。

內容包含：

- 時間戳
- mode / substate
- calibration 與 source valid
- manual stage
- ADC / baseline / delta
- total light / contrast
- encoder count / angle
- command
- `error_x / error_y`

## 7. 為什麼這個檔案值得先看

當你第一次接手專案時，先看這個檔案有個很大的好處：

你會先知道整個系統「資料長什麼樣」，之後再去讀模組時，比較不會被一堆 struct 名稱淹沒。

例如你先知道：

- `LdrTrackingFrame_t` 長什麼樣
- `MotionCommand_t` 長什麼樣
- `TelemetrySnapshot_t` 長什麼樣

再讀 `tracker_controller.c`、`motor_control.c`、`telemetry.c` 會快很多。

## 8. 修改時機

通常以下情況會改到這個檔案：

- 新增主模式或子狀態
- 新增 serial command 類型
- 新增 search / telemetry 需要保存的欄位
- 想讓模組之間共用新的資料格式

## 9. 修改時容易踩雷的地方

### 9.1 這是共用檔，影響面很大

只要改了 struct 欄位名稱或 enum 順序，很可能會同時影響多個模組。

### 9.2 不要把實作細節塞太多進來

這個檔案應該放「共享資料型別」，不是放演算法或模組內部專用 helper。

如果一個型別只在單一 `.c` 檔內部使用，通常應該留在那個 `.c` 裡。
