# tracker_controller.c — PID 追光控制器（調適核心）

來源: `Core/Src/App/tracker_controller.c`

## 1. 角色

把 `error_x / error_y` 轉成兩軸 `step_hz` 命令。

這是追蹤手感的**核心決定者**。改了這裡的參數，馬達追光的速度、精度、穩定性都會直接改變。

---

## 2. 輸入 / 輸出

### 輸入
```
LdrTrackingFrame_t *frame    // 主要用 error_x, error_y, is_valid
uint32_t control_period_ms   // 1, 2, 或 5 ms
```

### 輸出
```
MotionCommand_t { axis1_step_hz, axis2_step_hz }
// 正值=forward, 負值=reverse, 0=停
```

---

## 3. 控制演算法完整流程

每個軸獨立計算，以下以單軸為例：

```
輸入: error (float), 例如 error_x * CTRL_AXIS1_ERROR_SIGN

Step 1: 計算 dt
    dt = control_period_ms / 1000.0    // 1ms → 0.001s

Step 2: 死區判斷
    if |error| <= CTRL_ERR_DEADBAND (0.015):
        integrator *= 0.8    // 緩慢衰減，不歸零
        prev_output_hz = 0
        return 0             // 不輸出

Step 3: Gain Scheduling（選擇 Kp）
    if |error| <= 0.055:   kp = CTRL_KP_SMALL  = 180
    elif |error| <= 0.140:  kp = CTRL_KP_MEDIUM = 360
    else:                   kp = CTRL_KP_LARGE  = 620

Step 4: 微分
    derivative = (error - prev_error) / dt

Step 5: 條件式積分
    if |error| <= CTRL_ERR_MEDIUM (0.140):
        integrator += error * CTRL_KI (8.0) * dt
    // 大誤差時不累積，避免積分飽和

Step 6: PID 合成
    output = (kp * error) + integrator + (CTRL_KD (18.0) * derivative)

Step 7: 輸出放大
    output *= OUTPUT_GAIN (2.0)

Step 8: 方向補償
    if output >= 0: output *= POS_SCALE (1.10 / 1.02)
    else:           output *= NEG_SCALE (1.24 / 1.16)

Step 9: 飽和限制
    output = clamp(output, -MAX_STEP_HZ (60000), +MAX_STEP_HZ (60000))

Step 10: 變化率限制
    delta = output - prev_output_hz
    if delta > RATE_LIMIT (16250):  output = prev_output + RATE_LIMIT
    if delta < -RATE_LIMIT:         output = prev_output - RATE_LIMIT

Step 11: 更新狀態
    prev_error = error
    prev_output_hz = output
    return output
```

---

## 4. Gain Scheduling 設計意義

```
     |error|
  0        0.015      0.055       0.140        1.0
  |---死區---|---小誤差---|---中誤差---|---大誤差----|
  | output=0 | Kp=180   | Kp=360   | Kp=620    |
  |          | +KI +KD  | +KI +KD  | +KD only  |
```

- **小誤差**（已接近對準）：低 Kp 避免過衝 + 積分消除穩態偏移
- **中誤差**：中等 Kp + 積分
- **大誤差**（偏離很遠）：高 Kp 快速回追 + **不累積積分**避免飽和
- **死區**：積分衰減（×0.8），不歸零 → 下次離開死區時不會跳變

---

## 5. 各參數對手感的影響

### 5.1 追蹤速度（想要更快）

| 優先順序 | 調什麼 | 目前值 | 效果 |
|----------|--------|--------|------|
| 1 | `CTRL_AXIS*_OUTPUT_GAIN` | 2.0 | **最直接** — 所有輸出等比放大 |
| 2 | `CTRL_KP_LARGE` | 620 | 影響大偏移時的回追速度 |
| 3 | `CTRL_AXIS*_MAX_STEP_HZ` | 60000 | 需跟 gain 一起拉高，否則被飽和卡住 |
| 4 | `CTRL_AXIS*_RATE_LIMIT_STEP_HZ` | 16250/13750 | 加速上限 |

### 5.2 追蹤穩定性（想要更穩，減少振盪）

| 優先順序 | 調什麼 | 目前值 | 效果 |
|----------|--------|--------|------|
| 1 | `CTRL_ERR_DEADBAND` | 0.015 | **最直接** — 加大死區消除微小抖動 |
| 2 | `CTRL_KP_SMALL` | 180 | 降低精細跟蹤時的反應 |
| 3 | `CTRL_KD` | 18.0 | 降低微分（微分放大雜訊） |
| 4 | `CTRL_AXIS*_RATE_LIMIT_STEP_HZ` | 16250/13750 | 降低讓速度變化更平滑 |

### 5.3 穩態精度（對準後仍有偏移）

| 調什麼 | 效果 |
|--------|------|
| `CTRL_KI` (8.0) | 加大積分增益，消除穩態偏移 |
| `CTRL_ERR_DEADBAND` (0.015) | 縮小死區，但可能引入抖動 |
| `CTRL_ERR_MEDIUM` (0.140) | 擴大積分允許的誤差範圍 |

### 5.4 方向與速度不對稱

| 調什麼 | 用途 |
|--------|------|
| `CTRL_AXIS*_ERROR_SIGN` | 1.0 或 -1.0，反轉追蹤方向 |
| `CTRL_AXIS*_POS_SCALE / NEG_SCALE` | 補償正反方向機構阻力差異 |

---

## 6. 與控制週期的關係

| 週期 | dt | 微分 | 積分 | Rate Limit |
|------|----|------|------|------------|
| 1ms | 0.001s | 非常敏感 | 每秒累積 1000 次 | 等效 16.25M Hz/s |
| 2ms | 0.002s | 較不敏感 | 每秒累積 500 次 | 等效 8.125M Hz/s |
| 5ms | 0.005s | 較鈍 | 每秒累積 200 次 | 等效 3.25M Hz/s |

- 週期越短，微分對變化越敏感（可能放大雜訊）
- 週期越短，rate limit 等效加速越快
- 切換週期後，手感會明顯不同，可能需要重調參數

---

## 7. Reset 行為

`TrackerController_Reset()` = `Init()`，memset 歸零：

- `prev_error = 0`
- `integrator = 0`
- `prev_output_hz = 0`

在以下時機被呼叫：
- 失追時（`is_valid = 0`）
- 切模式時（進入 TRACKING / MANUAL / IDLE）
- 重新校正時

---

## 8. 調適步驟建議

### 從穩定開始，逐步加快

1. **先確認方向正確** — 手電筒往右偏 → axis1 應該往右追
   - 不對就改 `ERROR_SIGN`
2. **先用保守參數** — `OUTPUT_GAIN = 1.0`，確認能穩定追蹤
3. **逐步加大 OUTPUT_GAIN** — 1.0 → 1.5 → 2.0 → 2.5
   - 每次加大後觀察是否開始振盪
4. **如果振盪** — 加大 `DEADBAND` 或降低 `KP_SMALL`
5. **如果穩態偏移** — 加大 `KI`
6. **如果反應很快但急停時過衝** — 降低 `RATE_LIMIT`

### 分軸調整

兩軸的機構特性通常不同（例如 X 軸水平、Y 軸有重力）：
- `CTRL_AXIS1_*` 和 `CTRL_AXIS2_*` 可以設不同值
- POS_SCALE / NEG_SCALE 用來補償正反方向差異

---

## 9. 上下游關係

```
ldr_tracking.c → LdrTrackingFrame_t
                      ↓
              tracker_controller.c (PID)
                      ↓
              MotionCommand_t
                      ↓
              motor_control.c → stepper_tmc2209.c
```

---

## 10. 踩雷提醒

1. **方向反了別先怪控制器** — 先檢查 `ERROR_SIGN`、LDR 象限映射、TMC2209 DIR 極性
2. **改 OUTPUT_GAIN 記得改 MAX_STEP_HZ** — 否則 gain 放大後被上限卡住，看起來像沒效果
3. **rate limit 是每週期** — 不是每秒。1ms 週期下 16250/週期 = 16,250,000/s，非常大
4. **積分在大誤差時不累積** — 這是故意的，防止積分飽和後反應遲鈍
