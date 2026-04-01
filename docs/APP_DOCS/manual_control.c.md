# manual_control.c

來源: `Core/Src/App/manual_control.c`

## 1. 這個檔案的角色

`manual_control.c` 是手動 stage 指令的管理層。

它的職責很單純：

- 接收外部要求的 manual stage
- 先保存成 pending
- 在合適時機真正交給 `motor_control`
- 套用成功後再更新 active stage

## 2. 為什麼不直接收到命令就切馬達

因為目前系統有兩種 manual 指令來源：

- serial `F1..R4 / STAGE`
- 實體按鈕 `B1` 輪切

如果這兩條路都直接去碰馬達，很容易變成：

- 某些情況直接切
- 某些情況延後切
- 某些情況切完 state 沒同步

`manual_control.c` 的兩段式設計，就是要把：

- 「請求某個 stage」
- 「真正套用這個 stage」

拆開來。

## 3. 主要流程

### `ManualControl_SetStage()`

只做一件事：

- 把 `pending_stage` 記起來
- 設 `pending_valid = 1`

這時候不表示馬達已經切換。

### `ManualControl_Task()`

這個函式在 `MODE_MANUAL` 期間被 `app_main.c` 呼叫。

它會：

1. 確認有 pending stage
2. 呼叫 `MotorControl_SetManualStage()`
3. 如果成功，更新 `active_stage` 與 `active_valid`
4. 清掉 `pending_valid`

這表示 active stage 一定是「成功套用後的 stage」。

## 4. 為什麼 active / pending 分開很有價值

這種設計可以清楚回答兩個不同問題：

### 系統現在想切去哪一檔

看 `pending_stage`

### 系統目前真的已經在哪一檔

看 `active_stage`

在除錯或做 telemetry 時，這個差異非常有用。

## 5. `Reset()` 在什麼時候用到

當模式切換或重設系統時，`ManualControl_Reset()` 會把手動狀態清空，避免舊的 pending stage 殘留進新模式。

## 6. 和其他模組的關係

### 上游

- `app_main.c`

### 下游

- `motor_control.c`

## 7. 最常改的地方

### 想讓 manual stage 套用後保留更多資訊

可以擴充 handle，記錄：

- 上次切換時間
- 上次切換來源是 serial 還是 button

### 想讓 manual 支援更多控制方式

例如每軸不同檔位、按住連發、長按加速，通常會先從這個模組開始設計新的狀態管理方式。

## 8. 修改時容易踩雷的地方

### 8.1 `SetStage()` 不等於馬達已經切完

這是最常誤會的地方。

### 8.2 manual 的真實執行還是依賴 `motor_control`

這個檔案只管 stage 狀態管理，不管底層 stepper 是否真的切得動。
