# tracker_controller.c

來源: `Core/Src/App/tracker_controller.c`

## 1. 角色

`tracker_controller.c` 把 `error_x / error_y` 轉成兩軸 `step_hz` 命令。

輸出格式：

```c
MotionCommand_t { axis1_step_hz, axis2_step_hz }
```

## 2. 目前控制特性

- deadband
- 依誤差區間切 `Kp`（gain scheduling）
- 條件式 integrator
- derivative
- 正反方向 scale 補償
- 輸出飽和限制
- 變化率限制（rate limit）

## 3. 這版調整重點

### 步幅再放大

- `CTRL_AXIS1_OUTPUT_GAIN = 2.0f`
- `CTRL_AXIS2_OUTPUT_GAIN = 2.0f`

### 對應上限同步提高

- `CTRL_AXIS1_MAX_STEP_HZ = 60000`
- `CTRL_AXIS2_MAX_STEP_HZ = 60000`
- `CTRL_AXIS1_RATE_LIMIT_STEP_HZ = 16250`
- `CTRL_AXIS2_RATE_LIMIT_STEP_HZ = 13750`

避免 output gain 放大後被舊上限直接夾住。

## 4. 與 1ms 週期的關係

`TrackerController_Run(..., control_period_ms)` 會用週期算 `dt_s`。
目前系統預設 `control_period_ms = 1`，因此：

- derivative 對變化更敏感
- rate limit 以更高更新率連續生效
- 實機反應速度更快，但抖動風險也會上升

## 5. 維護建議

- 想再兇一點：先看 `OUTPUT_GAIN`，再看 `MAX_STEP_HZ`。
- 想穩一點：先調 `DEADBAND`、`Kp`，再調 `RATE_LIMIT`。
- 若方向反了：先檢查 `CTRL_AXIS*_ERROR_SIGN` 與 LDR 映射，不要先怪控制器公式。
