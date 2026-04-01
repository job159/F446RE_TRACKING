# tracking_types.h — 共用資料格式總表

來源: `Core/Inc/App/tracking_types.h`

## 1. 角色

整個 App 層共用的資料語言。各模組彼此之間不直接知道對方的內部實作，但會共享相同的 enum 與 struct。

如果 `tracking_config.h` 是「可調參數總表」，這個檔案就是「資料格式總表」。

**建議第一個讀這個檔案** — 先知道資料長什麼樣，再讀模組會快很多。

---

## 2. 模式 enum

### `SystemMode_t` — 主模式

| 值 | 名稱 | 說明 |
|----|------|------|
| 0 | `MODE_IDLE` | 停止狀態（含校正子狀態） |
| 1 | `MODE_TRACKING` | 自動追光 |
| 2 | `MODE_SEARCH` | legacy，目前 app_main 不主動進入 |
| 3 | `MODE_MANUAL` | 手動八檔控制 |

### `IdleSubstate_t` — IDLE 內部子狀態

| 值 | 名稱 | 說明 |
|----|------|------|
| 0 | `IDLE_CALIBRATING` | 正在做 5 秒開機校正 |
| 1 | `IDLE_WAIT_CMD` | 校正完成，等待命令 |

### `SearchSubstate_t` — Search 子狀態（legacy）

| 值 | 名稱 | 說明 |
|----|------|------|
| 0 | `SEARCH_HISTORY_BIAS` | 沿上次成功方向找 |
| 1 | `SEARCH_REVISIT_LAST_GOOD` | 回到上次好位置 |
| 2 | `SEARCH_SWEEP_SCAN` | 掃描搜尋 |

---

## 3. Serial Command

### `SerialCmdId_t` — 命令種類

| 值 | 名稱 | 對應 UART 命令 |
|----|------|----------------|
| 0 | `SERIAL_CMD_NONE` | 無效 |
| 1 | `SERIAL_CMD_MODE_IDLE` | `IDLE` |
| 2 | `SERIAL_CMD_MODE_TRACKING` | `TRACK` |
| 3 | `SERIAL_CMD_MODE_MANUAL` | `MANUAL` |
| 4 | `SERIAL_CMD_MANUAL_STAGE` | `MAN 1..8` / `MAN F1..R4` |
| 5 | `SERIAL_CMD_RECALIBRATE` | `RECAL` |
| 6 | `SERIAL_CMD_STATUS_QUERY` | `STATUS` |
| 7 | `SERIAL_CMD_CAL_QUERY` | `CALDATA` |
| 8 | `SERIAL_CMD_CONFIG_QUERY` | `CONFIG` |
| 9 | `SERIAL_CMD_CONTROL_PERIOD` | `PERIOD 1MS\|2MS\|5MS` |
| 10 | `SERIAL_CMD_HELP` | `HELP` |

### `SerialCmd_t` — 命令結構

```c
typedef struct {
  SerialCmdId_t id;    // 命令種類
  int32_t arg0;        // 主參數（例如 stage=0~7、period=1/2/5）
  int32_t arg1;        // 保留給未來擴充
} SerialCmd_t;
```

---

## 4. 追光核心資料

### `LdrTrackingFrame_t` — LDR 追光幀（最重要的 struct）

這是 `ldr_tracking.c` 每個週期計算的結果，也是 `tracker_controller.c` 的輸入。

```c
typedef struct {
  uint16_t raw[4];           // ADC 原始值（已經過濾波）
  uint16_t baseline[4];      // 校正期間的平均值
  uint16_t noise_floor[4];   // 校正期間的雜訊寬度 + margin
  uint16_t delta[4];         // max(raw - effective_baseline, 0)
  uint16_t total;            // delta[0] + delta[1] + delta[2] + delta[3]
  uint16_t contrast;         // max(delta) - min(delta)
  float    error_x;          // (right - left) / total，歸一化 [-1, +1]
  float    error_y;          // (top - bottom) / total，歸一化 [-1, +1]
  uint8_t  is_valid;         // 1 = 有足夠光且有方向，可以追
  uint8_t  calibration_done; // 1 = 校正已完成
} LdrTrackingFrame_t;
```

**error 方向定義：**
- `error_x > 0` → 右側比較亮 → 馬達應向右追
- `error_y > 0` → 上側比較亮 → 馬達應向上追

**LDR 實體排列（順時鐘編號）：**
```
raw[0]=TL   raw[1]=TR
raw[3]=BL   raw[2]=BR
```

### `AxisController_t` — 單軸控制器狀態

```c
typedef struct {
  float    prev_error;       // 上一週期的誤差（微分用）
  float    integrator;       // 積分累積值
  int32_t  prev_output_hz;   // 上一週期的輸出（rate limit 用）
} AxisController_t;
```

### `MotionCommand_t` — 馬達命令

```c
typedef struct {
  int32_t axis1_step_hz;   // 正=forward，負=reverse，0=停
  int32_t axis2_step_hz;   // 同上
} MotionCommand_t;
```

tracking 和 manual 最終都產生這個結構，交給 `motor_control.c`。

---

## 5. 歷史與搜尋資料（legacy）

### `TrackingHistoryEntry_t`

```c
typedef struct {
  uint32_t tick_ms;          // 時間戳
  float    error_x;          // 當時的 error_x
  float    error_y;          // 當時的 error_y
  int32_t  axis1_cmd_hz;     // 當時的 axis1 命令
  int32_t  axis2_cmd_hz;     // 當時的 axis2 命令
  int32_t  enc1_count;       // 當時的 encoder1 位置
  int32_t  enc2_count;       // 當時的 encoder2 位置
  uint16_t total_light;      // 當時的 total
  uint8_t  valid;            // 是否有效
} TrackingHistoryEntry_t;
```

目前 search 已停用，但 struct 保留供未來恢復使用。

---

## 6. Telemetry 快照

### `TelemetrySnapshot_t` — 系統狀態快照

`app_main.c` 每圈整理一份，`telemetry.c` 負責格式化輸出。

```c
typedef struct {
  uint32_t tick_ms;              // 時間戳
  SystemMode_t mode;             // 主模式
  IdleSubstate_t idle_substate;  // IDLE 子狀態
  SearchSubstate_t search_substate; // （legacy）
  uint8_t  calibration_done;     // 校正是否完成
  uint8_t  source_valid;         // 追光是否有效
  uint8_t  manual_stage_valid;   // manual stage 是否有效
  uint8_t  manual_stage;         // 目前 manual stage
  uint16_t adc[4];               // raw ADC
  uint16_t baseline[4];          // 校正基線
  uint16_t delta[4];             // 有效差值
  uint16_t total_light;          // delta 總和
  uint16_t contrast;             // 對比度
  int32_t  enc1_count;           // encoder1 累積 count
  int32_t  enc2_count;           // encoder2 累積 count
  uint32_t enc1_angle_x10000;    // encoder1 角度 (×10000)
  uint32_t enc2_angle_x10000;    // encoder2 角度 (×10000)
  int32_t  cmd_axis1_hz;         // 目前 axis1 命令
  int32_t  cmd_axis2_hz;         // 目前 axis2 命令
  int32_t  error_x_x1000;       // error_x × 1000（整數格式）
  int32_t  error_y_x1000;       // error_y × 1000（整數格式）
} TelemetrySnapshot_t;
```

---

## 7. 修改時機

- 新增主模式或子狀態 → 改 enum
- 新增 serial command → 改 `SerialCmdId_t`
- LDR frame 要多保存的欄位 → 改 `LdrTrackingFrame_t`
- telemetry 要多輸出的資料 → 改 `TelemetrySnapshot_t`
- 模組間要共用新資料格式 → 在這裡定義

## 8. 踩雷提醒

- **影響面很大** — 改了 struct 欄位名稱或 enum 順序，多個模組需要同步改
- **不要放實作細節** — 這裡只放共享資料型別，不放演算法或模組內部 helper
- **單一模組專用的型別** — 留在該模組的 `.c` 裡，不要塞進來
