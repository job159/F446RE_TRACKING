#include "App/app_adc.h"

#define APP_ADC_FILTER_WEIGHT_PREV 700U
#define APP_ADC_FILTER_WEIGHT_NEW 300U
#define APP_ADC_FILTER_WEIGHT_SCALE 1000U
#define APP_ADC_FILTER_ROUNDING (APP_ADC_FILTER_WEIGHT_SCALE / 2U)

static uint16_t AppAdc_ApplyLowPass(uint16_t previous, uint16_t sample)
{
  uint32_t weighted_previous;
  uint32_t weighted_sample;
  uint32_t blended_value;

  weighted_previous = (uint32_t)previous * APP_ADC_FILTER_WEIGHT_PREV;
  weighted_sample = (uint32_t)sample * APP_ADC_FILTER_WEIGHT_NEW;
  blended_value = weighted_previous + weighted_sample + APP_ADC_FILTER_ROUNDING;

  return (uint16_t)(blended_value / APP_ADC_FILTER_WEIGHT_SCALE);
}

static void AppAdc_UpdateFilter(
    uint16_t raw_value,
    uint16_t *filtered_value,
    uint8_t *is_seeded)
{
  if (*is_seeded == 0U)
  {
    *filtered_value = raw_value;
    *is_seeded = 1U;
  }
  else
  {
    *filtered_value = AppAdc_ApplyLowPass(*filtered_value, raw_value);
  }
}

static HAL_StatusTypeDef AppAdc_StartDmaChannel(
    ADC_HandleTypeDef *hadc,
    volatile uint16_t *dma_target)
{
  HAL_StatusTypeDef status;

  if ((hadc == NULL) || (dma_target == NULL))
  {
    return HAL_ERROR;
  }

  status = HAL_ADC_Start_DMA(hadc, (uint32_t *)dma_target, 1U);
  if (status != HAL_OK)
  {
    return status;
  }

  if (hadc->DMA_Handle != NULL)
  {
    /* Keep circular DMA running without per-sample IRQ load. */
    __HAL_DMA_DISABLE_IT(hadc->DMA_Handle, DMA_IT_HT);
    __HAL_DMA_DISABLE_IT(hadc->DMA_Handle, DMA_IT_TC);
  }

  return HAL_OK;
}

void AppAdc_Init(
    AppAdc_HandleTypeDef *handle,
    ADC_HandleTypeDef *hadc1,
    ADC_HandleTypeDef *hadc2)
{
  if ((handle == NULL) || (hadc1 == NULL) || (hadc2 == NULL))
  {
    return;
  }

  handle->hadc1 = hadc1;
  handle->hadc2 = hadc2;
  handle->dma_adc1 = 0U;
  handle->dma_adc2 = 0U;
  handle->raw_adc1 = 0U;
  handle->raw_adc2 = 0U;
  handle->filtered_adc1 = 0U;
  handle->filtered_adc2 = 0U;
  handle->adc1_seeded = 0U;
  handle->adc2_seeded = 0U;

  if (AppAdc_StartDmaChannel(handle->hadc1, &handle->dma_adc1) != HAL_OK)
  {
    Error_Handler();
  }

  if (AppAdc_StartDmaChannel(handle->hadc2, &handle->dma_adc2) != HAL_OK)
  {
    Error_Handler();
  }
}

void AppAdc_Task(AppAdc_HandleTypeDef *handle)
{
  if ((handle == NULL) || (handle->hadc1 == NULL) || (handle->hadc2 == NULL))
  {
    return;
  }

  handle->raw_adc1 = handle->dma_adc1;
  handle->raw_adc2 = handle->dma_adc2;

  AppAdc_UpdateFilter(
      handle->raw_adc1,
      &handle->filtered_adc1,
      &handle->adc1_seeded);

  AppAdc_UpdateFilter(
      handle->raw_adc2,
      &handle->filtered_adc2,
      &handle->adc2_seeded);
}

uint16_t AppAdc_GetFilteredAdc1(const AppAdc_HandleTypeDef *handle)
{
  if (handle == NULL)
  {
    return 0U;
  }

  return handle->filtered_adc1;
}

uint16_t AppAdc_GetFilteredAdc2(const AppAdc_HandleTypeDef *handle)
{
  if (handle == NULL)
  {
    return 0U;
  }

  return handle->filtered_adc2;
}
