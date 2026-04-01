# app_encoder.c

來源: `Core/Src/App/app_encoder.c`

## 1. 這個檔案的角色

`app_encoder.c` 負責把兩組 encoder timer 的硬體計數，轉成 App 層可以直接使用的「累積位置」與「角度」。

它的定位和 `app_adc.c` 很像：

- 不負責決策
- 不負責控制
- 專心把硬體資料整理成上層好用的格式

## 2. 這個模組解決了什麼問題

如果上層直接去讀 TIM counter，會遇到這些問題：

- counter 會持續 overflow / wrap around
- 直接看 counter 不容易得到長期位置
- 角度換算會散在多處

這個模組把這些問題收斂成：

- 長期 count
- 固定格式角度

## 3. 初始化：`AppEncoder_Init()`

初始化時會：

1. 保存 `htim_enc1` / `htim_enc2`
2. 清掉累積 count
3. 啟動 `HAL_TIM_Encoder_Start(..., TIM_CHANNEL_ALL)`
4. 讀當下 counter 當作起始點

### 為什麼要記住 `last_counter`

因為後續每次 `Task()` 都是靠：

```text
本次 counter - 上次 counter = delta
```

再把這個 delta 加到長期 count 裡。

## 4. 執行：`AppEncoder_Task()`

每次被呼叫時，會對兩個 encoder 分別做：

- 讀目前硬體 counter
- 算出和上次值的差
- 加到 `count_enc1` / `count_enc2`
- 更新 `last_counter_enc1` / `last_counter_enc2`

這樣即使 timer 本身只是一個循環計數器，App 層拿到的還是可累積的位置值。

## 5. 角度換算

除了 count，這個模組也提供角度換算：

- `AppEncoder_GetAngle1X10000()`
- `AppEncoder_GetAngle2X10000()`

### 為什麼用 `x10000`

這是固定小數格式，例如：

- `123456` 代表 `12.3456 deg`

這樣做的好處是：

- 不用在各層都傳 float
- telemetry 比較容易格式化
- 對嵌入式環境較友善

## 6. 主要 helper 的意義

### `AppEncoder_NormalizeCountToOneTurn()`

把長期 count 折回一圈範圍內，避免角度換算時落在不必要的大數值。

### `AppEncoder_ConvertCountToAngleX10000()`

把 count 轉成 `0 ~ 360.0000 deg` 對應的固定小數格式。

### `AppEncoder_CalcDelta()`

計算目前 counter 和上次 counter 的差值。

## 7. 上下游關係

### 上游

- `main.c` 初始化的 TIM2 / TIM5 encoder mode

### 下游

- `app_main.c` snapshot
- `search_strategy.c`
- `telemetry.c`

## 8. 最常調的地方

### encoder 規格不同

如果馬達編碼器不是目前這顆 1000 pulse/rev，就要改：

- `APP_ENCODER_PULSE_PER_REV`
- 進而影響 `APP_ENCODER_COUNTS_PER_REV`

### 角度格式要改

如果之後想輸出更粗或更細的角度，可以改：

- `APP_ENCODER_ANGLE_SCALE`

## 9. 修改時容易踩雷的地方

### 9.1 count 與 angle 是兩種不同用途

- count 比較適合 search 與位置比較
- angle 比較適合輸出與人類觀察

不要把 angle 當成 search 控制的唯一依據。

### 9.2 這個模組預設只讀，不做方向修正

如果實際 encoder 方向與機構期望相反，目前最適合改的是外層對 count 的使用方式，或在這個模組增加符號反轉，而不是在很多地方零碎相乘 `-1`。
