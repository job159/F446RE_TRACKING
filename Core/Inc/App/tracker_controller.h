#ifndef APP_TRACKER_CONTROLLER_H
#define APP_TRACKER_CONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "App/tracking_types.h"

typedef struct
{
  AxisController_t axis1;
  AxisController_t axis2;
} TrackerController_HandleTypeDef;

void TrackerController_Init(TrackerController_HandleTypeDef *handle);
void TrackerController_Reset(TrackerController_HandleTypeDef *handle);
MotionCommand_t TrackerController_Run(
    TrackerController_HandleTypeDef *handle,
    const LdrTrackingFrame_t *frame,
    uint32_t control_period_ms);

#ifdef __cplusplus
}
#endif

#endif /* APP_TRACKER_CONTROLLER_H */
