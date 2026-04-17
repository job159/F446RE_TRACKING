#ifndef APP_SERIAL_CMD_H
#define APP_SERIAL_CMD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "App/tracking_config.h"
#include "App/tracking_types.h"
#include "App/stepper_tmc2209.h"   /* for TMC_SPEED_STAGE_COUNT */

typedef struct {
  UART_HandleTypeDef *huart;
  uint8_t      rx_buf[SERIAL_CMD_RX_LINE_MAX];
  uint8_t      rx_len;
  SerialCmd_t  queue[SERIAL_CMD_QUEUE_LENGTH];
  uint8_t      q_head;
  uint8_t      q_tail;
  uint8_t      q_count;
} SerialCmd_HandleTypeDef;

void    SerialCmd_Init(SerialCmd_HandleTypeDef *h, UART_HandleTypeDef *huart);
void    SerialCmd_PollRx(SerialCmd_HandleTypeDef *h);
uint8_t SerialCmd_Dequeue(SerialCmd_HandleTypeDef *h, SerialCmd_t *out);

#ifdef __cplusplus
}
#endif

#endif /* APP_SERIAL_CMD_H */
