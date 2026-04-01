#include "App/search_strategy.h"

#include <string.h>

static int32_t SearchStrategy_ClampSignedMagnitude(int32_t value, int32_t fallback_hz)
{
  if (value > 0)
  {
    return (value < fallback_hz) ? fallback_hz : value;
  }

  if (value < 0)
  {
    return (value > -fallback_hz) ? -fallback_hz : value;
  }

  return fallback_hz;
}

static int32_t SearchStrategy_AverageRecentCommand(
    const TrackingHistory_HandleTypeDef *history,
    uint8_t axis_index)
{
  int32_t sum = 0;
  uint8_t used = 0U;
  uint8_t offset;

  if ((history == NULL) || (history->count == 0U))
  {
    return 0;
  }

  for (offset = 0U; (offset < history->count) && (used < 4U); offset++)
  {
    int32_t physical_index = (int32_t)history->head - 1 - (int32_t)offset;

    if (physical_index < 0)
    {
      physical_index += SEARCH_HISTORY_LEN;
    }

    if (history->entries[physical_index].valid != 0U)
    {
      sum += (axis_index == 0U) ? history->entries[physical_index].axis1_cmd_hz
                                : history->entries[physical_index].axis2_cmd_hz;
      used++;
    }
  }

  if (used == 0U)
  {
    return 0;
  }

  return (sum / (int32_t)used);
}

void TrackingHistory_Init(TrackingHistory_HandleTypeDef *handle)
{
  if (handle == NULL)
  {
    return;
  }

  (void)memset(handle, 0, sizeof(*handle));
}

void TrackingHistory_Push(
    TrackingHistory_HandleTypeDef *handle,
    const LdrTrackingFrame_t *frame,
    int32_t enc1_count,
    int32_t enc2_count,
    const MotionCommand_t *command,
    uint32_t tick_ms)
{
  TrackingHistoryEntry_t *entry;

  if ((handle == NULL) || (frame == NULL) || (command == NULL) || (frame->is_valid == 0U))
  {
    return;
  }

  entry = &handle->entries[handle->head];
  entry->tick_ms = tick_ms;
  entry->error_x = frame->error_x;
  entry->error_y = frame->error_y;
  entry->axis1_cmd_hz = command->axis1_step_hz;
  entry->axis2_cmd_hz = command->axis2_step_hz;
  entry->enc1_count = enc1_count;
  entry->enc2_count = enc2_count;
  entry->total_light = frame->total;
  entry->valid = 1U;

  handle->head = (uint8_t)((handle->head + 1U) % SEARCH_HISTORY_LEN);
  if (handle->count < SEARCH_HISTORY_LEN)
  {
    handle->count++;
  }
}

uint8_t TrackingHistory_GetLatestValid(
    const TrackingHistory_HandleTypeDef *handle,
    TrackingHistoryEntry_t *entry)
{
  int32_t physical_index;

  if ((handle == NULL) || (entry == NULL) || (handle->count == 0U))
  {
    return 0U;
  }

  physical_index = (int32_t)handle->head - 1;
  if (physical_index < 0)
  {
    physical_index += SEARCH_HISTORY_LEN;
  }

  *entry = handle->entries[physical_index];
  return entry->valid;
}

void SearchStrategy_Init(SearchStrategy_HandleTypeDef *handle)
{
  if (handle == NULL)
  {
    return;
  }

  (void)memset(handle, 0, sizeof(*handle));
  handle->substate = SEARCH_HISTORY_BIAS;
  handle->sweep_dir_x = 1;
  handle->sweep_dir_y = 1;
}

void SearchStrategy_Reset(SearchStrategy_HandleTypeDef *handle)
{
  SearchStrategy_Init(handle);
}

void SearchStrategy_Enter(
    SearchStrategy_HandleTypeDef *handle,
    const TrackingHistory_HandleTypeDef *history,
    uint32_t now_ms)
{
  TrackingHistoryEntry_t last_good = {0};

  if (handle == NULL)
  {
    return;
  }

  SearchStrategy_Init(handle);
  handle->state_tick_ms = now_ms;
  handle->bias_axis1_hz = SearchStrategy_ClampSignedMagnitude(
      SearchStrategy_AverageRecentCommand(history, 0U),
      SEARCH_BIAS_STEP_HZ);
  handle->bias_axis2_hz = SearchStrategy_ClampSignedMagnitude(
      SearchStrategy_AverageRecentCommand(history, 1U),
      SEARCH_BIAS_STEP_HZ / 2);

  if ((history != NULL) && (TrackingHistory_GetLatestValid(history, &last_good) != 0U))
  {
    handle->last_good_enc1 = last_good.enc1_count;
    handle->last_good_enc2 = last_good.enc2_count;
  }
}

MotionCommand_t SearchStrategy_Run(
    SearchStrategy_HandleTypeDef *handle,
    const TrackingHistory_HandleTypeDef *history,
    int32_t enc1_count,
    int32_t enc2_count,
    uint32_t now_ms)
{
  MotionCommand_t command = {0, 0};
  uint32_t elapsed_ms;

  (void)history;

  if (handle == NULL)
  {
    return command;
  }

  elapsed_ms = now_ms - handle->state_tick_ms;

  switch (handle->substate)
  {
    case SEARCH_HISTORY_BIAS:
      command.axis1_step_hz = handle->bias_axis1_hz;
      command.axis2_step_hz = handle->bias_axis2_hz;

      if (elapsed_ms >= SEARCH_BIAS_HOLD_MS)
      {
        handle->state_tick_ms = now_ms;
        handle->history_bias_cycles++;
        if (handle->history_bias_cycles >= SEARCH_HISTORY_BIAS_CYCLES)
        {
          handle->substate = SEARCH_REVISIT_LAST_GOOD;
        }
      }
      break;

    case SEARCH_REVISIT_LAST_GOOD:
      if (elapsed_ms >= SEARCH_REVISIT_MAX_MS)
      {
        handle->substate = SEARCH_SWEEP_SCAN;
        handle->state_tick_ms = now_ms;
        break;
      }

      if ((enc1_count < (handle->last_good_enc1 - SEARCH_REVISIT_TOL_COUNTS)) ||
          (enc1_count > (handle->last_good_enc1 + SEARCH_REVISIT_TOL_COUNTS)))
      {
        command.axis1_step_hz = (enc1_count < handle->last_good_enc1) ? (int32_t)SEARCH_REVISIT_STEP_HZ
                                                                      : -(int32_t)SEARCH_REVISIT_STEP_HZ;
      }

      if ((enc2_count < (handle->last_good_enc2 - SEARCH_REVISIT_TOL_COUNTS)) ||
          (enc2_count > (handle->last_good_enc2 + SEARCH_REVISIT_TOL_COUNTS)))
      {
        command.axis2_step_hz = (enc2_count < handle->last_good_enc2) ? (int32_t)SEARCH_REVISIT_STEP_HZ
                                                                      : -(int32_t)SEARCH_REVISIT_STEP_HZ;
      }

      if ((command.axis1_step_hz == 0) && (command.axis2_step_hz == 0))
      {
        handle->substate = SEARCH_SWEEP_SCAN;
        handle->state_tick_ms = now_ms;
      }
      break;

    case SEARCH_SWEEP_SCAN:
    default:
      command.axis1_step_hz = (int32_t)handle->sweep_dir_x * (int32_t)SEARCH_SWEEP_STEP_HZ;
      command.axis2_step_hz = ((handle->sweep_phase & 0x01U) != 0U)
                                  ? ((int32_t)handle->sweep_dir_y * (int32_t)SEARCH_SWEEP_Y_STEP_HZ)
                                  : 0;

      if (elapsed_ms >= SEARCH_SWEEP_HOLD_MS)
      {
        handle->state_tick_ms = now_ms;
        handle->sweep_dir_x = (int8_t)(-handle->sweep_dir_x);
        handle->sweep_phase++;
        if ((handle->sweep_phase & 0x01U) == 0U)
        {
          handle->sweep_dir_y = (int8_t)(-handle->sweep_dir_y);
        }
      }
      break;
  }

  return command;
}
