#ifndef APP_SEARCH_STRATEGY_H
#define APP_SEARCH_STRATEGY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "App/tracking_config.h"
#include "App/tracking_types.h"

typedef struct {
  TrackingHistoryEntry_t entries[SEARCH_HISTORY_LEN];
  uint8_t head;
  uint8_t count;
} TrackingHistory_HandleTypeDef;

typedef struct {
  SearchSubstate_t substate;
  uint32_t state_tick;
  uint8_t  bias_cycles;
  int32_t  bias_hz1;
  int32_t  bias_hz2;
  int32_t  last_good_enc1;
  int32_t  last_good_enc2;
  int8_t   sweep_dx;
  int8_t   sweep_dy;
  uint8_t  sweep_phase;
} SearchStrategy_HandleTypeDef;

void TrackingHistory_Init(TrackingHistory_HandleTypeDef *h);
void TrackingHistory_Push(TrackingHistory_HandleTypeDef *h,
    const LdrTrackingFrame_t *frame,
    int32_t enc1, int32_t enc2,
    const MotionCommand_t *cmd, uint32_t tick);
uint8_t TrackingHistory_GetLatestValid(const TrackingHistory_HandleTypeDef *h,
    TrackingHistoryEntry_t *out);

void SearchStrategy_Init(SearchStrategy_HandleTypeDef *h);
void SearchStrategy_Reset(SearchStrategy_HandleTypeDef *h);
void SearchStrategy_Enter(SearchStrategy_HandleTypeDef *h,
    const TrackingHistory_HandleTypeDef *hist, uint32_t now);
MotionCommand_t SearchStrategy_Run(SearchStrategy_HandleTypeDef *h,
    const TrackingHistory_HandleTypeDef *hist,
    int32_t enc1, int32_t enc2, uint32_t now);

#ifdef __cplusplus
}
#endif

#endif /* APP_SEARCH_STRATEGY_H */
