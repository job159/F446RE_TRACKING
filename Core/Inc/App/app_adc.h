#ifndef APP_ADC_H
#define APP_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* 每顆 ADC 掃 2 通道,共 4 路 LDR */
#define APP_ADC_CH_PER_DEVICE  2U
#define APP_ADC_TOTAL_CH       4U

typedef struct {
  ADC_HandleTypeDef *hadc1;
  ADC_HandleTypeDef *hadc2;
  volatile uint16_t  dma_buf1[APP_ADC_CH_PER_DEVICE];
  volatile uint16_t  dma_buf2[APP_ADC_CH_PER_DEVICE];
  uint16_t           filtered[APP_ADC_TOTAL_CH];
  uint8_t            seeded;
} AppAdc_HandleTypeDef;

void     AppAdc_Init(AppAdc_HandleTypeDef *h, ADC_HandleTypeDef *hadc1, ADC_HandleTypeDef *hadc2);
void     AppAdc_Task(AppAdc_HandleTypeDef *h);
uint16_t AppAdc_GetFiltered(const AppAdc_HandleTypeDef *h, uint8_t ch);

#ifdef __cplusplus
}
#endif

#endif /* APP_ADC_H */
