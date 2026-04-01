# tracker_controller.h

來源: `Core/Inc/App/tracker_controller.h`

## 1. 這個檔案的角色

這個 header 定義追光控制器的公開型別與 API。

它的地位是把「追光誤差 -> 馬達命令」這層功能隔離成一個可替換的模組。也就是說，上層不必知道內部是 PID、gain scheduling 還是其他控制方法，只需要知道輸入 frame、輸出 command。

## 2. `TrackerController_HandleTypeDef`

內容包含：

- `AxisController_t axis1`
- `AxisController_t axis2`

每個 `AxisController_t` 來自 `tracking_types.h`，內含：

- `prev_error`
- `integrator`
- `prev_output_hz`

因此這個 handle 的本質是「雙軸控制器的狀態容器」。

## 3. 對外 API

### `TrackerController_Init()`

初始化雙軸控制器狀態。

### `TrackerController_Reset()`

在切模式或重新開始 tracking 時，把積分與歷史誤差清乾淨。

### `TrackerController_Run()`

輸入：

- `TrackerController_HandleTypeDef *handle`
- `const LdrTrackingFrame_t *frame`
- `uint32_t control_period_ms`

輸出：

- `MotionCommand_t`

這個 API 是 App 層拿來做追光控制的唯一正式入口。

## 4. 使用方式

目前典型用法是：

1. 系統初始化時 `Init()`
2. 切進 tracking 前 `Reset()`
3. 每個控制週期呼叫 `Run()`
4. 結果交給 `motor_control.c`

## 5. 修改時機

以下情況通常會改這個 header：

- 想讓控制器保存更多中間狀態
- 想輸出更多 debug 資訊
- 想把雙軸控制器拆得更細

## 6. 修改時要注意

只要這個 header 的型別或 API 改了，通常要一起檢查：

- `tracker_controller.c`
- `app_main.c`
- `tracking_types.h`
