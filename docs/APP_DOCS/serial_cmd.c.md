# serial_cmd.c

來源: `Core/Src/App/serial_cmd.c`

## 1. 角色

`serial_cmd.c` 負責把 UART 文字命令轉成 `SerialCmd_t` queue，不直接控制馬達或模式。

流程是：

1. 收字元
2. 組命令行
3. 轉大寫與 trim
4. 解析命令
5. enqueue 交給 `app_main.c`

## 2. 目前主命令（建議使用）

- `IDLE`
- `TRACK`
- `MANUAL`
- `MAN 1` 到 `MAN 8`
- `MAN F1..F4` / `MAN R1..R4`
- `PERIOD 1MS` / `PERIOD 2MS` / `PERIOD 5MS`
- `RECAL`
- `STATUS`
- `CALDATA`
- `CONFIG`
- `HELP`

## 3. 相容舊命令（仍可用）

- `MODE 0/1/2`、`0/1/2`
- `F1..F4`、`R1..R4`
- `STAGE 0..7`
- `CTL 1|2|5`（含 `CTL 1MS|2MS|5MS`）
- `STAT?`、`CAL?`、`CFG?`
- `CAL`

## 4. 重要解析 helper

### `SerialCmd_ParsePeriodMs()`

- 支援純數字與 `MS` 後綴
- 例如 `1`、`2MS`、`5MS`
- 只做數字解析，不做合法值限制（合法性在 `app_main.c` 判）

### `SerialCmd_ParseManualStageToken()`

- 支援 `1..8`、`F1..F4`、`R1..R4`
- 轉成 stage `0..7`

## 5. parser 行為特性

- polling `RXNE`，非 DMA parser
- `\r` 或 `\n` 才結束一筆
- queue 滿時丟棄新命令
- backspace (`\b`, `0x7F`) 可退字

## 6. 維護建議

- 要新增命令時，請同步改：
  `tracking_types.h` 的 `SerialCmdId_t`、`serial_cmd.c` parser、`app_main.c` command handler。
- 若要提高命令吞吐，檢查：
  `SERIAL_CMD_RX_LINE_MAX` 與 `SERIAL_CMD_QUEUE_LENGTH`。
