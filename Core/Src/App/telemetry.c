#include "App/telemetry.h"

#include <stdio.h>
#include <string.h>

#define TELEMETRY_TX_BUFFER_SIZE  256U
#define TELEMETRY_TX_TIMEOUT_MS   30U

static const char *Telemetry_ModeToText(SystemMode_t mode)
{
  switch (mode)
  {
    case MODE_IDLE:
      return "IDLE";
    case MODE_TRACKING:
      return "TRACK";
    case MODE_SEARCH:
      return "SEARCH";
    case MODE_MANUAL:
      return "MANUAL";
    default:
      return "UNKNOWN";
  }
}

static const char *Telemetry_SubstateToText(const TelemetrySnapshot_t *snapshot)
{
  if (snapshot == NULL)
  {
    return "NA";
  }

  if (snapshot->mode == MODE_IDLE)
  {
    return (snapshot->idle_substate == IDLE_CALIBRATING) ? "CAL" : "WAIT";
  }

  if (snapshot->mode == MODE_SEARCH)
  {
    switch (snapshot->search_substate)
    {
      case SEARCH_HISTORY_BIAS:
        return "HBIAS";
      case SEARCH_REVISIT_LAST_GOOD:
        return "REVISIT";
      case SEARCH_SWEEP_SCAN:
      default:
        return "SWEEP";
    }
  }

  return "-";
}

void Telemetry_Init(
    Telemetry_HandleTypeDef *handle,
    UART_HandleTypeDef *huart,
    uint32_t period_ms)
{
  if (handle == NULL)
  {
    return;
  }

  handle->huart = huart;
  handle->period_ms = period_ms;
  handle->last_tick_ms = HAL_GetTick();
  handle->sequence = 0U;
}

void Telemetry_SendLine(
    Telemetry_HandleTypeDef *handle,
    const char *text)
{
  if ((handle == NULL) || (handle->huart == NULL) || (text == NULL))
  {
    return;
  }

  (void)HAL_UART_Transmit(handle->huart,
                          (uint8_t *)text,
                          (uint16_t)strlen(text),
                          TELEMETRY_TX_TIMEOUT_MS);
}

void Telemetry_Task(
    Telemetry_HandleTypeDef *handle,
    const TelemetrySnapshot_t *snapshot)
{
  char buffer[TELEMETRY_TX_BUFFER_SIZE];
  int length;
  uint32_t now_ms;

  if ((handle == NULL) || (snapshot == NULL) || (handle->huart == NULL))
  {
    return;
  }

  now_ms = HAL_GetTick();
  if ((now_ms - handle->last_tick_ms) < handle->period_ms)
  {
    return;
  }

  handle->last_tick_ms = now_ms;

  length = snprintf(buffer,
                    sizeof(buffer),
                    "%lu mode:%s sub:%s cal:%u valid:%u adc:%u,%u,%u,%u base:%u,%u,%u,%u d:%u,%u,%u,%u err:%ld,%ld cmd:%ld,%ld enc:%ld,%ld stg:%u\r\n",
                    (unsigned long)handle->sequence,
                    Telemetry_ModeToText(snapshot->mode),
                    Telemetry_SubstateToText(snapshot),
                    (unsigned int)snapshot->calibration_done,
                    (unsigned int)snapshot->source_valid,
                    (unsigned int)snapshot->adc[0],
                    (unsigned int)snapshot->adc[1],
                    (unsigned int)snapshot->adc[2],
                    (unsigned int)snapshot->adc[3],
                    (unsigned int)snapshot->baseline[0],
                    (unsigned int)snapshot->baseline[1],
                    (unsigned int)snapshot->baseline[2],
                    (unsigned int)snapshot->baseline[3],
                    (unsigned int)snapshot->delta[0],
                    (unsigned int)snapshot->delta[1],
                    (unsigned int)snapshot->delta[2],
                    (unsigned int)snapshot->delta[3],
                    (long)snapshot->error_x_x1000,
                    (long)snapshot->error_y_x1000,
                    (long)snapshot->cmd_axis1_hz,
                    (long)snapshot->cmd_axis2_hz,
                    (long)snapshot->enc1_count,
                    (long)snapshot->enc2_count,
                    (unsigned int)(snapshot->manual_stage_valid != 0U ? snapshot->manual_stage : 255U));

  if (length <= 0)
  {
    return;
  }

  if (length > (int)(sizeof(buffer) - 1U))
  {
    length = (int)(sizeof(buffer) - 1U);
  }

  if (HAL_UART_Transmit(handle->huart,
                        (uint8_t *)buffer,
                        (uint16_t)length,
                        TELEMETRY_TX_TIMEOUT_MS) == HAL_OK)
  {
    handle->sequence++;
  }
}
