# 06 — PID 控制邏輯

每個控制週期接收 LDR error,輸出 step 頻率(含正負方向)給馬達。
本系統採**單軸獨立 PID**,Motor1 跟 Motor2 用不同參數。

## 1. PID 公式(本實作)

[tracker_controller.c::run_axis()](../Core/Src/App/tracker_controller.c:43):

```
   ┌─ 死區判定 ────────────────────────────────┐
   │ if |error| <= DEADBAND:                  │
   │     integrator *= DECAY                  │
   │     return 0  (馬達停)                   │
   └──────────────────────────────────────────┘

   kp = (|err|<=ERR_SMALL)  ? KP_SMALL
      : (|err|<=ERR_MEDIUM) ? KP_MEDIUM
      : KP_LARGE                          ← 三段選擇

   if |err| <= ERR_MEDIUM:                ← 只在中小範圍積分
     integrator += err × KI × dt          ← 防 windup

   deriv = (err - prev_err) / dt

   out = (kp × err  +  integrator  +  KD × deriv) × OUTPUT_GAIN

   if out >= 0:  out *= POS_SCALE         ← 正/反方向不對稱補償
   else:         out *= NEG_SCALE

   out = clamp(out, ±MAX_STEP_HZ)         ← 輸出限幅

   delta = out - prev_out
   delta = clamp(delta, ±RATE_LIMIT_HZ)   ← 速率限制
   out   = prev_out + delta

   return out
```

## 2. 為何要分段 KP?

傳統 PID 一個 KP 走天下,但太陽追蹤的 error 範圍跨度很大:
- 微小偏差(error 0.02) 想要**輕觸**,免得抖
- 大偏差(error 0.8) 想要**衝過去**,不然慢吞吞

| 段別 | error 範圍 | KP 角色 | Motor1 / Motor2 預設 |
|---|---|---|---|
| **DEADBAND** | 0 ~ 0.015 | 0 (不動) | 共用 |
| **SMALL**    | 0.015 ~ 0.055 | 細修 | 90 / 180 |
| **MEDIUM**   | 0.055 ~ 0.140 | 中速追 | 180 / 360 |
| **LARGE**    | > 0.140 | 急衝 | 310 / 620 |

調整在 [tracking_config.h](../Core/Inc/App/tracking_config.h):
```c
#define PID_ERR_DEADBAND   0.015f
#define PID_ERR_SMALL      0.055f
#define PID_ERR_MEDIUM     0.140f
```

## 3. Motor1 vs Motor2 為何分開?

實機上:
- **Motor1 (水平軸)** 通常拖比較大的負載(整個追蹤頭),**慣量大**
- **Motor2 (仰角軸)** 只搬鏡頭/面板,**慣量小**

慣量大 → 高 KP 會 overshoot、振盪
慣量小 → 低 KP 會慢吞吞

所以本系統把 Motor1 的所有 PID 參數設為 **Motor2 的 1/2**:

```c
/* Motor1 (緩和) */            /* Motor2 (原版) */
M1_KP_SMALL    90.0f          M2_KP_SMALL    180.0f
M1_KP_MEDIUM   180.0f         M2_KP_MEDIUM   360.0f
M1_KP_LARGE    310.0f         M2_KP_LARGE    620.0f
M1_KI          4.0f           M2_KI          8.0f
M1_KD          9.0f           M2_KD          18.0f
M1_OUTPUT_GAIN 1.0f           M2_OUTPUT_GAIN 2.0f
M1_RATE_LIMIT  8000U          M2_RATE_LIMIT  13750U
```

## 4. 各參數深度說明

### KP (比例增益)
最重要,直接決定「響應強度」。

| 太大 | 太小 |
|---|---|
| Overshoot,振盪 | 反應慢 |
| 共振、機構抖動 | 永遠到不了 |
| 高頻噪音放大 | 死區附近停留 |

**經驗值**:從小往大調,直到出現輕微 overshoot,然後降 30%。

### KI (積分增益)
消除穩態誤差(例如機構摩擦造成 error 始終 0.02 走不掉)。

⚠️ 本實作**只在 |err| <= ERR_MEDIUM 時積分**:
```c
if (abs_e <= PID_ERR_MEDIUM)
    ax->integrator += error * p->ki * dt;
```

這是 **conditional integration**,避免大誤差時積分爆衝(integrator windup)。
另外死區內會衰減 (`integrator *= 0.8`),避免 LDR 雜訊讓積分慢慢飄。

### KD (微分增益)
抑制振盪,當 error 變化快時反向阻尼。

⚠️ 對 LDR 這種離散且雜訊多的訊號,**KD 過大會放大噪音**。
建議從 KP/20 開始試。

### OUTPUT_GAIN
把 PID 輸出 (kp×err+...) 整體乘 / 除一個倍率,
不改 PID 動態只改最終強度。
快速「整體加速 / 減速」用這個比較直覺。

### POS_SCALE / NEG_SCALE
機構不對稱補償(例如皮帶張力一邊緊一邊鬆,反向比正向重)。
正轉乘 POS_SCALE,反轉乘 NEG_SCALE。
平衡的機構兩個都設 1.0。

### MAX_STEP_HZ
最終輸出絕對上限。
即使 PID 算出 100k Hz 也會被 clamp。
與 TMC2209 + ramp 能接受的最高頻率有關,一般 60kHz 安全。

### RATE_LIMIT_HZ
**單個控制週期內** step 頻率最多變多少。
例如 RATE_LIMIT=8000, period=1ms → 每秒最多變 8000 × 1000 = 8M Hz/s。
但每次 PID 算 +20000Hz 的命令會被 throttle 成 +8000Hz。

主要保護:
- 機構慣量,避免馬達失步
- 太突兀的速度變化會傷 driver

## 5. 死區與積分器衰減

[tracker_controller.c:55-61](../Core/Src/App/tracker_controller.c:55):
```c
if (abs_e <= PID_ERR_DEADBAND)
{
  ax->integrator    *= PID_INTEGRATOR_DECAY;   // 0.8
  ax->prev_error     = error;
  ax->prev_output_hz = 0;
  return 0;
}
```

進入死區:
1. 積分器逐輪乘 0.8(5ms 週期下,每秒 200 次 ×0.8 → 約 0.03 秒衰減一半)
2. 輸出立即歸零(prev 也歸零,下次出來的 rate limit 從 0 算)

這樣**追到目標就乾淨停下**,不會在 0 附近抖。

## 6. 控制週期影響

`SYS_CONTROL_PERIOD_MS` (預設 5ms) 會影響:
- 積分項 `error × KI × dt` (週期變大,單次積分量變大)
- 微分項 `(err - prev_err) / dt` (週期變大,微分變小)
- 速率限制 (週期變大,實際每秒變化量變小)

### 串口可動態切換
```
PERIOD 1    → 1ms
PERIOD 2    → 2ms
PERIOD 5    → 5ms
```

預設 5ms 已是省 CPU 與反應速度的平衡點。真的要更快才切 1ms(但 stepper ramp 在 1ms 週期下可能吃光 CPU)。

## 7. 調 PID 的標準流程

1. **先把 KI、KD 設 0**,只用 KP
2. 從 `M*_KP_SMALL = 50` 開始,慢慢加直到追蹤可見 → 抓到「合理 KP_SMALL」
3. KP_MEDIUM ≈ KP_SMALL × 2,KP_LARGE ≈ KP_SMALL × 3.5
4. 如果穩態有殘差(總是差一點點不到位)→ 加一點點 KI(從 KP_SMALL/30 開始)
5. 如果有 overshoot → 加一點點 KD(從 KP_SMALL/15 開始)
6. 反覆微調 30~60 分鐘可調出滿意結果

### 如何即時測試?
1. 連接 serial 115200
2. 輸入 `RECAL` 重新校正
3. 拿手電筒對著 LDR 移動,觀察:
   - `STATUS` 看 cmd:?,? 數值
   - 馬達是否平順追上
4. 不滿意 → 改 [tracking_config.h](../Core/Inc/App/tracking_config.h) → rebuild → flash → 重複

## 8. 進階:Anti-windup 機制

本實作 anti-windup 三層:
1. **Conditional integration**: 大誤差不積分
2. **Integrator decay**: 死區內衰減
3. **Output clamping**: PID 輸出在 KP × error 之後也 clamp,間接限制 integrator 影響

這對 LDR 這類「光源消失就完全失去回授」的場景很重要,
否則重新出現光源時會有積分餘量讓馬達衝過頭。

## 9. 進階:加入 feed-forward(目前未實作)

若知道太陽軌跡(緯度 + 時刻),可以額外加入前饋指令,
減輕 PID 負擔。但本系統設計為「純被動跟隨」,不需 RTC 與位置資訊。

若要加,在 [tracker_controller.c::TrackerController_Run()](../Core/Src/App/tracker_controller.c) 的 `cmd.axis1_step_hz` 加上 feedforward 項即可。
