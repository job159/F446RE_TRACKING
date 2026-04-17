#ifndef APP_STEPPER_TMC2209_H
#define APP_STEPPER_TMC2209_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define TMC_SPEED_STAGE_COUNT   8U
#define TMC_DIR_SPLIT_STAGE     (TMC_SPEED_STAGE_COUNT / 2U)  /* 前4格正轉,後4格反轉 */

typedef struct {
  uint8_t ihold;
  uint8_t irun;
  uint8_t iholddelay;
} StepperTmc2209_CurrentConfig_t;

typedef struct {
  TIM_HandleTypeDef  *htim_step;
  uint32_t            step_channel;
  UART_HandleTypeDef *huart;
  GPIO_TypeDef       *dir_port;
  uint16_t            dir_pin;
  GPIO_TypeDef       *en_port;
  uint16_t            en_pin;
  uint8_t             slave_addr;
  uint16_t            speed_hz[TMC_SPEED_STAGE_COUNT];
  uint8_t             speed_index;
  uint16_t            current_hz;
  GPIO_PinState       current_dir;
  uint16_t            microsteps;
  StepperTmc2209_CurrentConfig_t current_config;
} StepperTmc2209_HandleTypeDef;

HAL_StatusTypeDef StepperTmc2209_Init(
    StepperTmc2209_HandleTypeDef *h,
    TIM_HandleTypeDef *htim_step, uint32_t step_channel,
    UART_HandleTypeDef *huart,
    GPIO_TypeDef *dir_port, uint16_t dir_pin,
    GPIO_TypeDef *en_port,  uint16_t en_pin,
    uint8_t slave_addr,
    const uint16_t speed_table[TMC_SPEED_STAGE_COUNT]);

HAL_StatusTypeDef StepperTmc2209_SetSpeedStage(StepperTmc2209_HandleTypeDef *h, uint8_t stage);
HAL_StatusTypeDef StepperTmc2209_Stop(StepperTmc2209_HandleTypeDef *h);
HAL_StatusTypeDef StepperTmc2209_SetSignedHz(StepperTmc2209_HandleTypeDef *h, int32_t signed_hz);
HAL_StatusTypeDef StepperTmc2209_SetMicrosteps(StepperTmc2209_HandleTypeDef *h, uint16_t microsteps);
HAL_StatusTypeDef StepperTmc2209_SetCurrentConfig(StepperTmc2209_HandleTypeDef *h,
    StepperTmc2209_CurrentConfig_t cfg);
HAL_StatusTypeDef StepperTmc2209_ReadGconf(StepperTmc2209_HandleTypeDef *h, uint32_t *gconf);
HAL_StatusTypeDef StepperTmc2209_ReadIfcnt(StepperTmc2209_HandleTypeDef *h, uint8_t *ifcnt);
HAL_StatusTypeDef StepperTmc2209_ReadIholdIrun(StepperTmc2209_HandleTypeDef *h, uint32_t *ihold_irun);
HAL_StatusTypeDef StepperTmc2209_ReadChopconf(StepperTmc2209_HandleTypeDef *h, uint32_t *chopconf);

uint8_t StepperTmc2209_GetSpeedStage(const StepperTmc2209_HandleTypeDef *h);
uint16_t StepperTmc2209_GetMicrosteps(const StepperTmc2209_HandleTypeDef *h);
StepperTmc2209_CurrentConfig_t StepperTmc2209_GetCurrentConfig(const StepperTmc2209_HandleTypeDef *h);

#ifdef __cplusplus
}
#endif

#endif /* APP_STEPPER_TMC2209_H */
