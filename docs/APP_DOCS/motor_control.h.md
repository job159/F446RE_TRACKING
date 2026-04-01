# motor_control.h

來源: `Core/Inc/App/motor_control.h`

## 1. 這個檔案的角色

宣告雙軸 motor control 的 handle 與對外控制 API。

## 2. `MotorControl_HandleTypeDef`

包含：

- `StepperTmc2209_HandleTypeDef axis1`
- `StepperTmc2209_HandleTypeDef axis2`
- `MotionCommand_t last_motion_command`
- `uint8_t manual_stage`
- `uint8_t manual_stage_valid`

這代表 handle 內既包含底層驅動 handle，也保存了 App 層需要知道的最後狀態。

## 3. 對外 API

### 初始化與停止

- `MotorControl_Init()`
- `MotorControl_StopAll()`

### signed step 控制

- `MotorControl_ApplySignedStepHz()`

### manual stage 控制

- `MotorControl_SetManualStage()`
- `MotorControl_ClearManualStage()`
- `MotorControl_IsManualStageValid()`
- `MotorControl_GetManualStage()`

## 4. 使用方式

- tracking / search 期間主要使用 `ApplySignedStepHz()`
- manual 期間主要使用 `SetManualStage()`

## 5. 這個 header 什麼時候會改

- 需要增加 per-axis manual control
- 需要增加 enable/disable 或 fault 狀態
- handle 要保存更多執行層狀態
