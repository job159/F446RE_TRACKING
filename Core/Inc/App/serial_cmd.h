#ifndef APP_SERIAL_CMD_H
#define APP_SERIAL_CMD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "App/tracking_config.h"
#include "App/tracking_types.h"

typedef struct
{
  UART_HandleTypeDef *huart;
  uint8_t rx_line[SERIAL_CMD_RX_LINE_MAX];
  uint8_t rx_length;
  SerialCmd_t queue[SERIAL_CMD_QUEUE_LENGTH];
  uint8_t queue_head;
  uint8_t queue_tail;
  uint8_t queue_count;
} SerialCmd_HandleTypeDef;

void SerialCmd_Init(
    SerialCmd_HandleTypeDef *handle,
    UART_HandleTypeDef *huart);

void SerialCmd_PollRx(SerialCmd_HandleTypeDef *handle);

uint8_t SerialCmd_Dequeue(
    SerialCmd_HandleTypeDef *handle,
    SerialCmd_t *command);

#ifdef __cplusplus
}
#endif

#endif /* APP_SERIAL_CMD_H */
