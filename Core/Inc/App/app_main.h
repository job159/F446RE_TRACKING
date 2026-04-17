#ifndef APP_MAIN_H
#define APP_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

void AppMain_Init(ADC_HandleTypeDef  *hadc1,
                  ADC_HandleTypeDef  *hadc2,
                  UART_HandleTypeDef *huart_log,
                  TIM_HandleTypeDef  *htim_step1,
                  TIM_HandleTypeDef  *htim_step2,
                  UART_HandleTypeDef *huart_tmc1,
                  UART_HandleTypeDef *huart_tmc2);

void AppMain_Task(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_MAIN_H */
