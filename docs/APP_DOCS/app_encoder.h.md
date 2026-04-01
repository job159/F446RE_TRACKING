# app_encoder.h

來源: `Core/Inc/App/app_encoder.h`

## 1. 這個檔案的角色

宣告 encoder 模組對外提供的常數、handle 與 API。

它是 `app_encoder.c` 的介面定義檔。

## 2. 主要常數

### `APP_ENCODER_PULSE_PER_REV`

目前註解寫的是 1000 pulse/rev。

### `APP_ENCODER_QUADRATURE_MULTIPLIER`

目前使用 x4 encoder mode，所以是 `4`。

### `APP_ENCODER_COUNTS_PER_REV`

由上面兩者相乘得到，現在是：

```text
1000 * 4 = 4000 count/rev
```

### `APP_ENCODER_ANGLE_SCALE`

目前設定 `10000`，代表角度輸出是固定小數格式到小數第 4 位。

## 3. `AppEncoder_HandleTypeDef`

這個 handle 保存：

- 兩個 encoder timer handle
- 兩個上次讀到的硬體 counter
- 兩個長期累積 count

這樣 `Task()` 才能知道如何從單次硬體讀值推導出長期位置。

## 4. 對外 API

### 初始化與更新

- `AppEncoder_Init()`
- `AppEncoder_Task()`

### 讀取位置

- `AppEncoder_GetCount1()`
- `AppEncoder_GetCount2()`

### 讀取角度

- `AppEncoder_GetAngle1X10000()`
- `AppEncoder_GetAngle2X10000()`

## 5. 修改時機

通常以下情況才需要改這個 header：

- encoder 規格改了
- 要新增 velocity 或 zero-offset 類型功能
- 要改角度輸出的固定小數格式

如果只是修內部計算流程，通常改 `.c` 即可。
