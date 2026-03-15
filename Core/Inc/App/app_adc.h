#ifndef APP_ADC_H
#define APP_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define APP_ADC_DEVICE_CHANNEL_COUNT 2U
#define APP_ADC_LOGICAL_CHANNEL_COUNT 4U

typedef struct
{
  ADC_HandleTypeDef *hadc1;
  ADC_HandleTypeDef *hadc2;
  volatile uint16_t dma_adc1[APP_ADC_DEVICE_CHANNEL_COUNT];
  volatile uint16_t dma_adc2[APP_ADC_DEVICE_CHANNEL_COUNT];
  uint16_t raw_adc[APP_ADC_LOGICAL_CHANNEL_COUNT];
  uint16_t filtered_adc[APP_ADC_LOGICAL_CHANNEL_COUNT];
  uint8_t adc_seeded[APP_ADC_LOGICAL_CHANNEL_COUNT];
} AppAdc_HandleTypeDef;

void AppAdc_Init(
    AppAdc_HandleTypeDef *handle,
    ADC_HandleTypeDef *hadc1,
    ADC_HandleTypeDef *hadc2);

void AppAdc_Task(AppAdc_HandleTypeDef *handle);

uint16_t AppAdc_GetFilteredAdc1(const AppAdc_HandleTypeDef *handle);
uint16_t AppAdc_GetFilteredAdc2(const AppAdc_HandleTypeDef *handle);
uint16_t AppAdc_GetFilteredAdc3(const AppAdc_HandleTypeDef *handle);
uint16_t AppAdc_GetFilteredAdc4(const AppAdc_HandleTypeDef *handle);

#ifdef __cplusplus
}
#endif

#endif /* APP_ADC_H */
