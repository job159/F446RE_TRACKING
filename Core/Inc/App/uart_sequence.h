#ifndef APP_UART_SEQUENCE_H
#define APP_UART_SEQUENCE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

typedef struct {
  UART_HandleTypeDef *huart;
  uint32_t period_ms;
  uint32_t last_tick;
  uint32_t serial_no;
} UartSequence_HandleTypeDef;

void UartSequence_Init(UartSequence_HandleTypeDef *h, UART_HandleTypeDef *huart, uint32_t period_ms);

void UartSequence_Task(
    UartSequence_HandleTypeDef *h,
    uint16_t adc1, uint16_t adc2, uint16_t adc3, uint16_t adc4,
    int32_t enc1, int32_t enc2,
    uint32_t ang1_x10000, uint32_t ang2_x10000,
    uint8_t m1_mode, uint8_t m2_mode);

#ifdef __cplusplus
}
#endif

#endif /* APP_UART_SEQUENCE_H */
