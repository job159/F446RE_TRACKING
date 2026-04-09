#include "App/uart_sequence.h"
#include <stdio.h>

#define TX_BUF_SIZE   192
#define TX_TIMEOUT     30

void UartSequence_Init(UartSequence_HandleTypeDef *h, UART_HandleTypeDef *huart, uint32_t period_ms)
{
  h->huart = huart;
  h->period_ms = period_ms;
  h->last_tick = HAL_GetTick();
  h->serial_no = 0;
}

void UartSequence_Task(
    UartSequence_HandleTypeDef *h,
    uint16_t adc1, uint16_t adc2, uint16_t adc3, uint16_t adc4,
    int32_t enc1, int32_t enc2,
    uint32_t ang1_x10000, uint32_t ang2_x10000,
    uint8_t m1_mode, uint8_t m2_mode)
{
  if (h->huart == NULL) return;

  uint32_t now = HAL_GetTick();
  if ((now - h->last_tick) < h->period_ms) return;
  h->last_tick = now;

  char buf[TX_BUF_SIZE];
  int len = snprintf(buf, sizeof(buf),
      "%u m1:%u m2:%u "
      "adc1:%u adc2:%u adc3:%u adc4:%u "
      "enc1:%d enc2:%d "
      "ang1:%u.%04u ang2:%u.%04u\r\n",
      h->serial_no,
      m1_mode, m2_mode,
      adc1, adc2, adc3, adc4,
      enc1, enc2,
      ang1_x10000 / 10000, ang1_x10000 % 10000,
      ang2_x10000 / 10000, ang2_x10000 % 10000);

  if (len <= 0) return;
  if (len > (int)(sizeof(buf) - 1)) len = sizeof(buf) - 1;

  if (HAL_UART_Transmit(h->huart, (uint8_t *)buf, (uint16_t)len, TX_TIMEOUT) == HAL_OK)
    h->serial_no++;
}
