# F446RE 太陽能追蹤系統 — 文件總覽

本資料夾包含雙軸 LDR 太陽能追蹤器的完整設計與調整說明。
目標硬體:**STM32F446RE Nucleo-64**,配 **2 × TMC2209 步進驅動**,**4 × LDR** 感光。

## 系統方塊圖

```
     +------+   +------+    +------+    +------+
     | LDR0 |   | LDR1 |    | LDR2 |    | LDR3 |
     +--+---+   +--+---+    +--+---+    +--+---+
        |          |           |           |
        +---+------+           +----+------+
            |                       |
        ADC1 (PC0/PC3)          ADC2 (PC1/PC4)
          |   DMA                   |   DMA
          +----------+   +----------+
                     |   |
                 +---v---v---+
                 |  AppAdc   |  <- 低通濾波 + 分壓反向
                 +-----+-----+
                       |
                 +-----v------+
                 | LdrTracking|  <- baseline 校正 + error_x / error_y
                 +-----+------+
                       |
           +-----------+-----------+
           |                       |
      +----v-----+           +-----v-----+
      | Tracker  |           | Manual    |
      |  (PID)   |           | (14 段速) |
      +----+-----+           +-----+-----+
           |                       |
           +-----------+-----------+
                       |
                 +-----v-------+
                 | MotorControl|
                 +--+-------+--+
                    |       |
           +--------+       +---------+
           |                          |
     +-----v-------+           +------v------+
     | TMC2209 #1  |  UART4    | TMC2209 #2  |  UART5
     |  Step: TIM1 |           |  Step: TIM3 |
     +-------------+           +-------------+
```

## 檔案導覽

| 檔名 | 內容 |
|---|---|
| [01_腳位對照.md](01_腳位對照.md) | 所有 GPIO / 週邊 pin 腳表、接線說明 |
| [02_週邊初始化.md](02_週邊初始化.md) | ADC / TIM / UART / DMA / GPIO 的 CubeMX 設定與原因 |
| [03_TMC2209_暫存器.md](03_TMC2209_暫存器.md) | GCONF / IHOLD_IRUN / CHOPCONF / PWMCONF 每個位元的意義 |
| [04_ADC_濾波與校正.md](04_ADC_濾波與校正.md) | 低通濾波、分壓反向、baseline / noise floor 校正流程 |
| [05_LDR_追蹤演算法.md](05_LDR_追蹤演算法.md) | 4 路 LDR 排列、delta/total/contrast/error 的數學、追蹤有效性判定 |
| [06_PID_控制邏輯.md](06_PID_控制邏輯.md) | 分段 KP、積分策略、速率限制、Motor1 vs Motor2 差異 |
| [07_馬達細分與速度.md](07_馬達細分與速度.md) | MRES 細分對照、14 段速度表、加減速 ramp 流程 |
| [08_串口指令.md](08_串口指令.md) | 所有串口指令、遙測欄位、常用操作範例 |
| [09_常用調整指南.md](09_常用調整指南.md) | 任務導向:要達成某效果該改哪個參數 / 哪一行 |

## 原始碼結構

```
Core/
├─ Src/App/          應用層 (user code)
│   ├─ app_main.c        主狀態機: Idle/Tracking/Manual
│   ├─ app_adc.c         ADC DMA 讀取 + 低通 + 反向
│   ├─ ldr_tracking.c    LDR 校正與 error 計算
│   ├─ tracker_controller.c   雙軸 PID
│   ├─ motor_control.c   2 顆 TMC2209 高階包裝
│   ├─ stepper_tmc2209.c TMC2209 UART 暫存器 + STEP/DIR/EN
│   ├─ manual_control.c  手動 stage 派發
│   ├─ serial_cmd.c      串口指令 parser
│   └─ telemetry.c       100ms 遙測輸出
└─ Inc/App/          對應 headers
    └─ tracking_config.h    ★ 所有可調參數集中於此 ★
```

## 最重要的一個檔

**[tracking_config.h](../Core/Inc/App/tracking_config.h)** — 90% 的調整都在這一個檔裡,分 6 區塊:

1. 系統時間 (校正時間、控制週期、遙測週期)
2. ADC (反向開關、低通係數)
3. LDR 判定 (總光量門檻、對比度門檻)
4. PID 共用 (死區、誤差分段門檻)
5. Motor1 PID (緩和版)
6. Motor2 PID (原版)

> 調參時先找這個檔;只有進階調整才需要改 .c 檔。

## 已停用的模組

以下 source 檔目前是空殼(保留檔名避免舊 build 系統抱怨):

- `app_encoder.c/h` — 光編碼器回授,原本掛 TIM2/TIM5,CubeMX 已拆,程式也不再呼叫
- `search_strategy.c/h` — 失去光源時的掃描搜尋
- `uart_sequence.c/h` — 舊版遙測輸出

都可以安全從 build 清單移除。CubeMX 端 TIM2/TIM5、USART1、UART4/5 DMA 已全部拆乾淨。
