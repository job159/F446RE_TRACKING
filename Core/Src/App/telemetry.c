#include "App/telemetry.h"
#include <stdio.h>
#include <string.h>

#define TX_BUF_SIZE   256
#define TX_TIMEOUT_MS  30

static const char *mode_str(SystemMode_t m)
{
  switch (m)
  {
  case MODE_IDLE:     return "IDLE";
  case MODE_TRACKING: return "TRACK";
  case MODE_SEARCH:   return "SEARCH";
  case MODE_MANUAL:   return "MANUAL";
  default:            return "?";
  }
}

static const char *sub_str(const TelemetrySnapshot_t *s)
{
  if (s->mode == MODE_IDLE)
  {
    if (s->idle_substate == IDLE_CALIBRATING) return "CAL";
    return "WAIT";
  }

  if (s->mode == MODE_SEARCH)
  {
    switch (s->search_substate)
    {
    case SEARCH_HISTORY_BIAS:       return "HBIAS";
    case SEARCH_REVISIT_LAST_GOOD:  return "REVISIT";
    default:                        return "SWEEP";
    }
  }
  return "-";
}

void Telemetry_Init(Telemetry_HandleTypeDef *h, UART_HandleTypeDef *huart, uint32_t period_ms)
{
  h->huart = huart;
  h->period_ms = period_ms;
  h->last_tick = HAL_GetTick();
  h->seq = 0;
}

void Telemetry_SendLine(Telemetry_HandleTypeDef *h, const char *text)
{
  if (h->huart == NULL || text == NULL) return;
  HAL_UART_Transmit(h->huart, (uint8_t *)text, (uint16_t)strlen(text), TX_TIMEOUT_MS);
}

void Telemetry_Task(Telemetry_HandleTypeDef *h, const TelemetrySnapshot_t *snap)
{
  if (h->huart == NULL || snap == NULL) return;

  uint32_t now = HAL_GetTick();
  if ((now - h->last_tick) < h->period_ms) return;
  h->last_tick = now;

  unsigned stg_display = 255;
  if (snap->manual_stage_valid) stg_display = snap->manual_stage;

  char buf[TX_BUF_SIZE];
  int len = snprintf(buf, sizeof(buf),
      "%u mode:%s sub:%s cal:%u valid:%u "
      "adc:%u,%u,%u,%u base:%u,%u,%u,%u d:%u,%u,%u,%u "
      "err:%d,%d cmd:%d,%d enc:%d,%d stg:%u\r\n",
      h->seq,
      mode_str(snap->mode),
      sub_str(snap),
      snap->calibration_done,
      snap->source_valid,
      snap->adc[0], snap->adc[1], snap->adc[2], snap->adc[3],
      snap->baseline[0], snap->baseline[1], snap->baseline[2], snap->baseline[3],
      snap->delta[0], snap->delta[1], snap->delta[2], snap->delta[3],
      snap->error_x_x1000, snap->error_y_x1000,
      snap->cmd_axis1_hz, snap->cmd_axis2_hz,
      snap->enc1_count, snap->enc2_count,
      stg_display);

  if (len <= 0) return;
  if (len > (int)(sizeof(buf) - 1)) len = sizeof(buf) - 1;

  if (HAL_UART_Transmit(h->huart, (uint8_t *)buf, (uint16_t)len, TX_TIMEOUT_MS) == HAL_OK)
    h->seq++;
}
