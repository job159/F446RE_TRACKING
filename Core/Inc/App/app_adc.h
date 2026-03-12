#ifndef APP_ADC_H
#define APP_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

typedef struct
{
  ADC_HandleTypeDef *hadc1;
  ADC_HandleTypeDef *hadc2;
  volatile uint16_t dma_adc1;
  volatile uint16_t dma_adc2;
  uint16_t raw_adc1;
  uint16_t raw_adc2;
  uint16_t filtered_adc1;
  uint16_t filtered_adc2;
  uint8_t adc1_seeded;
  uint8_t adc2_seeded;
} AppAdc_HandleTypeDef;

void AppAdc_Init(
    AppAdc_HandleTypeDef *handle,
    ADC_HandleTypeDef *hadc1,
    ADC_HandleTypeDef *hadc2);

void AppAdc_Task(AppAdc_HandleTypeDef *handle);

uint16_t AppAdc_GetFilteredAdc1(const AppAdc_HandleTypeDef *handle);
uint16_t AppAdc_GetFilteredAdc2(const AppAdc_HandleTypeDef *handle);

#ifdef __cplusplus
}
#endif

#endif /* APP_ADC_H */
