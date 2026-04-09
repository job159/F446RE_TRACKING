#include "App/search_strategy.h"
#include <string.h>

/* 確保bias速度至少有fallback那麼大，方向維持原本的 */
static int32_t clamp_magnitude(int32_t val, int32_t fallback)
{
  if (val > 0 && val < fallback) return fallback;
  if (val < 0 && val > -fallback) return -fallback;
  if (val == 0) return fallback;
  return val;
}

/* 取最近幾筆有效歷史的指令平均 */
static int32_t avg_recent_cmd(const TrackingHistory_HandleTypeDef *hist, uint8_t axis)
{
  if (hist == NULL || hist->count == 0) return 0;

  int32_t sum = 0;
  uint8_t used = 0;

  for (uint8_t off = 0; off < hist->count && used < 4; off++)
  {
    int32_t idx = (int32_t)hist->head - 1 - off;
    if (idx < 0) idx += SEARCH_HISTORY_LEN;

    if (hist->entries[idx].valid)
    {
      if (axis == 0)
        sum += hist->entries[idx].axis1_cmd_hz;
      else
        sum += hist->entries[idx].axis2_cmd_hz;
      used++;
    }
  }

  if (used == 0) return 0;
  return sum / used;
}

/* ---- TrackingHistory ---- */

void TrackingHistory_Init(TrackingHistory_HandleTypeDef *h)
{
  memset(h, 0, sizeof(*h));
}

void TrackingHistory_Push(TrackingHistory_HandleTypeDef *h,
    const LdrTrackingFrame_t *frame,
    int32_t enc1, int32_t enc2,
    const MotionCommand_t *cmd, uint32_t tick)
{
  if (frame->is_valid == 0) return;

  TrackingHistoryEntry_t *e = &h->entries[h->head];
  e->tick_ms = tick;
  e->error_x = frame->error_x;
  e->error_y = frame->error_y;
  e->axis1_cmd_hz = cmd->axis1_step_hz;
  e->axis2_cmd_hz = cmd->axis2_step_hz;
  e->enc1_count = enc1;
  e->enc2_count = enc2;
  e->total_light = frame->total;
  e->valid = 1;

  h->head = (h->head + 1) % SEARCH_HISTORY_LEN;
  if (h->count < SEARCH_HISTORY_LEN) h->count++;
}

uint8_t TrackingHistory_GetLatestValid(const TrackingHistory_HandleTypeDef *h,
    TrackingHistoryEntry_t *out)
{
  if (h->count == 0) return 0;

  int32_t idx = (int32_t)h->head - 1;
  if (idx < 0) idx += SEARCH_HISTORY_LEN;

  *out = h->entries[idx];
  return out->valid;
}

/* ---- SearchStrategy ---- */

void SearchStrategy_Init(SearchStrategy_HandleTypeDef *h)
{
  memset(h, 0, sizeof(*h));
  h->substate = SEARCH_HISTORY_BIAS;
  h->sweep_dx = 1;
  h->sweep_dy = 1;
}

void SearchStrategy_Reset(SearchStrategy_HandleTypeDef *h)
{
  SearchStrategy_Init(h);
}

void SearchStrategy_Enter(SearchStrategy_HandleTypeDef *h,
    const TrackingHistory_HandleTypeDef *hist, uint32_t now)
{
  SearchStrategy_Init(h);
  h->state_tick = now;

  /* 用最近歷史的指令方向當bias */
  h->bias_hz1 = clamp_magnitude(avg_recent_cmd(hist, 0), SEARCH_BIAS_STEP_HZ);
  h->bias_hz2 = clamp_magnitude(avg_recent_cmd(hist, 1), SEARCH_BIAS_STEP_HZ / 2);

  /* 紀錄最後一次有效的encoder位置 */
  TrackingHistoryEntry_t last = {0};
  if (hist != NULL && TrackingHistory_GetLatestValid(hist, &last))
  {
    h->last_good_enc1 = last.enc1_count;
    h->last_good_enc2 = last.enc2_count;
  }
}

MotionCommand_t SearchStrategy_Run(SearchStrategy_HandleTypeDef *h,
    const TrackingHistory_HandleTypeDef *hist,
    int32_t enc1, int32_t enc2, uint32_t now)
{
  (void)hist;
  MotionCommand_t cmd = {0, 0};
  uint32_t elapsed = now - h->state_tick;

  switch (h->substate)
  {
  case SEARCH_HISTORY_BIAS:
    /* 先依照歷史方向移動一段時間 */
    cmd.axis1_step_hz = h->bias_hz1;
    cmd.axis2_step_hz = h->bias_hz2;

    if (elapsed >= SEARCH_BIAS_HOLD_MS)
    {
      h->state_tick = now;
      h->bias_cycles++;
      if (h->bias_cycles >= SEARCH_HISTORY_BIAS_CYCLES)
        h->substate = SEARCH_REVISIT_LAST_GOOD;
    }
    break;

  case SEARCH_REVISIT_LAST_GOOD:
    /* 嘗試回到最後有效位置 */
    if (elapsed >= SEARCH_REVISIT_MAX_MS)
    {
      h->substate = SEARCH_SWEEP_SCAN;
      h->state_tick = now;
      break;
    }

    if (enc1 < h->last_good_enc1 - SEARCH_REVISIT_TOL_COUNTS)
      cmd.axis1_step_hz = SEARCH_REVISIT_STEP_HZ;
    else if (enc1 > h->last_good_enc1 + SEARCH_REVISIT_TOL_COUNTS)
      cmd.axis1_step_hz = -(int32_t)SEARCH_REVISIT_STEP_HZ;

    if (enc2 < h->last_good_enc2 - SEARCH_REVISIT_TOL_COUNTS)
      cmd.axis2_step_hz = SEARCH_REVISIT_STEP_HZ;
    else if (enc2 > h->last_good_enc2 + SEARCH_REVISIT_TOL_COUNTS)
      cmd.axis2_step_hz = -(int32_t)SEARCH_REVISIT_STEP_HZ;

    /* 兩軸都到了就進sweep */
    if (cmd.axis1_step_hz == 0 && cmd.axis2_step_hz == 0)
    {
      h->substate = SEARCH_SWEEP_SCAN;
      h->state_tick = now;
    }
    break;

  case SEARCH_SWEEP_SCAN:
  default:
    /* 左右掃描，偶數phase加Y軸 */
    cmd.axis1_step_hz = (int32_t)h->sweep_dx * SEARCH_SWEEP_STEP_HZ;
    if (h->sweep_phase & 1)
      cmd.axis2_step_hz = (int32_t)h->sweep_dy * SEARCH_SWEEP_Y_STEP_HZ;

    if (elapsed >= SEARCH_SWEEP_HOLD_MS)
    {
      h->state_tick = now;
      h->sweep_dx = -h->sweep_dx;
      h->sweep_phase++;
      if ((h->sweep_phase & 1) == 0)
        h->sweep_dy = -h->sweep_dy;
    }
    break;
  }

  return cmd;
}
