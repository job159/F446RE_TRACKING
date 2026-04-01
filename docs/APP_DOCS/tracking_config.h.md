# tracking_config.h

來源: `Core/Inc/App/tracking_config.h`

## 1. 角色

`tracking_config.h` 集中 App 層可調參數，包含：

- 系統時序
- LDR 有效性門檻
- tracker controller 參數
- 馬達輸出上限與變化率
- search 相關常數（目前為 legacy）
- serial parser buffer 常數

## 2. 目前關鍵設定

### 控制週期

- `SYS_CONTROL_PERIOD_DEFAULT_MS = 1U`
- 上電預設 1ms（約 1000Hz）
- runtime 可由 UART `PERIOD 1MS|2MS|5MS` 切換

### 追蹤輸出步幅（目前設定）

- `CTRL_AXIS1_OUTPUT_GAIN = 2.0f`
- `CTRL_AXIS2_OUTPUT_GAIN = 2.0f`

這是目前追蹤輸出放大倍率（相較最初版本已大幅上調）。

### 最大步頻與變化率

- `CTRL_AXIS1_MAX_STEP_HZ = 60000U`
- `CTRL_AXIS2_MAX_STEP_HZ = 60000U`
- `CTRL_AXIS1_RATE_LIMIT_STEP_HZ = 16250U`
- `CTRL_AXIS2_RATE_LIMIT_STEP_HZ = 13750U`

這些值已跟著 output gain 一起拉高，避免被飽和上限過早卡住。

## 3. 失追策略現況

雖然檔案仍保留 `TRACK_LOST_CONSECUTIVE_COUNT` 與 search 常數，
但目前 `app_main.c` 已改成「失追即停住，不進 search 掃描流程」。

也就是：

- search 參數目前不影響主流程
- 若未來恢復 search，再回頭啟用這組常數

## 4. 調整建議

### 想再加快追蹤

- 優先看 `CTRL_AXIS*_OUTPUT_GAIN`
- 搭配 `CTRL_AXIS*_MAX_STEP_HZ`
- 再看 `CTRL_AXIS*_RATE_LIMIT_STEP_HZ`

### 想減少抖動

- 先調大 `CTRL_ERR_DEADBAND`
- 再調低 `CTRL_KP_*` 或 output gain
- 必要時提高 rate limit 平滑度

## 5. 注意事項

- 高 gain + 1ms 週期會讓輸出非常積極，實機可能更容易過衝或失步。
- 若要做保守版，建議優先分軸調整（例如 X 軸保留大步幅、Y 軸先降）。
