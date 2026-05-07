#ifndef LTR390_H
#define LTR390_H

#include <stdint.h>
#include "stm32g0xx_hal.h"

typedef struct {
  I2C_HandleTypeDef *i2c;
  uint8_t present;
  uint32_t last_raw_uv;
} LTR390;

HAL_StatusTypeDef LTR390_Init(LTR390 *sensor, I2C_HandleTypeDef *i2c);
HAL_StatusTypeDef LTR390_ReadRawUv(LTR390 *sensor, uint32_t *raw_uv);
uint8_t LTR390_IsPresent(const LTR390 *sensor);

#endif
