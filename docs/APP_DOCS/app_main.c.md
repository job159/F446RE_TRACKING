# app_main.c

來源: `Core/Src/App/app_main.c`

## 1. 角色總覽

`app_main.c` 是 App 層主控中樞，負責：

- 初始化各子模組
- 處理 UART 指令與實體按鈕
- 依固定週期執行控制回圈
- 管理模式切換與 telemetry snapshot

目前實際運行模式以 `IDLE / TRACKING / MANUAL` 為主；`MODE_SEARCH` enum 仍保留在型別內，但 `app_main` 已不再進入搜尋流程。

## 2. 目前核心行為

### 開機流程

1. 開機先進 `IDLE + CALIBRATING`
2. 做 `SYS_BOOT_CALIBRATION_MS` 校正
3. 校正完成預設進 `TRACKING`
4. 若校正期間先收到 manual 指令，校正後改進 `MANUAL`

### 控制週期

- runtime 週期存放在 `g_app.control_period_ms`
- 預設由 `SYS_CONTROL_PERIOD_DEFAULT_MS` 載入
- 目前預設是 `1ms`
- scheduler 以 `HAL_GetTick()` 比對執行控制回圈

### 失追處理（已停用 search）

`MODE_TRACKING` 時若 `frame.is_valid == 0`：

- 立即 `TrackerController_Reset()`
- 立即 `MotorControl_StopAll()`
- 不再切入 `MODE_SEARCH`

也就是「沒追到就停住」。

## 3. 指令處理重點

`AppMain_HandleCommand()` 現在主要吃這組清晰命令：

- `IDLE`
- `TRACK`
- `MANUAL`
- `MAN 1..8`（也可 `MAN F1..F4` / `MAN R1..R4`）
- `PERIOD 1MS|2MS|5MS`
- `RECAL`
- `STATUS`
- `CALDATA`
- `CONFIG`
- `HELP`

同時保留舊格式相容別名（如 `MODE 0/1/2`、`STAT?`、`CFG?`、`CAL?`、`CTL`、`F1..R4`、`STAGE 0..7`）。

## 4. 按鈕行為

`B1` 目前是 polling + debounce：

- 每按一次輪切 manual stage
- 走向為 `F1 -> F2 -> F3 -> F4 -> R1 -> R2 -> R3 -> R4 -> ...`
- 與 serial manual 命令共用同一條 `AppMain_RequestManualStage()` 路徑

## 5. 狀態與回覆字串

目前回覆字串已改為較清楚格式：

- `STATUS ...`
- `CALDATA ...`
- `CONFIG ...`
- `PERIOD XMS OK`
- `ERR period_ms only 1|2|5`

## 6. 常改區域

- 模式切換規則：`AppMain_HandleCommand()`, `AppMain_RunControl()`
- 失追策略：`AppMain_RunTracking()`
- 週期設定：`AppMain_SetControlPeriod()`, scheduler 週期判斷
- 外部可觀測資訊：`AppMain_SendStatusSummary()`, `AppMain_SendConfigSummary()`, `AppMain_SendHelp()`

## 7. 注意事項

- 目前 `search_strategy` 模組仍存在於專案，但 `app_main` 不再使用它進行追丟後掃描。
- 若未來要恢復 search，優先檢查：
  `MODE_SEARCH` 進入條件、`AppMain_RunSearch()` 路徑、telemetry 顯示與文件描述是否一致。
