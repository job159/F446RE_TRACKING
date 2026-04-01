#ifndef APP_TELEMETRY_H
#define APP_TELEMETRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "App/tracking_types.h"

typedef struct
{
  UART_HandleTypeDef *huart;
  uint32_t period_ms;
  uint32_t last_tick_ms;
  uint32_t sequence;
} Telemetry_HandleTypeDef;

void Telemetry_Init(
    Telemetry_HandleTypeDef *handle,
    UART_HandleTypeDef *huart,
    uint32_t period_ms);

void Telemetry_Task(
    Telemetry_HandleTypeDef *handle,
    const TelemetrySnapshot_t *snapshot);

void Telemetry_SendLine(
    Telemetry_HandleTypeDef *handle,
    const char *text);

#ifdef __cplusplus
}
#endif

#endif /* APP_TELEMETRY_H */
