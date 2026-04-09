#ifndef APP_STEPPER_TMC2209_H
#define APP_STEPPER_TMC2209_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define TMC_SPEED_STAGE_COUNT   8U
#define TMC_DIR_SPLIT_STAGE     (TMC_SPEED_STAGE_COUNT / 2U)  /* 前4格正轉,後4格反轉 */

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

uint8_t StepperTmc2209_GetSpeedStage(const StepperTmc2209_HandleTypeDef *h);

#ifdef __cplusplus
}
#endif

#endif /* APP_STEPPER_TMC2209_H */
