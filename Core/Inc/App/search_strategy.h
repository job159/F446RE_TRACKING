#ifndef APP_SEARCH_STRATEGY_H
#define APP_SEARCH_STRATEGY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "App/tracking_config.h"
#include "App/tracking_types.h"

typedef struct
{
  TrackingHistoryEntry_t entries[SEARCH_HISTORY_LEN];
  uint8_t head;
  uint8_t count;
} TrackingHistory_HandleTypeDef;

typedef struct
{
  SearchSubstate_t substate;
  uint32_t state_tick_ms;
  uint8_t history_bias_cycles;
  int32_t bias_axis1_hz;
  int32_t bias_axis2_hz;
  int32_t last_good_enc1;
  int32_t last_good_enc2;
  int8_t sweep_dir_x;
  int8_t sweep_dir_y;
  uint8_t sweep_phase;
} SearchStrategy_HandleTypeDef;

void TrackingHistory_Init(TrackingHistory_HandleTypeDef *handle);
void TrackingHistory_Push(
    TrackingHistory_HandleTypeDef *handle,
    const LdrTrackingFrame_t *frame,
    int32_t enc1_count,
    int32_t enc2_count,
    const MotionCommand_t *command,
    uint32_t tick_ms);
uint8_t TrackingHistory_GetLatestValid(
    const TrackingHistory_HandleTypeDef *handle,
    TrackingHistoryEntry_t *entry);

void SearchStrategy_Init(SearchStrategy_HandleTypeDef *handle);
void SearchStrategy_Reset(SearchStrategy_HandleTypeDef *handle);
void SearchStrategy_Enter(
    SearchStrategy_HandleTypeDef *handle,
    const TrackingHistory_HandleTypeDef *history,
    uint32_t now_ms);
MotionCommand_t SearchStrategy_Run(
    SearchStrategy_HandleTypeDef *handle,
    const TrackingHistory_HandleTypeDef *history,
    int32_t enc1_count,
    int32_t enc2_count,
    uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* APP_SEARCH_STRATEGY_H */
