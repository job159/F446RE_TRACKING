# F446RE_TRACKING

STM32F446RE dual-stepper test project with TMC2209 control, 2 encoder inputs, and 4 analog inputs reported over UART.

## Overview

- Drives 2 stepper motors with `TIM1_CH1` and `TIM3_CH1`.
- Reads 2 encoders with `TIM2` and `TIM5`.
- Samples 4 analog inputs by using `ADC1` and `ADC2` in 2-channel scan mode with circular DMA.
- Sends a status line every 100 ms through `USART2` for terminal or host-side monitoring.

## ADC Layout

The project exposes 4 logical ADC values in the application layer:

| Logical Name | Peripheral Input | MCU Pin |
| --- | --- | --- |
| `adc1` | `ADC1_IN13` | `PC3` |
| `adc2` | `ADC2_IN14` | `PC4` |
| `adc3` | `ADC1_IN12` | `PC2` |
| `adc4` | `ADC2_IN11` | `PC1` |

Implementation details:

- `ADC1` scans `IN13 -> IN12`.
- `ADC2` scans `IN14 -> IN11`.
- Both ADCs run in continuous scan mode with circular DMA.
- The main loop only copies the DMA values and applies the existing low-pass filter.

## UART Status Output

`USART2` prints one text line every 100 ms with the current system state. The line includes:

- Sequence number
- `m1`, `m2` motor stage
- `adc1`, `adc2`, `adc3`, `adc4`
- `enc1`, `enc2`
- `ang1`, `ang2`

Example format:

```text
123 m1:2 m2:2 adc1:2048 adc2:1987 adc3:2101 adc4:2075 enc1:100 enc2:95 ang1:12.3456 ang2:11.9988
```

## Main Files

- [Core/Src/main.c](c:/Users/a2105/STM32CubeIDE/workspace_1.14.1/F446RE_TRACKING/Core/Src/main.c): peripheral init, ADC scan setup, DMA enable
- [Core/Src/stm32f4xx_hal_msp.c](c:/Users/a2105/STM32CubeIDE/workspace_1.14.1/F446RE_TRACKING/Core/Src/stm32f4xx_hal_msp.c): ADC GPIO and DMA wiring
- [Core/Src/App/app_adc.c](c:/Users/a2105/STM32CubeIDE/workspace_1.14.1/F446RE_TRACKING/Core/Src/App/app_adc.c): 4-channel ADC application logic and filtering
- [Core/Src/App/uart_sequence.c](c:/Users/a2105/STM32CubeIDE/workspace_1.14.1/F446RE_TRACKING/Core/Src/App/uart_sequence.c): UART line formatting
- [F446RE_TRACKING.ioc](c:/Users/a2105/STM32CubeIDE/workspace_1.14.1/F446RE_TRACKING/F446RE_TRACKING.ioc): CubeMX pin/peripheral configuration

## More Documentation

For a fuller hardware and behavior note, see [docs/F446RE_TRACKING_HackMD.md](c:/Users/a2105/STM32CubeIDE/workspace_1.14.1/F446RE_TRACKING/docs/F446RE_TRACKING_HackMD.md).
