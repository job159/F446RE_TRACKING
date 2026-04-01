# app_main.h

來源: `Core/Inc/App/app_main.h`

## 1. 這個檔案的角色

這個 header 很短，但它是整個 App 層最重要的「對外門面」。

`main.c` 不需要知道：

- 追光怎麼算
- search 怎麼跑
- manual stage 怎麼切
- telemetry 怎麼發

它只要知道：

- 開機時呼叫 `AppMain_Init()`
- 主迴圈裡呼叫 `AppMain_Task()`

所以這個檔案扮演的是 App 層 API 邊界。

## 2. 對外公開的函式

### `AppMain_Init(...)`

用途：

- 把 `main.c` 建好的 HAL handle 傳進 App 層
- 讓 App 層把 ADC、encoder、motor、telemetry 等模組串起來

傳入內容包含：

- `hadc1`, `hadc2`
- log UART
- 兩組 step timer
- 兩組 encoder timer
- 兩組 TMC2209 UART

這種設計代表 App 層不直接建立 HAL peripheral，而是依賴外部注入 handle。

### `AppMain_Task()`

用途：

- 每次主迴圈執行一次 App 層排程

這是 App 層在 runtime 的主入口。

## 3. 為什麼這個 header 重要

雖然它只有兩個函式，但它其實定義了整個專案的分層邊界：

- `main.c` 留在 HAL / CubeMX 世界
- `AppMain_*` 之後進入 App 層世界

這讓主程式結構保持乾淨，不會在 `main.c` 出現越來越多應用邏輯。

## 4. 修改時機

通常只有以下情況才會改這個檔案：

- `AppMain_Init()` 需要額外的 peripheral handle
- 你想讓 App 層提供新的外部入口
- 你想把 App 層拆成更多公開 API

如果只是改追光邏輯、manual 行為、telemetry 輸出，通常不需要動這個 header。
