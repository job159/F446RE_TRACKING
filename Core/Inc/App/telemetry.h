#ifndef APP_TELEMETRY_H
#define APP_TELEMETRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "App/tracking_types.h"

typedef struct {
  UART_HandleTypeDef *huart;
  uint32_t period_ms;
  uint32_t last_tick;
  uint32_t seq;
} Telemetry_HandleTypeDef;

void Telemetry_Init(Telemetry_HandleTypeDef *h, UART_HandleTypeDef *huart, uint32_t period_ms);
void Telemetry_Task(Telemetry_HandleTypeDef *h, const TelemetrySnapshot_t *snap);
void Telemetry_SendLine(Telemetry_HandleTypeDef *h, const char *text);

#ifdef __cplusplus
}
#endif

#endif /* APP_TELEMETRY_H */
