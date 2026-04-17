#include "App/app_adc.h"
#include "App/tracking_config.h"

/* 簡單低通: filtered = (old*1 + new*ALPHA) / (ALPHA+1)
 * 預設 ALPHA = 9 -> 新值權重 90% */
static uint16_t low_pass(uint16_t old_val, uint16_t new_val)
{
  uint32_t denom = (uint32_t)ADC_LPF_ALPHA_NEW + 1U;
  uint32_t sum   = (uint32_t)old_val + (uint32_t)new_val * ADC_LPF_ALPHA_NEW;
  return (uint16_t)((sum + denom / 2U) / denom);
}

/* 採樣讀進來的原始值,如果硬體分壓反向就翻過來 */
static uint16_t orient(uint16_t raw)
{
#if ADC_INVERT
  return (uint16_t)(ADC_12BIT_MAX - raw);
#else
  return raw;
#endif
}

/* circular DMA 會自己填 buffer,不需要 HT/TC 中斷;
 * 不關掉的話 ADC 轉換太快,ISR 一直觸發把 CPU 吃滿,主迴圈看起來像卡死。 */
static void start_adc_dma(ADC_HandleTypeDef *hadc, volatile uint16_t *buf, uint32_t len)
{
  HAL_ADC_Start_DMA(hadc, (uint32_t *)buf, len);
  if (hadc->DMA_Handle != NULL)
  {
    __HAL_DMA_DISABLE_IT(hadc->DMA_Handle, DMA_IT_HT);
    __HAL_DMA_DISABLE_IT(hadc->DMA_Handle, DMA_IT_TC);
  }
}

void AppAdc_Init(AppAdc_HandleTypeDef *h, ADC_HandleTypeDef *hadc1, ADC_HandleTypeDef *hadc2)
{
  h->hadc1 = hadc1;
  h->hadc2 = hadc2;
  h->seeded = 0;
  for (int i = 0; i < APP_ADC_TOTAL_CH; i++) h->filtered[i] = 0;

  start_adc_dma(hadc1, h->dma_buf1, APP_ADC_CH_PER_DEVICE);
  start_adc_dma(hadc2, h->dma_buf2, APP_ADC_CH_PER_DEVICE);
}

void AppAdc_Task(AppAdc_HandleTypeDef *h)
{
  /* 兩顆 ADC 各 2 通道,組合成 4 路 LDR
   *   ch0 = adc1[0]   ch1 = adc2[0]
   *   ch2 = adc1[1]   ch3 = adc2[1] */
  uint16_t raw[APP_ADC_TOTAL_CH] = {
    orient(h->dma_buf1[0]),
    orient(h->dma_buf2[0]),
    orient(h->dma_buf1[1]),
    orient(h->dma_buf2[1])
  };

  if (!h->seeded)
  {
    for (int i = 0; i < APP_ADC_TOTAL_CH; i++) h->filtered[i] = raw[i];
    h->seeded = 1;
    return;
  }

  for (int i = 0; i < APP_ADC_TOTAL_CH; i++)
    h->filtered[i] = low_pass(h->filtered[i], raw[i]);
}

uint16_t AppAdc_GetFiltered(const AppAdc_HandleTypeDef *h, uint8_t ch)
{
  if (ch >= APP_ADC_TOTAL_CH) return 0;
  return h->filtered[ch];
}
