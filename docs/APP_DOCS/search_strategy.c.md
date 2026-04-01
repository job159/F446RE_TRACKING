# search_strategy.c

來源: `Core/Src/App/search_strategy.c`

## 1. 這個檔案的角色

當追光失敗時，系統不能只停住不動，否則很可能永遠找不回光源。

`search_strategy.c` 的角色，就是在 `MODE_SEARCH` 期間決定：

- 要往哪邊找
- 先找哪裡
- 找多久
- 什麼時候改變策略

## 2. 這個模組為什麼分兩塊

這個檔案同時包含：

- `TrackingHistory_*`
- `SearchStrategy_*`

原因是 search 不是憑空做決策，它會依賴「最後一次成功追光時的資料」。

所以這兩塊設計上是很自然地綁在一起的：

- history 負責保存過去
- strategy 負責利用過去決定現在

## 3. `TrackingHistory_*` 這一塊在做什麼

### `TrackingHistory_Init()`

清空歷史 buffer。

### `TrackingHistory_Push()`

當 tracking 成功且 frame 有效時，把以下資訊推進 ring buffer：

- time
- error_x / error_y
- axis1 / axis2 command
- enc1 / enc2 count
- total light
- valid

### `TrackingHistory_GetLatestValid()`

取出最近一筆有效資料。

這筆資料特別重要，因為 search 第二階段會拿它當最後好位置。

## 4. Search 狀態機

目前 search 分成三段：

### 4.1 `SEARCH_HISTORY_BIAS`

先沿用最近成功追光時的命令方向。

做法是：

- 取最近幾筆有效 history
- 平均 axis1 / axis2 指令
- 如果平均值太小，就至少給一個 fallback step rate

這個階段的想法是：

> 也許光源只是剛剛沿著原方向稍微跑掉，先沿原方向追一小段試試看。

### 4.2 `SEARCH_REVISIT_LAST_GOOD`

如果前面沒找回來，下一步不是亂掃，而是回最後一次追得好的 encoder 位置附近。

做法是：

- 比較目前 encoder 與 `last_good_enc1/enc2`
- 如果差距超過容忍範圍，就朝該方向回去
- 如果兩軸都已接近 last good，就進下一階段

### 4.3 `SEARCH_SWEEP_SCAN`

最後才做比較像掃描的行為。

目前是：

- X 軸主要左右來回掃
- Y 軸則以 phase 的方式間歇移動
- 每隔固定 hold time 改一次方向或 phase

這是一種簡單但實用的 raster-like 搜尋方式。

## 5. `SearchStrategy_Enter()`

這個函式在剛進 search 模式時呼叫一次。

它會：

- 重設 search handle
- 記錄 search 起始時間
- 從 history 推估 bias 速度
- 若有最近有效 history，就保存最後好位置 encoder

這表示 search 是「進模式時先初始化一次，再在 run 階段持續演進」。

## 6. `SearchStrategy_Run()`

這是 search 每個控制週期的主函式。

它會依照 `substate` 分支：

- 在 bias 階段，輸出 bias 命令直到時間到或 cycle 數夠
- 在 revisit 階段，朝 last good encoder 回去
- 在 sweep 階段，做固定規則掃描

輸出型別仍然是：

- `MotionCommand_t`

因此 search 與 tracking 在馬達層看起來是同一種命令格式，只是命令來源不同。

## 7. 目前這版 search 手感

這一版參數已調得比早期更積極：

- bias step 較大
- revisit step 較快
- sweep 幅度更明顯
- hold 時間較短

所以追丟後的反應會比前一版更果斷。

## 8. 和其他模組的關係

### 上游

- `TrackingHistory` 來自 tracking 成功期間的資料
- encoder count 來自 `app_encoder.c`
- 參數來自 `tracking_config.h`

### 下游

- `app_main.c`
- `motor_control.c`

## 9. 最常改的地方

### 想讓 search 更大範圍

調：

- `SEARCH_BIAS_STEP_HZ`
- `SEARCH_REVISIT_STEP_HZ`
- `SEARCH_SWEEP_STEP_HZ`
- `SEARCH_SWEEP_Y_STEP_HZ`

### 想讓 search 不要那麼快跳策略

調：

- `SEARCH_BIAS_HOLD_MS`
- `SEARCH_HISTORY_BIAS_CYCLES`
- `SEARCH_REVISIT_MAX_MS`
- `SEARCH_SWEEP_HOLD_MS`

### 想讓 revisit 更準

調：

- `SEARCH_REVISIT_TOL_COUNTS`

## 10. 修改時容易踩雷的地方

### 10.1 history 空時要考慮 fallback

如果 history 沒資料，bias 不應該變成完全不動。目前程式已有 fallback step rate。

### 10.2 search 太激進可能會和機構限制打架

如果 sweep / revisit 速度太快，步進系統可能更容易失步或來回大晃動。

### 10.3 search 成功回 tracking 不是這個檔案決定的

是否回 tracking，真正是 `app_main.c` 根據 `ldr.frame.is_valid` 與 `reacquire_counter` 決定。這個模組只負責 search 期間要怎麼找。
