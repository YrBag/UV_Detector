#include "ltr390.h"

#define LTR390_I2C_ADDR_7BIT            0x53U
#define LTR390_I2C_ADDR                 (LTR390_I2C_ADDR_7BIT << 1U)
#define LTR390_REG_MAIN_CTRL            0x00U
#define LTR390_REG_MEAS_RATE            0x04U
#define LTR390_REG_GAIN                 0x05U
#define LTR390_REG_PART_ID              0x06U
#define LTR390_REG_MAIN_STATUS          0x07U
#define LTR390_REG_UVS_DATA_0           0x10U
#define LTR390_MAIN_CTRL_UVS_ENABLE     0x03U
#define LTR390_STATUS_DATA_READY        0x08U
#define LTR390_I2C_TIMEOUT_MS           50U

/* LTR390 单寄存器写入封装，应用层不直接接触 I2C 寄存器地址。 */
static HAL_StatusTypeDef LTR390_WriteReg(LTR390 *sensor, uint8_t reg, uint8_t value)
{
  return HAL_I2C_Mem_Write(sensor->i2c,
                           LTR390_I2C_ADDR,
                           reg,
                           I2C_MEMADD_SIZE_8BIT,
                           &value,
                           1U,
                           LTR390_I2C_TIMEOUT_MS);
}

/* LTR390 单寄存器读取封装。 */
static HAL_StatusTypeDef LTR390_ReadReg(LTR390 *sensor, uint8_t reg, uint8_t *value)
{
  return HAL_I2C_Mem_Read(sensor->i2c,
                          LTR390_I2C_ADDR,
                          reg,
                          I2C_MEMADD_SIZE_8BIT,
                          value,
                          1U,
                          LTR390_I2C_TIMEOUT_MS);
}

/* 初始化 LTR390：确认设备在线，配置采样率、增益，并进入 UVS 测量模式。 */
HAL_StatusTypeDef LTR390_Init(LTR390 *sensor, I2C_HandleTypeDef *i2c)
{
  uint8_t part_id = 0U;

  sensor->i2c = i2c;
  sensor->present = 0U;
  sensor->last_raw_uv = 0U;

  if (HAL_I2C_IsDeviceReady(i2c, LTR390_I2C_ADDR, 2U, LTR390_I2C_TIMEOUT_MS) != HAL_OK) {
    return HAL_ERROR;
  }

  if (LTR390_ReadReg(sensor, LTR390_REG_PART_ID, &part_id) != HAL_OK) {
    return HAL_ERROR;
  }

  if (LTR390_WriteReg(sensor, LTR390_REG_MEAS_RATE, 0x22U) != HAL_OK) {
    return HAL_ERROR;
  }

  if (LTR390_WriteReg(sensor, LTR390_REG_GAIN, 0x02U) != HAL_OK) {
    return HAL_ERROR;
  }

  if (LTR390_WriteReg(sensor, LTR390_REG_MAIN_CTRL, LTR390_MAIN_CTRL_UVS_ENABLE) != HAL_OK) {
    return HAL_ERROR;
  }

  sensor->present = 1U;
  (void)part_id;
  return HAL_OK;
}

/* 读取 20bit UVS 原始数据；若新数据未准备好，返回上一次有效值。 */
HAL_StatusTypeDef LTR390_ReadRawUv(LTR390 *sensor, uint32_t *raw_uv)
{
  uint8_t status = 0U;
  uint8_t data[3] = {0U};

  if (sensor->present == 0U) {
    return HAL_ERROR;
  }

  if (LTR390_ReadReg(sensor, LTR390_REG_MAIN_STATUS, &status) != HAL_OK) {
    sensor->present = 0U;
    return HAL_ERROR;
  }

  if ((status & LTR390_STATUS_DATA_READY) == 0U) {
    /* 传感器尚未更新数据时，不阻塞等待，保持主循环响应。 */
    *raw_uv = sensor->last_raw_uv;
    return HAL_OK;
  }

  if (HAL_I2C_Mem_Read(sensor->i2c,
                       LTR390_I2C_ADDR,
                       LTR390_REG_UVS_DATA_0,
                       I2C_MEMADD_SIZE_8BIT,
                       data,
                       sizeof(data),
                       LTR390_I2C_TIMEOUT_MS) != HAL_OK) {
    sensor->present = 0U;
    return HAL_ERROR;
  }

  sensor->last_raw_uv = ((uint32_t)data[2] << 16U) |
                        ((uint32_t)data[1] << 8U) |
                        (uint32_t)data[0];
  *raw_uv = sensor->last_raw_uv;
  return HAL_OK;
}

/* 传感器在线状态，用于 LCD 显示或后续安全策略。 */
uint8_t LTR390_IsPresent(const LTR390 *sensor)
{
  return sensor->present;
}
