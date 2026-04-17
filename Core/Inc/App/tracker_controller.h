#ifndef APP_TRACKER_CONTROLLER_H
#define APP_TRACKER_CONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "App/tracking_types.h"

/* 純比例控制不需要任何狀態,保留 struct 讓呼叫端的 handle 介面不變 */
typedef struct {
  uint8_t _reserved;
} TrackerController_HandleTypeDef;

void TrackerController_Init(TrackerController_HandleTypeDef *h);
void TrackerController_Reset(TrackerController_HandleTypeDef *h);
MotionCommand_t TrackerController_Run(TrackerController_HandleTypeDef *h,
    const LdrTrackingFrame_t *frame, uint32_t period_ms);

#ifdef __cplusplus
}
#endif

#endif /* APP_TRACKER_CONTROLLER_H */
