#ifndef LTR390_H
#define LTR390_H

#include <stdint.h>
#include "stm32g0xx_hal.h"

typedef enum {
  LTR390_ERROR_NONE = 0,
  LTR390_ERROR_NOT_READY,
  LTR390_ERROR_READ_PART_ID,
  LTR390_ERROR_BAD_PART_ID,
  LTR390_ERROR_WRITE_MEAS_RATE,
  LTR390_ERROR_WRITE_GAIN,
  LTR390_ERROR_WRITE_INT_CFG,
  LTR390_ERROR_WRITE_MAIN_CTRL,
  LTR390_ERROR_NOT_PRESENT,
  LTR390_ERROR_READ_STATUS,
  LTR390_ERROR_READ_UVS_DATA,
  LTR390_ERROR_WRITE_THRESHOLD
} LTR390_Error;

typedef struct {
  I2C_HandleTypeDef *i2c;
  LTR390_Error last_error;
  uint8_t last_part_id;
  uint8_t present;
  uint32_t last_raw_uv;
} LTR390;

HAL_StatusTypeDef LTR390_Init(LTR390 *sensor, I2C_HandleTypeDef *i2c);
HAL_StatusTypeDef LTR390_ReadRawUv(LTR390 *sensor, uint32_t *raw_uv);
HAL_StatusTypeDef LTR390_SetThreshold(LTR390 *sensor, uint32_t low, uint32_t high);
uint8_t LTR390_IsPresent(const LTR390 *sensor);
const char *LTR390_GetErrorString(LTR390_Error error);

#endif
