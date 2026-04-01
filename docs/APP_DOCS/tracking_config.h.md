# tracking_config.h — 全參數調適手冊

來源: `Core/Inc/App/tracking_config.h`

## 1. 角色

集中 App 層所有可調參數。這是調適追蹤手感的**唯一起點**。

修改這個檔案不需要動任何 `.c` 的邏輯，只需要重新編譯。

---

## 2. 參數總表（目前值）

### 2.1 系統時序

| 參數 | 值 | 單位 | 說明 |
|------|-----|------|------|
| `SYS_BOOT_CALIBRATION_MS` | 5000 | ms | 開機校正期間長度 |
| `SYS_CONTROL_PERIOD_DEFAULT_MS` | 1 | ms | 預設控制週期（約 1000 Hz） |
| `SYS_TELEMETRY_PERIOD_MS` | 100 | ms | telemetry 輸出週期（10 Hz） |

**調適說明：**
- 校正時間越長，baseline 越穩定，但開機等待越久
- 控制週期可在 runtime 用 `PERIOD 1MS|2MS|5MS` 切換
- telemetry 頻率開太高會吃主迴圈時間（`HAL_UART_Transmit` 是 blocking）

### 2.2 LDR 校正與有效性門檻

| 參數 | 值 | 說明 |
|------|-----|------|
| `LDR_CHANNEL_COUNT` | 4 | LDR 通道數 |
| `LDR_BASELINE_MARGIN` | 10 | 加在 noise_span 上的額外安全餘量 |
| `LDR_MIN_NOISE_FLOOR` | 6 | 最低 noise floor（即使實測雜訊很低也至少保留這麼多） |
| `TRACK_VALID_TOTAL_MIN` | 140 | 四顆 delta 總和 ≥ 此值才算「有光」 |
| `TRACK_DIRECTION_CONTRAST_MIN` | 28 | max_delta − min_delta ≥ 此值才算「有方向」 |
| `TRACK_REACQUIRE_CONSECUTIVE_COUNT` | 2 | （目前未使用） |
| `TRACK_LOST_CONSECUTIVE_COUNT` | 3 | （目前未使用，search 已停用） |

**校正公式：**
```
baseline[i] = 校正期間 raw[i] 的平均值
noise_span[i] = 校正期間 raw[i] 的 max - min
noise_floor[i] = max(noise_span[i] + LDR_BASELINE_MARGIN, LDR_MIN_NOISE_FLOOR)
effective_baseline[i] = baseline[i] + noise_floor[i]
delta[i] = max(raw[i] - effective_baseline[i], 0)
```

**有效性判斷（三個條件同時滿足）：**
```
is_valid = (calibration_done != 0)
        && (total >= TRACK_VALID_TOTAL_MIN)        // 有足夠光
        && (contrast >= TRACK_DIRECTION_CONTRAST_MIN)  // 有明確方向
```

**調適建議：**

| 問題 | 調法 |
|------|------|
| 弱光環境追不到 | 降低 `TRACK_VALID_TOTAL_MIN`（例如 80） |
| 四顆 LDR 都亮但判定無效 | 降低 `TRACK_DIRECTION_CONTRAST_MIN`（例如 15） |
| 室內雜光誤觸追蹤 | 提高 `TRACK_VALID_TOTAL_MIN` 和 `TRACK_DIRECTION_CONTRAST_MIN` |
| 校正後背景扣除不乾淨 | 提高 `LDR_BASELINE_MARGIN` |

### 2.3 PID 控制器參數

#### 死區與誤差區間

| 參數 | 值 | 說明 |
|------|-----|------|
| `CTRL_ERR_DEADBAND` | 0.015 | \|error\| ≤ 此值 → 輸出 0，積分衰減至 80% |
| `CTRL_ERR_SMALL` | 0.055 | \|error\| ≤ 此值 → 用 `KP_SMALL` |
| `CTRL_ERR_MEDIUM` | 0.140 | \|error\| ≤ 此值 → 用 `KP_MEDIUM`，且允許積分累積 |
| （大於 MEDIUM） | — | 用 `KP_LARGE`，不累積積分 |

#### PID 增益

| 參數 | 值 | 說明 |
|------|-----|------|
| `CTRL_KP_SMALL` | 180.0 | 小誤差時的比例增益（精細跟蹤用） |
| `CTRL_KP_MEDIUM` | 360.0 | 中誤差時的比例增益 |
| `CTRL_KP_LARGE` | 620.0 | 大誤差時的比例增益（快速回追用） |
| `CTRL_KI` | 8.0 | 積分增益（只在 \|error\| ≤ MEDIUM 時累積） |
| `CTRL_KD` | 18.0 | 微分增益 |

**控制器公式：**
```
dt = control_period_ms / 1000.0

if |error| <= DEADBAND:
    integrator *= 0.8    // 衰減
    output = 0
    return

kp = SelectKp(|error|)  // gain scheduling
derivative = (error - prev_error) / dt
if |error| <= MEDIUM:
    integrator += error * KI * dt

output = (kp * error) + integrator + (KD * derivative)
output *= OUTPUT_GAIN
output *= (output >= 0) ? POS_SCALE : NEG_SCALE
output = clamp(output, -MAX_STEP_HZ, +MAX_STEP_HZ)
output = rate_limit(output, prev_output, RATE_LIMIT_STEP_HZ)
```

**Gain Scheduling 設計意義：**
- 小誤差（已接近對準）→ 低 Kp 避免過衝振盪
- 大誤差（偏離很遠）→ 高 Kp 加速回追
- 積分只在中小誤差時累積，避免大偏移時積分飽和

#### 輸出放大與方向補償

| 參數 | 值 | 說明 |
|------|-----|------|
| `CTRL_AXIS1_OUTPUT_GAIN` | 2.0 | Axis1 總輸出放大倍率 |
| `CTRL_AXIS2_OUTPUT_GAIN` | 2.0 | Axis2 總輸出放大倍率 |
| `CTRL_AXIS1_POS_SCALE` | 1.10 | Axis1 正方向補償（機構不對稱修正） |
| `CTRL_AXIS1_NEG_SCALE` | 1.24 | Axis1 負方向補償 |
| `CTRL_AXIS2_POS_SCALE` | 1.02 | Axis2 正方向補償 |
| `CTRL_AXIS2_NEG_SCALE` | 1.16 | Axis2 負方向補償 |
| `CTRL_AXIS1_ERROR_SIGN` | 1.0 | Axis1 誤差方向（改 -1.0 反轉） |
| `CTRL_AXIS2_ERROR_SIGN` | 1.0 | Axis2 誤差方向（改 -1.0 反轉） |

**POS_SCALE / NEG_SCALE 的用途：**
補償機構正反方向阻力不同。例如重力讓向下比向上容易，就讓向上的 scale 大一點。

#### 輸出限制

| 參數 | 值 | 單位 | 說明 |
|------|-----|------|------|
| `CTRL_AXIS1_MAX_STEP_HZ` | 60000 | Hz | Axis1 最大步頻 |
| `CTRL_AXIS2_MAX_STEP_HZ` | 60000 | Hz | Axis2 最大步頻 |
| `CTRL_AXIS1_RATE_LIMIT_STEP_HZ` | 16250 | Hz/週期 | Axis1 每週期最大變化量 |
| `CTRL_AXIS2_RATE_LIMIT_STEP_HZ` | 13750 | Hz/週期 | Axis2 每週期最大變化量 |

**Rate Limit 說明：**
- 以 1ms 週期為例：`16250 Hz/週期` = 每秒最多加速 16,250,000 Hz
- 這數字很大，代表目前加速限制相當寬鬆
- 若馬達失步，可降低此值讓加速更平滑

### 2.4 Search 參數（legacy，目前不影響主流程）

| 參數 | 值 | 說明 |
|------|-----|------|
| `SEARCH_HISTORY_LEN` | 16 | 歷史 ring buffer 長度 |
| `SEARCH_BIAS_STEP_HZ` | 900 | 沿上次成功方向的步頻 |
| `SEARCH_BIAS_HOLD_MS` | 100 | bias 階段每步維持時間 |
| `SEARCH_HISTORY_BIAS_CYCLES` | 6 | bias 階段循環次數 |
| `SEARCH_REVISIT_STEP_HZ` | 650 | 回到上次好位置的步頻 |
| `SEARCH_REVISIT_MAX_MS` | 1000 | revisit 階段最長時間 |
| `SEARCH_REVISIT_TOL_COUNTS` | 80 | 接近上次好位置的容忍量 |
| `SEARCH_SWEEP_STEP_HZ` | 750 | X 軸掃描步頻 |
| `SEARCH_SWEEP_Y_STEP_HZ` | 420 | Y 軸掃描步頻 |
| `SEARCH_SWEEP_HOLD_MS` | 120 | 掃描每步維持時間 |

### 2.5 Serial 解析器

| 參數 | 值 | 說明 |
|------|-----|------|
| `SERIAL_CMD_RX_LINE_MAX` | 32 | 單行命令最大字元數 |
| `SERIAL_CMD_QUEUE_LENGTH` | 4 | 待處理命令佇列深度 |

---

## 3. 調適流程建議

### 第一步：確認感測端正常

1. 送 `CALDATA` 確認 baseline 合理（室內約 200~600，戶外可能更高）
2. 送 `STATUS` 觀察 `total` 和 `contrast`
3. 用手電筒對準 LDR → `total` 應上升、`is_valid` 應為 1

### 第二步：調有效性門檻

- 如果光源明確但 `is_valid = 0` → 降 `TRACK_VALID_TOTAL_MIN` 和 `TRACK_DIRECTION_CONTRAST_MIN`
- 如果雜光誤觸 → 提高這兩個門檻

### 第三步：調控制器

1. **先調 DEADBAND** — 消除對準時的微小振盪
2. **再調 KP_SMALL** — 控制精細跟蹤的靈敏度
3. **再調 OUTPUT_GAIN** — 整體加速或減速
4. **最後調 RATE_LIMIT** — 如果馬達失步才需要收緊

### 第四步：調方向

- 若追反方向 → 改 `ERROR_SIGN` 為 -1.0
- 若正反速不同 → 調 `POS_SCALE` / `NEG_SCALE`

---

## 4. 與控制週期的交互影響

| 週期 | 特性 |
|------|------|
| 1ms（預設） | 反應最快，微分最敏感，抖動風險最高 |
| 2ms | 中間值，通常是較穩的選擇 |
| 5ms | 反應較慢，但對機構較溫和 |

注意：rate limit 是「每週期」為單位，週期越短等效加速越快。
