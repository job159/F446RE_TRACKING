# manual_control.h

來源: `Core/Inc/App/manual_control.h`

## 1. 這個檔案的角色

這個 header 定義 manual control 模組的公開介面。

它的重要性在於：雖然 manual 模組本身很小，但它是 serial 手動命令、實體按鈕輪切、主狀態機與馬達層之間的共同交會點。這個 header 就是那個交會點的邊界定義。

## 2. `ManualControl_HandleTypeDef`

這個 handle 只有 4 個欄位，但概念非常重要：

- `pending_stage`
- `pending_valid`
- `active_stage`
- `active_valid`

### `pending_*` 是什麼

表示：

- 外部已經提出想切換的手動檔位
- 但尚未保證馬達真的切換完成

### `active_*` 是什麼

表示：

- 馬達層已成功套用
- 系統現在真的在這個 stage

這個區分讓 manual 模組可以乾淨地處理「請求」與「已生效狀態」。

## 3. 對外 API

### `ManualControl_Init()`

初始化 handle。

### `ManualControl_Reset()`

把 pending 與 active 狀態都清掉，通常在切模式或重新開始某個流程時使用。

### `ManualControl_SetStage()`

登記一個想切換的 stage。這個函式不保證馬達立刻切完。

### `ManualControl_Task()`

真正把 pending stage 交給 `motor_control` 套用的函式。

### `ManualControl_IsStageValid()`

檢查目前 active stage 是否有效。

### `ManualControl_GetStage()`

讀目前 active stage。

## 4. 使用模式

目前 App 層的典型用法是：

1. serial 或實體按鈕決定要切哪一檔
2. 呼叫 `ManualControl_SetStage()`
3. 進入 `MODE_MANUAL` 後由 `ManualControl_Task()` 真正套用
4. telemetry 透過 `IsStageValid()` / `GetStage()` 顯示目前檔位

## 5. 修改時機

以下情況通常會改這個 header：

- 你想保存更多 manual 狀態，例如來源、時間戳
- 想支援每軸不同的手動檔位
- 想支援長按、連發、暫停恢復等更複雜手動模式

## 6. 修改時要注意

這個 header 雖小，但它改了之後通常會連動：

- `app_main.c`
- `manual_control.c`
- `telemetry.c`

所以若欄位或 API 變更，最好一起檢查這三個模組。
