#include "App/app_adc.h"
#include "App/tracking_config.h"

/* 簡單低通：權重比例定義在 tracking_config.h */
static uint16_t low_pass(uint16_t prev, uint16_t sample)
{
  uint32_t sum = (uint32_t)prev * ADC_LPF_OLD_WEIGHT
               + (uint32_t)sample * ADC_LPF_NEW_WEIGHT
               + ADC_LPF_SCALE / 2;
  return (uint16_t)(sum / ADC_LPF_SCALE);
}

/*
 * 啟動一組ADC的DMA circular模式
 * 關掉 HT/TC 中斷，避免每筆取樣都觸發ISR
 */
static void start_dma(ADC_HandleTypeDef *hadc, volatile uint16_t *buf, uint32_t len)
{
  if (HAL_ADC_Start_DMA(hadc, (uint32_t *)buf, len) != HAL_OK)
  {
    Error_Handler();
  }

  /* 不需要DMA完成中斷，circular模式會自動填值 */
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

  for (int i = 0; i < APP_ADC_TOTAL_CH; i++)
  {
    h->filtered[i] = 0;
    h->seeded[i] = 0;
  }

  start_dma(hadc1, h->dma_buf1, APP_ADC_CH_PER_DEVICE);
  start_dma(hadc2, h->dma_buf2, APP_ADC_CH_PER_DEVICE);
}

void AppAdc_Task(AppAdc_HandleTypeDef *h)
{
  /* 把兩顆ADC的DMA buffer組合成4路LDR
   * adc1[0] -> ch0, adc2[0] -> ch1, adc1[1] -> ch2, adc2[1] -> ch3 */
  uint16_t raw[APP_ADC_TOTAL_CH];
  raw[0] = h->dma_buf1[0];
  raw[1] = h->dma_buf2[0];
  raw[2] = h->dma_buf1[1];
  raw[3] = h->dma_buf2[1];

  for (int i = 0; i < APP_ADC_TOTAL_CH; i++)
  {
    if (h->seeded[i] == 0)
    {
      h->filtered[i] = raw[i];
      h->seeded[i] = 1;
    }
    else
    {
      h->filtered[i] = low_pass(h->filtered[i], raw[i]);
    }
  }
}

uint16_t AppAdc_GetFiltered(const AppAdc_HandleTypeDef *h, uint8_t ch)
{
  if (ch >= APP_ADC_TOTAL_CH) return 0;
  return h->filtered[ch];
}
