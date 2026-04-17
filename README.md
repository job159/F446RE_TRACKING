# F446RE_TRACKING

STM32F446RE Nucleo + 2×TMC2209 步進 + 4×LDR 的雙軸太陽能追蹤器。

## 硬體

- **MCU**: STM32F446RE Nucleo-64
- **Driver**: 2 顆 TMC2209(UART4 / UART5 單線)
- **感測**: 4 顆 LDR,分壓後進 ADC1 / ADC2
- **UART log**: USART2(ST-Link VCP,COM port 115200 8N1)
- **按鈕**: B1 (PC13),cycle manual 速度段

詳細腳位對照、週邊設定、演算法、參數調整見 **[new_docs/](new_docs/)**。

## 直接燒錄流程(clone + flash)

**必要工具:**
- [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html)(開源免費,內建 arm-gcc + OpenOCD)

**步驟:**
1. `git clone <repo-url>`
2. 打開 STM32CubeIDE → File → Import → Existing Projects into Workspace → 選剛 clone 的資料夾 → Finish
3. 專案名稱 `F446RE_TRACKING` 出現在 Project Explorer
4. 插上 Nucleo-F446RE(USB 接電腦)
5. 點選專案 → Project → Build All(或 `Ctrl+B`)
6. 點 Run → Debug(或直接 `F11`)→ 自動燒錄 + 進入除錯
7. 打開 serial terminal(PuTTY / TeraTerm / Arduino Serial Monitor),選 ST-Link VCP 那個 COM port,115200 8N1

**注意:** 本專案已把 STM32F4 HAL driver 跟 CMSIS headers 複製進 `Drivers/` 資料夾,**不依賴外部 STM32Cube 套件位置**,clone 完即可 build。

## 操作

開機 5 秒校正後自動進 TRACKING 模式。

| 指令 | 說明 |
|---|---|
| `IDLE` | 停止所有馬達 |
| `TRACK` | 追蹤模式(需校正完) |
| `MANUAL` | 手動模式 |
| `F1`..`F7` / `R1`..`R7` | 手動速度段(F=正轉,R=反轉,1 慢 7 快) |
| `RECAL` | 重新校正(5 秒) |
| `STATUS` / `CAL?` / `CFG?` | 查詢狀態 |
| `HELP` | 列出所有指令 |

按板載 B1 按鈕 = cycle 手動速度段(校正中按會打斷校正)。

## 專案結構

```
F446RE_TRACKING/
├─ Core/                         應用與 CubeMX 產出
│  ├─ Inc/                       (main.h、App 的 h)
│  └─ Src/
│     ├─ main.c, stm32f4xx_hal_msp.c, stm32f4xx_it.c, system_stm32f4xx.c
│     └─ App/                    ★ 主要應用程式碼
│        ├─ app_main.c           主狀態機
│        ├─ app_adc.c            ADC DMA + 低通
│        ├─ ldr_tracking.c       LDR 校正與誤差計算
│        ├─ tracker_controller.c 追蹤控制器(純比例)
│        ├─ motor_control.c      馬達高階包裝
│        ├─ stepper_tmc2209.c    TMC2209 UART + STEP/DIR/EN
│        ├─ manual_control.c     手動 stage 派發
│        ├─ serial_cmd.c         串口指令 parser
│        └─ telemetry.c          100 ms 遙測輸出
├─ Core/Inc/App/tracking_config.h  ★★ 9 成參數都在這一個檔
├─ Core/Startup/                 啟動組語
├─ Drivers/                      ★ 已內含,不用自己抓 STM32Cube 套件
│  ├─ STM32F4xx_HAL_Driver/      HAL driver 原始碼
│  └─ CMSIS/                     CMSIS 標準介面
├─ new_docs/                     詳細設計文件(10 份,含腳位、演算法、調參)
├─ F446RE_TRACKING.ioc           CubeMX 設定(可在 CubeMX 打開改)
└─ STM32F446RETX_FLASH.ld        linker script
```

## 調參

最常改的在 **[Core/Inc/App/tracking_config.h](Core/Inc/App/tracking_config.h)**,一個檔搞定 9 成調整:
- 校正時間、控制週期
- ADC 反向、低通係數
- LDR 追蹤門檻
- PID KP(純比例、無記憶)、MAX_STEP_HZ、方向翻轉

詳細說明見 [new_docs/09_常用調整指南.md](new_docs/09_常用調整指南.md)。

## 授權

HAL driver / CMSIS 遵循 ST 原始 LICENSE(BSD-3),見 `Drivers/` 內各自 LICENSE 檔。
本專案應用程式碼部分(Core/Src/App、Core/Inc/App)可自由使用。
