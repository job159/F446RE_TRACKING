# search_strategy.h

來源: `Core/Inc/App/search_strategy.h`

## 1. 這個檔案的角色

宣告 search strategy 與 tracking history 的資料結構和 API。

## 2. `TrackingHistory_HandleTypeDef`

這個 handle 是固定長度的歷史 ring buffer，內容包含：

- `entries[SEARCH_HISTORY_LEN]`
- `head`
- `count`

其中每筆 entry 的格式定義在 `tracking_types.h`。

## 3. `SearchStrategy_HandleTypeDef`

這個 handle 保存 search 狀態機目前的所有狀態：

- `substate`
- `state_tick_ms`
- `history_bias_cycles`
- `bias_axis1_hz`
- `bias_axis2_hz`
- `last_good_enc1`
- `last_good_enc2`
- `sweep_dir_x`
- `sweep_dir_y`
- `sweep_phase`

所以 search 不是每圈無狀態重算，而是有完整狀態記憶。

## 4. 對外 API

### history 相關

- `TrackingHistory_Init()`
- `TrackingHistory_Push()`
- `TrackingHistory_GetLatestValid()`

### strategy 相關

- `SearchStrategy_Init()`
- `SearchStrategy_Reset()`
- `SearchStrategy_Enter()`
- `SearchStrategy_Run()`

## 5. 使用方式

標準順序是：

1. 系統初始化時 `TrackingHistory_Init()` 與 `SearchStrategy_Init()`
2. tracking 成功時持續 `TrackingHistory_Push()`
3. 一旦進 search，先 `SearchStrategy_Enter()`
4. search 期間持續 `SearchStrategy_Run()`

## 6. 修改時機

通常在以下情況會改這個 header：

- 想讓 history 保存更多欄位
- 想增加新的 search 子狀態
- search handle 需要更多內部狀態
