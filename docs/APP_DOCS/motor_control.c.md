# motor_control.c

來源: `Core/Src/App/motor_control.c`

## 1. 這個檔案的角色

`motor_control.c` 是 App 層控制邏輯與底層步進驅動之間的轉接層。

它的工作不是自己決定要不要追光，而是把上層給的命令：

- tracking command
- search command
- manual stage

轉成兩顆 `StepperTmc2209_HandleTypeDef` 實際可執行的操作。

## 2. 為什麼需要這一層

如果 `app_main.c` 直接操作 `StepperTmc2209_*`，它就得知道：

- 哪個 GPIO 是 axis1 DIR
- 哪個 UART 是 axis2 TMC
- speed table 怎麼排
- manual stage 怎麼映射成正反方向

這些都屬於驅動層與機構配置細節，不應該散在主流程中。

所以 `motor_control.c` 的目的，就是把「雙軸步進控制」封裝成上層可直接用的 API。

## 3. handle 內保存什麼

`MotorControl_HandleTypeDef` 內有：

- `axis1`
- `axis2`
- `last_motion_command`
- `manual_stage`
- `manual_stage_valid`

這讓它除了能控制雙軸，也能記住：

- 上次真正成功送出的 motion command
- 目前手動模式用的是哪個 stage

## 4. 初始化流程：`MotorControl_Init()`

初始化時會：

1. 準備一份 8 檔 speed table
2. 清空 last command 與 manual stage 狀態
3. 初始化 axis1 的 `StepperTmc2209`
4. 初始化 axis2 的 `StepperTmc2209`
5. 呼叫 `MotorControl_StopAll()`

### speed table 的意義

目前 speed table 長這樣：

```text
0~3 = forward 四檔
4~7 = reverse 四檔
```

所以 manual mode 的 `F1..F4` 與 `R1..R4` 本質上就是在選 speed table 的索引。

## 5. 主要控制 API

### `MotorControl_StopAll()`

讓兩軸都停止，並把最後命令記錄設回 0。

### `MotorControl_ApplySignedStepHz()`

這是 tracking / search 最常走的路徑。

它接受：

- `MotionCommand_t`

然後分別對兩軸呼叫 `StepperTmc2209_SetSignedStepRate()`。

### `MotorControl_SetManualStage()`

這是 manual mode 的主要輸入。

它會：

1. 對 axis1 套用指定 stage
2. 對 axis2 套用指定 stage
3. 若兩軸都成功，更新 `manual_stage`
4. 同步把 `last_motion_command` 更新成該 stage 對應的 signed step rate

這裡有一個重點：

目前 manual stage 是「同一個 stage 套到兩軸」，不是一軸一檔分開控。

## 6. `MotorControl_StageToSignedHz()` 的作用

這個 helper 主要是把 stage index 轉回人類較容易理解的 signed step rate。

例如：

- stage `0` 代表正轉低速
- stage `5` 代表反轉第二檔

把它轉成 signed step rate 後，telemetry 或 status summary 比較能反映目前 manual stage 的真實方向與速度。

## 7. 和其他模組的關係

### 上游

- `app_main.c`
- `manual_control.c`
- `tracker_controller.c`
- `search_strategy.c`

### 下游

- `stepper_tmc2209.c`

## 8. 最常改的地方

### 想改 manual 四檔速度

改這裡的 speed table。

### 想讓正反不同速

也是改這裡的 speed table，因為前四格與後四格可以分開設。

### 想把 manual mode 改成兩軸不同 stage

這時就不是只改 speed table，而是要改 `MotorControl_SetManualStage()` 的設計。

## 9. 修改時容易踩雷的地方

### 9.1 這層不該做模式決策

它應該只負責「怎麼執行」，不要在這裡決定何時進 tracking / manual。

### 9.2 manual stage 與 signed step command 是兩種控制路徑

- tracking / search 走 signed step rate
- manual 走 stage

這兩條路最後都到同一顆 stepper，但抽象層不同，不要混在同一個 API 裡硬塞。
