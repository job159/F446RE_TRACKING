#include "App/stepper_tmc2209.h"

/* =========================================================================
 *  TMC2209 暫存器設定 (易讀,直接改值即可)
 * =========================================================================
 *  細分(microstep) = 32  -> CHOPCONF.MRES = 3
 *  電流: irun=16, ihold=6, iholddelay=4
 * ========================================================================= */
#define TMC_MICROSTEP        32U   /* 1/32 細分 */

#define REG_GCONF            0x00
#define REG_IHOLD_IRUN       0x10
#define REG_CHOPCONF         0x6C
#define REG_PWMCONF          0x70

#define VAL_GCONF            ((1UL << 6) | (1UL << 7))  /* pdn_disable + mstep_reg_select */
#define VAL_IHOLD_IRUN       ((4UL << 16) | (16UL << 8) | 6UL)
#define VAL_PWMCONF          0xC10D0024UL

/* CHOPCONF 基底 + MRES 欄位 (bit27:24)
 * MRES: 0=256, 1=128, 2=64, 3=32, 4=16, 5=8, 6=4 */
#define CHOPCONF_BASE        0x10000053UL
#define CHOPCONF_MRES_BITS   (3UL << 24)              /* 1/32 = MRES=3 */
#define VAL_CHOPCONF         ((CHOPCONF_BASE & ~(0x0FUL << 24)) | CHOPCONF_MRES_BITS)

/* UART frame */
#define SYNC_BYTE            0x05
#define WRITE_BIT            0x80
#define FRAME_LEN            8
#define UART_TIMEOUT         30

/* Ramp 漸進式調速 (避免突然失步) */
#define RAMP_STEP_HZ         800
#define RAMP_DELAY_MS        1
#define DIR_SETTLE_MS        2

/* ---------- 內部工具 ---------- */

static uint8_t calc_crc8(const uint8_t *data, uint8_t len)
{
  uint8_t crc = 0;
  for (uint8_t i = 0; i < len; i++)
  {
    uint8_t byte = data[i];
    for (uint8_t b = 0; b < 8; b++)
    {
      crc = (((crc >> 7) ^ (byte & 0x01)) != 0) ? ((crc << 1) ^ 0x07) : (crc << 1);
      byte >>= 1;
    }
  }
  return crc;
}

static uint32_t get_timer_clock(const TIM_HandleTypeDef *htim)
{
  /* TIM1 在 APB2,其餘 (TIM2/3/5) 在 APB1 */
  uint8_t apb2 = (htim->Instance == TIM1);
  uint32_t pclk = apb2 ? HAL_RCC_GetPCLK2Freq() : HAL_RCC_GetPCLK1Freq();
  uint32_t ppre_mask = apb2 ? RCC_CFGR_PPRE2 : RCC_CFGR_PPRE1;
  uint32_t ppre_div1 = apb2 ? RCC_CFGR_PPRE2_DIV1 : RCC_CFGR_PPRE1_DIV1;
  return ((RCC->CFGR & ppre_mask) == ppre_div1) ? pclk : (pclk * 2);
}

static HAL_StatusTypeDef write_reg(const StepperTmc2209_HandleTypeDef *h, uint8_t reg, uint32_t val)
{
  uint8_t f[FRAME_LEN];
  f[0] = SYNC_BYTE;
  f[1] = h->slave_addr;
  f[2] = reg | WRITE_BIT;
  f[3] = (val >> 24) & 0xFF;
  f[4] = (val >> 16) & 0xFF;
  f[5] = (val >> 8) & 0xFF;
  f[6] = val & 0xFF;
  f[7] = calc_crc8(f, 7);
  return HAL_UART_Transmit(h->huart, f, FRAME_LEN, UART_TIMEOUT);
}

static HAL_StatusTypeDef config_registers(const StepperTmc2209_HandleTypeDef *h)
{
  if (write_reg(h, REG_GCONF,      VAL_GCONF)      != HAL_OK) return HAL_ERROR;
  HAL_Delay(1);
  if (write_reg(h, REG_IHOLD_IRUN, VAL_IHOLD_IRUN) != HAL_OK) return HAL_ERROR;
  HAL_Delay(1);
  if (write_reg(h, REG_CHOPCONF,   VAL_CHOPCONF)   != HAL_OK) return HAL_ERROR;
  HAL_Delay(1);
  return write_reg(h, REG_PWMCONF, VAL_PWMCONF);
}

static HAL_StatusTypeDef set_step_freq(StepperTmc2209_HandleTypeDef *h, uint16_t hz)
{
  if (hz == 0) return HAL_ERROR;

  uint32_t counter_clk = get_timer_clock(h->htim_step) / (h->htim_step->Init.Prescaler + 1);
  if (counter_clk <= hz) return HAL_ERROR;

  uint32_t arr = counter_clk / hz;
  if (arr < 4) arr = 4;
  arr -= 1;

  __HAL_TIM_SET_AUTORELOAD(h->htim_step, arr);
  __HAL_TIM_SET_COMPARE(h->htim_step, h->step_channel, (arr + 1) / 2);
  __HAL_TIM_SET_COUNTER(h->htim_step, 0);
  return HAL_TIM_GenerateEvent(h->htim_step, TIM_EVENTSOURCE_UPDATE);
}

static HAL_StatusTypeDef ramp_freq(StepperTmc2209_HandleTypeDef *h, uint16_t from, uint16_t to)
{
  if (from == to) return set_step_freq(h, to);

  int32_t cur = from;
  int32_t tgt = to;
  while (cur != tgt)
  {
    int32_t step = (tgt > cur) ? (tgt - cur) : (cur - tgt);
    if (step > RAMP_STEP_HZ) step = RAMP_STEP_HZ;
    cur += (tgt > cur) ? step : -step;

    HAL_StatusTypeDef st = set_step_freq(h, (uint16_t)cur);
    if (st != HAL_OK) return st;
    if (cur != tgt) HAL_Delay(RAMP_DELAY_MS);
  }
  return HAL_OK;
}

static void set_dir(StepperTmc2209_HandleTypeDef *h, GPIO_PinState dir)
{
  HAL_GPIO_WritePin(h->dir_port, h->dir_pin, dir);
  h->current_dir = dir;
}

/* 帶方向切換的 ramp:停->動 / 同向 / 反向各自處理
 * 停->動只有在方向真的要換時才做 DIR_SETTLE_MS 的等待,
 * 否則直接起步,避免 tracking 死區邊緣每 cycle 都卡 2ms 使主迴圈餓死。 */
static HAL_StatusTypeDef apply_speed(StepperTmc2209_HandleTypeDef *h, GPIO_PinState dir, uint16_t hz)
{
  if (h->current_hz == 0)
  {
    if (h->current_dir != dir)
    {
      set_dir(h, dir);
      HAL_Delay(DIR_SETTLE_MS);
    }
    return set_step_freq(h, hz);
  }
  if (h->current_dir != dir)
  {
    /* 反向: 先 ramp 到最低速 -> 切方向 -> ramp 到目標 */
    uint16_t low_hz = h->speed_hz[0];
    if (low_hz == 0) low_hz = hz;
    HAL_StatusTypeDef st = ramp_freq(h, h->current_hz, low_hz);
    if (st != HAL_OK) return st;
    set_dir(h, dir);
    HAL_Delay(DIR_SETTLE_MS);
    return ramp_freq(h, low_hz, hz);
  }
  return ramp_freq(h, h->current_hz, hz);
}

/* ---------- public ---------- */

HAL_StatusTypeDef StepperTmc2209_Init(
    StepperTmc2209_HandleTypeDef *h,
    TIM_HandleTypeDef *htim_step, uint32_t step_channel,
    UART_HandleTypeDef *huart,
    GPIO_TypeDef *dir_port, uint16_t dir_pin,
    GPIO_TypeDef *en_port,  uint16_t en_pin,
    uint8_t slave_addr,
    const uint16_t speed_table[TMC_SPEED_STAGE_COUNT])
{
  h->htim_step    = htim_step;
  h->step_channel = step_channel;
  h->huart        = huart;
  h->dir_port     = dir_port;
  h->dir_pin      = dir_pin;
  h->en_port      = en_port;
  h->en_pin       = en_pin;
  h->slave_addr   = slave_addr;
  h->speed_index  = 0;
  h->current_hz   = 0;
  h->current_dir  = GPIO_PIN_RESET;

  for (int i = 0; i < TMC_SPEED_STAGE_COUNT; i++)
    h->speed_hz[i] = (speed_table != NULL) ? speed_table[i] : 0;

  HAL_GPIO_WritePin(en_port, en_pin, GPIO_PIN_SET);     /* disable */
  HAL_GPIO_WritePin(dir_port, dir_pin, GPIO_PIN_RESET); /* forward */
  HAL_Delay(2);

  if (config_registers(h) != HAL_OK) return HAL_ERROR;

  HAL_GPIO_WritePin(en_port, en_pin, GPIO_PIN_RESET);   /* enable */

  if (HAL_TIM_PWM_Start(htim_step, step_channel) != HAL_OK) return HAL_ERROR;
  return StepperTmc2209_SetSpeedStage(h, 0);
}

HAL_StatusTypeDef StepperTmc2209_SetSpeedStage(StepperTmc2209_HandleTypeDef *h, uint8_t stage)
{
  if (stage >= TMC_SPEED_STAGE_COUNT) return HAL_ERROR;

  GPIO_PinState dir = (stage < TMC_DIR_SPLIT_STAGE) ? GPIO_PIN_RESET : GPIO_PIN_SET;
  uint16_t hz = h->speed_hz[stage];

  HAL_StatusTypeDef st = apply_speed(h, dir, hz);
  if (st == HAL_OK)
  {
    h->speed_index = stage;
    h->current_hz  = hz;
  }
  return st;
}

HAL_StatusTypeDef StepperTmc2209_Stop(StepperTmc2209_HandleTypeDef *h)
{
  __HAL_TIM_SET_COMPARE(h->htim_step, h->step_channel, 0);
  __HAL_TIM_SET_COUNTER(h->htim_step, 0);
  HAL_TIM_GenerateEvent(h->htim_step, TIM_EVENTSOURCE_UPDATE);
  h->current_hz = 0;
  return HAL_OK;
}

HAL_StatusTypeDef StepperTmc2209_SetSignedHz(StepperTmc2209_HandleTypeDef *h, int32_t signed_hz)
{
  if (signed_hz == 0) return StepperTmc2209_Stop(h);

  uint32_t mag = (signed_hz > 0) ? (uint32_t)signed_hz : (uint32_t)(-signed_hz);
  if (mag > UINT16_MAX) mag = UINT16_MAX;

  GPIO_PinState dir = (signed_hz > 0) ? GPIO_PIN_RESET : GPIO_PIN_SET;
  HAL_StatusTypeDef st = apply_speed(h, dir, (uint16_t)mag);
  if (st == HAL_OK) h->current_hz = (uint16_t)mag;
  return st;
}
