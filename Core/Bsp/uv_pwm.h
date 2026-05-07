#ifndef UV_PWM_H
#define UV_PWM_H

#include <stdint.h>
#include "stm32g0xx_hal.h"

typedef struct {
  TIM_HandleTypeDef *timer;
  uint32_t channel;
  uint8_t brightness_percent;
  uint8_t started;
} UV_PWM;

HAL_StatusTypeDef UV_PWM_Init(UV_PWM *pwm, TIM_HandleTypeDef *timer, uint32_t channel);
void UV_PWM_SetBrightness(UV_PWM *pwm, uint8_t percent);
void UV_PWM_Enable(UV_PWM *pwm, uint8_t enable);
uint8_t UV_PWM_GetBrightness(const UV_PWM *pwm);

#endif
