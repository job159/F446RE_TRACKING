# stepper_tmc2209.h

來源: `Core/Inc/App/stepper_tmc2209.h`

## 1. 這個檔案的角色

宣告單軸 TMC2209 驅動 handle、常數與控制 API。

## 2. 重要常數

### speed stage 相關

- `STEPPER_TMC2209_SPEED_STAGE_COUNT`
- `STEPPER_TMC2209_DIRECTION_SPLIT_STAGE`

目前 stage count 是 8，前 4 檔視為 forward，後 4 檔視為 reverse。

### 方向與使能極性

- `STEPPER_TMC2209_DIR_FORWARD`
- `STEPPER_TMC2209_DIR_REVERSE`
- `STEPPER_TMC2209_ENABLE`
- `STEPPER_TMC2209_DISABLE`

這些定義直接影響實體驅動板控制極性。

## 3. `StepperTmc2209_HandleTypeDef`

內容包含：

- PWM timer 與 channel
- TMC UART handle
- DIR / EN GPIO
- slave address
- speed table
- speed index
- current step rate
- current direction

也就是說，這個 handle 足夠描述一顆 stepper driver 的完整 runtime 狀態。

## 4. 對外 API

### 初始化

- `StepperTmc2209_Init()`

### stage 模式

- `StepperTmc2209_SetSpeedStage()`
- `StepperTmc2209_NextSpeedStage()`

### step rate 模式

- `StepperTmc2209_SetStepHz()`
- `StepperTmc2209_SetSignedStepRate()`
- `StepperTmc2209_Stop()`

### 直接 GPIO 控制

- `StepperTmc2209_SetDirection()`
- `StepperTmc2209_SetEnable()`

### 查詢

- `StepperTmc2209_GetSpeedStage()`

## 5. 修改時機

通常在以下情況會改這個 header：

- speed stage 數量要改
- handle 需要更多狀態
- 要新增 fault / status 介面
