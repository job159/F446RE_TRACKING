#ifndef APP_UART_SEQUENCE_H
#define APP_UART_SEQUENCE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

typedef struct
{
  UART_HandleTypeDef *huart;
  uint32_t period_ms;
  uint32_t last_tick_ms;
  uint32_t serial_number;
} UartSequence_HandleTypeDef;

void UartSequence_Init(
    UartSequence_HandleTypeDef *handle,
    UART_HandleTypeDef *huart,
    uint32_t period_ms);

void UartSequence_Task(
    UartSequence_HandleTypeDef *handle,
    uint16_t adc1_value,
    uint16_t adc2_value,
    int32_t enc1_count,
    int32_t enc2_count,
    uint32_t enc1_angle_x10000,
    uint32_t enc2_angle_x10000,
    uint8_t motor_1_mode,
    uint8_t motor_2_mode);

#ifdef __cplusplus
}
#endif

#endif /* APP_UART_SEQUENCE_H */
