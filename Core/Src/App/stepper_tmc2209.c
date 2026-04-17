#include "App/stepper_tmc2209.h"
#include <stdint.h>

/* TMC2209 UART協議相關 */
#define SYNC_BYTE       0x05
#define WRITE_BIT       0x80
#define REG_GCONF       0x00
#define REG_IFCNT       0x02
#define REG_SLAVECONF   0x03
#define REG_IHOLD_IRUN  0x10
#define REG_CHOPCONF    0x6C
#define REG_PWMCONF     0x70
#define FRAME_LEN       8
#define READ_FRAME_LEN  4
#define UART_TIMEOUT     30
#define DEFAULT_MICROSTEPS 16U

/* 暫存器預設值 */
#define GCONF_VAL        ((1UL << 6) | (1UL << 7))             /* pdn_disable + mstep_reg_select */
#define IHOLD_DEFAULT    6U
#define IRUN_DEFAULT     16U
#define IHOLDDELAY_DEFAULT 4U
#define IHOLD_IRUN_VAL   ((IHOLDDELAY_DEFAULT << 16) | (IRUN_DEFAULT << 8) | IHOLD_DEFAULT)
#define CHOPCONF_VAL     0x10000053UL                           /* base chopper setting, MRES另外套用 */
#define PWMCONF_VAL      0xC10D0024UL
#define SLAVECONF_VAL    (0x0FUL << 8)                           /* max read reply delay */

/* CHOPCONF bit[27:24] = MRES */
#define CHOPCONF_MRES_MASK   (0x0FUL << 24)

/* 加減速參數 */
#define RAMP_STEP_HZ     800
#define RAMP_DELAY_MS     1
#define DIR_SETTLE_MS     2

/* CRC8 for TMC2209 UART frame */
static uint8_t calc_crc8(const uint8_t *data, uint8_t len)
{
  uint8_t crc = 0;
  for (uint8_t i = 0; i < len; i++)
  {
    uint8_t byte = data[i];
    for (uint8_t b = 0; b < 8; b++)
    {
      if (((crc >> 7) ^ (byte & 0x01)) != 0)
        crc = (crc << 1) ^ 0x07;
      else
        crc <<= 1;
      byte >>= 1;
    }
  }
  return crc;
}

/* 判斷timer掛在APB2還是APB1 */
static uint8_t is_apb2_timer(const TIM_TypeDef *inst)
{
  return (inst == TIM1 || inst == TIM8 ||
          inst == TIM9 || inst == TIM10 || inst == TIM11);
}

static uint32_t get_timer_clock(const TIM_HandleTypeDef *htim)
{
  if (is_apb2_timer(htim->Instance))
  {
    uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE2) == RCC_CFGR_PPRE2_DIV1) return pclk2;
    return pclk2 * 2;
  }
  uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
  if ((RCC->CFGR & RCC_CFGR_PPRE1) == RCC_CFGR_PPRE1_DIV1) return pclk1;
  return pclk1 * 2;
}

static uint8_t microsteps_to_mres(uint16_t microsteps, uint8_t *mres)
{
  if (mres == NULL) return 0;

  switch (microsteps)
  {
  case 256: *mres = 0; return 1;
  case 128: *mres = 1; return 1;
  case 64:  *mres = 2; return 1;
  case 32:  *mres = 3; return 1;
  case 16:  *mres = 4; return 1;
  case 8:   *mres = 5; return 1;
  case 4:   *mres = 6; return 1;
  case 2:   *mres = 7; return 1;
  case 1:   *mres = 8; return 1;
  default:  return 0;
  }
}

static uint8_t make_chopconf(uint16_t microsteps, uint32_t *out)
{
  uint8_t mres;
  if (out == NULL || !microsteps_to_mres(microsteps, &mres)) return 0;

  *out = (CHOPCONF_VAL & ~CHOPCONF_MRES_MASK) | ((uint32_t)mres << 24);
  return 1;
}

static uint8_t make_ihold_irun(StepperTmc2209_CurrentConfig_t cfg, uint32_t *out)
{
  if (out == NULL) return 0;
  if (cfg.ihold > 31U || cfg.irun > 31U || cfg.iholddelay > 15U) return 0;

  *out = ((uint32_t)cfg.iholddelay << 16) |
         ((uint32_t)cfg.irun << 8) |
         (uint32_t)cfg.ihold;
  return 1;
}

static void clear_uart_rx(UART_HandleTypeDef *huart)
{
  if (huart == NULL) return;

  if (__HAL_UART_GET_FLAG(huart, UART_FLAG_ORE))
    __HAL_UART_CLEAR_OREFLAG(huart);
  if (__HAL_UART_GET_FLAG(huart, UART_FLAG_NE))
    __HAL_UART_CLEAR_NEFLAG(huart);
  if (__HAL_UART_GET_FLAG(huart, UART_FLAG_FE))
    __HAL_UART_CLEAR_FEFLAG(huart);

  while (__HAL_UART_GET_FLAG(huart, UART_FLAG_RXNE))
  {
    volatile uint32_t dr = huart->Instance->DR;
    (void)dr;
  }
}

/* 寫一個TMC2209暫存器 */
static HAL_StatusTypeDef write_reg(const StepperTmc2209_HandleTypeDef *h,
    uint8_t reg, uint32_t val)
{
  uint8_t frame[FRAME_LEN];
  frame[0] = SYNC_BYTE;
  frame[1] = h->slave_addr;
  frame[2] = reg | WRITE_BIT;
  frame[3] = (val >> 24) & 0xFF;
  frame[4] = (val >> 16) & 0xFF;
  frame[5] = (val >> 8) & 0xFF;
  frame[6] = val & 0xFF;
  frame[7] = calc_crc8(frame, 7);

  return HAL_UART_Transmit(h->huart, frame, FRAME_LEN, UART_TIMEOUT);
}

static HAL_StatusTypeDef read_reg(StepperTmc2209_HandleTypeDef *h,
    uint8_t reg, uint32_t *val)
{
  if (h == NULL || h->huart == NULL || val == NULL) return HAL_ERROR;

  uint8_t req[READ_FRAME_LEN];
  uint8_t reply[FRAME_LEN];

  req[0] = SYNC_BYTE;
  req[1] = h->slave_addr;
  req[2] = reg & 0x7FU;
  req[3] = calc_crc8(req, 3);

  clear_uart_rx(h->huart);

  HAL_StatusTypeDef st = HAL_UART_Transmit(h->huart, req, READ_FRAME_LEN, UART_TIMEOUT);
  if (st != HAL_OK) return st;

  clear_uart_rx(h->huart);

  st = HAL_UART_Receive(h->huart, reply, FRAME_LEN, UART_TIMEOUT);
  if (st != HAL_OK) return st;

  if (reply[0] != SYNC_BYTE || reply[1] != 0xFFU || reply[2] != (reg & 0x7FU))
    return HAL_ERROR;
  if (reply[7] != calc_crc8(reply, 7))
    return HAL_ERROR;

  *val = ((uint32_t)reply[3] << 24) |
         ((uint32_t)reply[4] << 16) |
         ((uint32_t)reply[5] << 8) |
         (uint32_t)reply[6];
  return HAL_OK;
}

/* 初始化TMC2209的四個暫存器 */
static HAL_StatusTypeDef config_registers(const StepperTmc2209_HandleTypeDef *h)
{
  HAL_StatusTypeDef st;
  uint32_t chopconf;

  st = write_reg(h, REG_GCONF, GCONF_VAL);
  if (st != HAL_OK) return st;
  HAL_Delay(1);

  st = write_reg(h, REG_SLAVECONF, SLAVECONF_VAL);
  if (st != HAL_OK) return st;
  HAL_Delay(1);

  st = write_reg(h, REG_IHOLD_IRUN, IHOLD_IRUN_VAL);
  if (st != HAL_OK) return st;
  HAL_Delay(1);

  if (!make_chopconf(h->microsteps, &chopconf)) return HAL_ERROR;
  st = write_reg(h, REG_CHOPCONF, chopconf);
  if (st != HAL_OK) return st;
  HAL_Delay(1);

  return write_reg(h, REG_PWMCONF, PWMCONF_VAL);
}

/* 設定step頻率，直接改ARR跟CCR */
static HAL_StatusTypeDef set_step_freq(StepperTmc2209_HandleTypeDef *h, uint16_t hz)
{
  if (hz == 0) return HAL_ERROR;

  uint32_t clk = get_timer_clock(h->htim_step);
  uint32_t counter_clk = clk / (h->htim_step->Init.Prescaler + 1);
  if (counter_clk <= hz) return HAL_ERROR;

  uint32_t arr = counter_clk / hz;
  if (arr < 4) arr = 4;
  arr -= 1;

  __HAL_TIM_SET_AUTORELOAD(h->htim_step, arr);
  __HAL_TIM_SET_COMPARE(h->htim_step, h->step_channel, (arr + 1) / 2);
  __HAL_TIM_SET_COUNTER(h->htim_step, 0);
  return HAL_TIM_GenerateEvent(h->htim_step, TIM_EVENTSOURCE_UPDATE);
}

/* 漸進式改變step頻率，每次最多跳RAMP_STEP_HZ */
static HAL_StatusTypeDef ramp_freq(StepperTmc2209_HandleTypeDef *h,
    uint16_t from, uint16_t to)
{
  if (from == to)
    return set_step_freq(h, to);

  uint32_t cur = from;
  uint32_t target = to;

  while (cur != target)
  {
    if (cur < target)
    {
      uint32_t gap = target - cur;
      if (gap > RAMP_STEP_HZ) gap = RAMP_STEP_HZ;
      cur += gap;
    }
    else
    {
      uint32_t gap = cur - target;
      if (gap > RAMP_STEP_HZ) gap = RAMP_STEP_HZ;
      cur -= gap;
    }

    HAL_StatusTypeDef st = set_step_freq(h, (uint16_t)cur);
    if (st != HAL_OK) return st;

    if (cur != target) HAL_Delay(RAMP_DELAY_MS);
  }
  return HAL_OK;
}

static void set_dir(StepperTmc2209_HandleTypeDef *h, GPIO_PinState dir)
{
  HAL_GPIO_WritePin(h->dir_port, h->dir_pin, dir);
  h->current_dir = dir;
}

/* ---- public functions ---- */

HAL_StatusTypeDef StepperTmc2209_Init(
    StepperTmc2209_HandleTypeDef *h,
    TIM_HandleTypeDef *htim_step, uint32_t step_channel,
    UART_HandleTypeDef *huart,
    GPIO_TypeDef *dir_port, uint16_t dir_pin,
    GPIO_TypeDef *en_port,  uint16_t en_pin,
    uint8_t slave_addr,
    const uint16_t speed_table[TMC_SPEED_STAGE_COUNT])
{
  /* 預設速度表 */
  static const uint16_t default_table[TMC_SPEED_STAGE_COUNT] = {
    200, 1400, 5000, 7500,
    200, 1400, 5000, 7500
  };

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
  h->microsteps   = DEFAULT_MICROSTEPS;
  h->current_config.ihold = IHOLD_DEFAULT;
  h->current_config.irun = IRUN_DEFAULT;
  h->current_config.iholddelay = IHOLDDELAY_DEFAULT;

  const uint16_t *table = default_table;
  if (speed_table != NULL) table = speed_table;
  for (int i = 0; i < TMC_SPEED_STAGE_COUNT; i++)
    h->speed_hz[i] = table[i];

  /* 先disable再設方向，等driver醒來 */
  HAL_GPIO_WritePin(en_port, en_pin, GPIO_PIN_SET);    /* disable */
  HAL_GPIO_WritePin(dir_port, dir_pin, GPIO_PIN_RESET); /* forward */
  HAL_Delay(2);

  HAL_StatusTypeDef st = config_registers(h);
  if (st != HAL_OK) return st;

  HAL_GPIO_WritePin(en_port, en_pin, GPIO_PIN_RESET);  /* enable */

  st = HAL_TIM_PWM_Start(htim_step, step_channel);
  if (st != HAL_OK) return st;

  return StepperTmc2209_SetSpeedStage(h, 0);
}

HAL_StatusTypeDef StepperTmc2209_SetSpeedStage(StepperTmc2209_HandleTypeDef *h, uint8_t stage)
{
  if (stage >= TMC_SPEED_STAGE_COUNT) return HAL_ERROR;

  GPIO_PinState want_dir = GPIO_PIN_SET;
  if (stage < TMC_DIR_SPLIT_STAGE) want_dir = GPIO_PIN_RESET;
  uint16_t want_hz = h->speed_hz[stage];
  HAL_StatusTypeDef st;

  /* 找方向切換時的起始速度 */
  uint8_t base_idx = TMC_DIR_SPLIT_STAGE;
  if (want_dir == GPIO_PIN_RESET) base_idx = 0;
  uint16_t base_hz = h->speed_hz[base_idx];
  if (base_hz == 0) base_hz = want_hz;

  if (h->current_hz == 0)
  {
    /* 馬達靜止，直接設方向後啟動 */
    set_dir(h, want_dir);
    HAL_Delay(DIR_SETTLE_MS);
    st = set_step_freq(h, want_hz);
  }
  else if (h->current_dir != want_dir)
  {
    /* 先ramp到base速度，切方向，再ramp到目標 */
    st = ramp_freq(h, h->current_hz, base_hz);
    if (st != HAL_OK) return st;
    set_dir(h, want_dir);
    HAL_Delay(DIR_SETTLE_MS);
    st = ramp_freq(h, base_hz, want_hz);
  }
  else
  {
    set_dir(h, want_dir);
    st = ramp_freq(h, h->current_hz, want_hz);
  }

  if (st == HAL_OK)
  {
    h->speed_index = stage;
    h->current_dir = want_dir;
    h->current_hz = want_hz;
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
  if (signed_hz == 0)
    return StepperTmc2209_Stop(h);

  uint32_t mag;
  if (signed_hz > 0) mag = (uint32_t)signed_hz;
  else mag = (uint32_t)(-signed_hz);
  if (mag > UINT16_MAX) mag = UINT16_MAX;

  GPIO_PinState dir = GPIO_PIN_SET;
  if (signed_hz > 0) dir = GPIO_PIN_RESET;
  uint16_t hz = (uint16_t)mag;

  /* 找方向切換時的base速度 */
  uint8_t base_idx = TMC_DIR_SPLIT_STAGE;
  if (dir == GPIO_PIN_RESET) base_idx = 0;
  uint16_t base_hz = h->speed_hz[base_idx];
  if (base_hz == 0) base_hz = hz;

  HAL_StatusTypeDef st;

  if (h->current_hz == 0)
  {
    set_dir(h, dir);
    HAL_Delay(DIR_SETTLE_MS);
    st = set_step_freq(h, hz);
  }
  else if (h->current_dir != dir)
  {
    st = ramp_freq(h, h->current_hz, base_hz);
    if (st != HAL_OK) return st;
    set_dir(h, dir);
    HAL_Delay(DIR_SETTLE_MS);
    st = ramp_freq(h, base_hz, hz);
  }
  else
  {
    st = ramp_freq(h, h->current_hz, hz);
  }

  if (st == HAL_OK)
  {
    h->current_dir = dir;
    h->current_hz = hz;
  }
  return st;
}

HAL_StatusTypeDef StepperTmc2209_SetMicrosteps(StepperTmc2209_HandleTypeDef *h, uint16_t microsteps)
{
  uint32_t chopconf;
  if (h == NULL || !make_chopconf(microsteps, &chopconf)) return HAL_ERROR;

  HAL_StatusTypeDef st = write_reg(h, REG_GCONF, GCONF_VAL);
  if (st != HAL_OK) return st;
  HAL_Delay(1);

  st = write_reg(h, REG_CHOPCONF, chopconf);
  if (st == HAL_OK)
    h->microsteps = microsteps;
  return st;
}

HAL_StatusTypeDef StepperTmc2209_SetCurrentConfig(StepperTmc2209_HandleTypeDef *h,
    StepperTmc2209_CurrentConfig_t cfg)
{
  uint32_t ihold_irun;
  if (h == NULL || !make_ihold_irun(cfg, &ihold_irun)) return HAL_ERROR;

  HAL_StatusTypeDef st = write_reg(h, REG_IHOLD_IRUN, ihold_irun);
  if (st == HAL_OK)
    h->current_config = cfg;
  return st;
}

HAL_StatusTypeDef StepperTmc2209_ReadGconf(StepperTmc2209_HandleTypeDef *h, uint32_t *gconf)
{
  return read_reg(h, REG_GCONF, gconf);
}

HAL_StatusTypeDef StepperTmc2209_ReadIfcnt(StepperTmc2209_HandleTypeDef *h, uint8_t *ifcnt)
{
  uint32_t val;
  if (ifcnt == NULL) return HAL_ERROR;

  HAL_StatusTypeDef st = read_reg(h, REG_IFCNT, &val);
  if (st == HAL_OK)
    *ifcnt = (uint8_t)(val & 0xFFU);
  return st;
}

HAL_StatusTypeDef StepperTmc2209_ReadIholdIrun(StepperTmc2209_HandleTypeDef *h, uint32_t *ihold_irun)
{
  return read_reg(h, REG_IHOLD_IRUN, ihold_irun);
}

HAL_StatusTypeDef StepperTmc2209_ReadChopconf(StepperTmc2209_HandleTypeDef *h, uint32_t *chopconf)
{
  return read_reg(h, REG_CHOPCONF, chopconf);
}

uint8_t StepperTmc2209_GetSpeedStage(const StepperTmc2209_HandleTypeDef *h)
{
  return h->speed_index;
}

uint16_t StepperTmc2209_GetMicrosteps(const StepperTmc2209_HandleTypeDef *h)
{
  if (h == NULL) return 0;
  return h->microsteps;
}

StepperTmc2209_CurrentConfig_t StepperTmc2209_GetCurrentConfig(const StepperTmc2209_HandleTypeDef *h)
{
  StepperTmc2209_CurrentConfig_t cfg = {0};
  if (h == NULL) return cfg;
  return h->current_config;
}
