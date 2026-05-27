#include "ltr390.h"

#define LTR390_I2C_ADDR_7BIT            0x53U
#define LTR390_I2C_ADDR                 (LTR390_I2C_ADDR_7BIT << 1U)
#define LTR390_REG_MAIN_CTRL            0x00U
#define LTR390_REG_MEAS_RATE            0x04U
#define LTR390_REG_GAIN                 0x05U
#define LTR390_REG_PART_ID              0x06U
#define LTR390_REG_MAIN_STATUS          0x07U
#define LTR390_REG_INT_CFG              0x19U
#define LTR390_REG_THRESH_UP_0          0x21U
#define LTR390_REG_THRESH_LOW_0         0x24U
#define LTR390_REG_UVS_DATA_0           0x10U
#define LTR390_PART_ID_EXPECTED         0xB2U
#define LTR390_MEAS_RATE_18BIT_100MS    0x22U
#define LTR390_GAIN_18                  0x04U
#define LTR390_INT_CFG_UVS              0x34U
#define LTR390_MAIN_CTRL_UVS_ACTIVE     0x0AU
#define LTR390_STATUS_DATA_READY        0x08U
#define LTR390_I2C_TIMEOUT_MS           50U
#define LTR390_UVI_SCALE_NUMERATOR      2U
#define LTR390_UVI_SCALE_DENOMINATOR    23U
#define LTR390_UW_CM2_X100_NUMERATOR    175U
#define LTR390_UW_CM2_X100_DENOMINATOR  4U

static HAL_StatusTypeDef LTR390_Fail(LTR390 *sensor, LTR390_Error error)
{
  sensor->last_error = error;
  if ((error != LTR390_ERROR_READ_STATUS) &&
      (error != LTR390_ERROR_READ_UVS_DATA) &&
      (error != LTR390_ERROR_WRITE_INT_CFG) &&
      (error != LTR390_ERROR_WRITE_MAIN_CTRL)) {
    sensor->present = 0U;
  }
  return HAL_ERROR;
}

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
  sensor->last_error = LTR390_ERROR_NONE;
  sensor->last_part_id = 0U;
  sensor->present = 0U;
  sensor->last_raw_uv = 0U;

  if (HAL_I2C_IsDeviceReady(i2c, LTR390_I2C_ADDR, 2U, LTR390_I2C_TIMEOUT_MS) != HAL_OK) {
    return LTR390_Fail(sensor, LTR390_ERROR_NOT_READY);
  }

  if (LTR390_ReadReg(sensor, LTR390_REG_PART_ID, &part_id) != HAL_OK) {
    return LTR390_Fail(sensor, LTR390_ERROR_READ_PART_ID);
  }
  sensor->last_part_id = part_id;

  if (part_id != LTR390_PART_ID_EXPECTED) {
    return LTR390_Fail(sensor, LTR390_ERROR_BAD_PART_ID);
  }

  if (LTR390_WriteReg(sensor, LTR390_REG_MEAS_RATE, LTR390_MEAS_RATE_18BIT_100MS) != HAL_OK) {
    return LTR390_Fail(sensor, LTR390_ERROR_WRITE_MEAS_RATE);
  }

  if (LTR390_WriteReg(sensor, LTR390_REG_GAIN, LTR390_GAIN_18) != HAL_OK) {
    return LTR390_Fail(sensor, LTR390_ERROR_WRITE_GAIN);
  }

  if (LTR390_WriteReg(sensor, LTR390_REG_INT_CFG, LTR390_INT_CFG_UVS) != HAL_OK) {
    return LTR390_Fail(sensor, LTR390_ERROR_WRITE_INT_CFG);
  }

  if (LTR390_WriteReg(sensor, LTR390_REG_MAIN_CTRL, LTR390_MAIN_CTRL_UVS_ACTIVE) != HAL_OK) {
    return LTR390_Fail(sensor, LTR390_ERROR_WRITE_MAIN_CTRL);
  }

  sensor->last_error = LTR390_ERROR_NONE;
  sensor->present = 1U;
  return HAL_OK;
}

/* 读取 20bit UVS 原始数据；若新数据未准备好，返回上一次有效值。 */
HAL_StatusTypeDef LTR390_ReadRawUv(LTR390 *sensor, uint32_t *raw_uv)
{
  uint8_t status = 0U;
  uint8_t data[3] = {0U};

  if (sensor->present == 0U) {
    return LTR390_Fail(sensor, LTR390_ERROR_NOT_PRESENT);
  }

  if (LTR390_WriteReg(sensor, LTR390_REG_INT_CFG, LTR390_INT_CFG_UVS) != HAL_OK) {
    return LTR390_Fail(sensor, LTR390_ERROR_WRITE_INT_CFG);
  }

  if (LTR390_WriteReg(sensor, LTR390_REG_MAIN_CTRL, LTR390_MAIN_CTRL_UVS_ACTIVE) != HAL_OK) {
    return LTR390_Fail(sensor, LTR390_ERROR_WRITE_MAIN_CTRL);
  }

  if (LTR390_ReadReg(sensor, LTR390_REG_MAIN_STATUS, &status) != HAL_OK) {
    return LTR390_Fail(sensor, LTR390_ERROR_READ_STATUS);
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
    return LTR390_Fail(sensor, LTR390_ERROR_READ_UVS_DATA);
  }

  sensor->last_raw_uv = ((uint32_t)data[2] << 16U) |
                        ((uint32_t)data[1] << 8U) |
                        (uint32_t)data[0];
  *raw_uv = sensor->last_raw_uv;
  sensor->last_error = LTR390_ERROR_NONE;
  return HAL_OK;
}

HAL_StatusTypeDef LTR390_SetThreshold(LTR390 *sensor, uint32_t low, uint32_t high)
{
  if (sensor->present == 0U) {
    return LTR390_Fail(sensor, LTR390_ERROR_NOT_PRESENT);
  }

  if (LTR390_WriteReg(sensor, LTR390_REG_THRESH_UP_0, (uint8_t)(high & 0xFFU)) != HAL_OK) {
    return LTR390_Fail(sensor, LTR390_ERROR_WRITE_THRESHOLD);
  }
  if (LTR390_WriteReg(sensor, LTR390_REG_THRESH_UP_0 + 1U, (uint8_t)((high >> 8U) & 0xFFU)) != HAL_OK) {
    return LTR390_Fail(sensor, LTR390_ERROR_WRITE_THRESHOLD);
  }
  if (LTR390_WriteReg(sensor, LTR390_REG_THRESH_UP_0 + 2U, (uint8_t)((high >> 16U) & 0x0FU)) != HAL_OK) {
    return LTR390_Fail(sensor, LTR390_ERROR_WRITE_THRESHOLD);
  }
  if (LTR390_WriteReg(sensor, LTR390_REG_THRESH_LOW_0, (uint8_t)(low & 0xFFU)) != HAL_OK) {
    return LTR390_Fail(sensor, LTR390_ERROR_WRITE_THRESHOLD);
  }
  if (LTR390_WriteReg(sensor, LTR390_REG_THRESH_LOW_0 + 1U, (uint8_t)((low >> 8U) & 0xFFU)) != HAL_OK) {
    return LTR390_Fail(sensor, LTR390_ERROR_WRITE_THRESHOLD);
  }
  if (LTR390_WriteReg(sensor, LTR390_REG_THRESH_LOW_0 + 2U, (uint8_t)((low >> 16U) & 0x0FU)) != HAL_OK) {
    return LTR390_Fail(sensor, LTR390_ERROR_WRITE_THRESHOLD);
  }

  sensor->last_error = LTR390_ERROR_NONE;
  return HAL_OK;
}

/* 传感器在线状态，用于 LCD 显示或后续安全策略。 */
uint32_t LTR390_RawUvToUviX100(uint32_t raw_uv)
{
  return (uint32_t)(((uint64_t)raw_uv * LTR390_UVI_SCALE_NUMERATOR +
                     (LTR390_UVI_SCALE_DENOMINATOR / 2U)) /
                    LTR390_UVI_SCALE_DENOMINATOR);
}

uint32_t LTR390_UviX100ToUwCm2X100(uint32_t uvi_x100)
{
  return (uint32_t)(((uint64_t)uvi_x100 * 5U + 1U) / 2U);
}

uint32_t LTR390_UviX100ToWM2X1000(uint32_t uvi_x100)
{
  return (uvi_x100 + 2U) / 4U;
}

uint32_t LTR390_UviX100ToMwCm2X1000000(uint32_t uvi_x100)
{
  return (uint32_t)((uint64_t)uvi_x100 * 25U);
}

uint32_t LTR390_RawUvToUwCm2X100(uint32_t raw_uv)
{
  return (uint32_t)(((uint64_t)raw_uv * LTR390_UW_CM2_X100_NUMERATOR +
                     (LTR390_UW_CM2_X100_DENOMINATOR / 2U)) /
                    LTR390_UW_CM2_X100_DENOMINATOR);
}

uint32_t LTR390_UwCm2X100ToWM2X1000(uint32_t uw_cm2_x100)
{
  return (uw_cm2_x100 + 5U) / 10U;
}

uint32_t LTR390_UwCm2X100ToMwCm2X1000000(uint32_t uw_cm2_x100)
{
  return (uint32_t)((uint64_t)uw_cm2_x100 * 10U);
}

uint8_t LTR390_IsPresent(const LTR390 *sensor)
{
  return sensor->present;
}

const char *LTR390_GetErrorString(LTR390_Error error)
{
  switch (error) {
    case LTR390_ERROR_NONE:
      return "none";
    case LTR390_ERROR_NOT_READY:
      return "no ack at 0x53";
    case LTR390_ERROR_READ_PART_ID:
      return "read PART_ID failed";
    case LTR390_ERROR_BAD_PART_ID:
      return "bad PART_ID";
    case LTR390_ERROR_WRITE_MEAS_RATE:
      return "write MEAS_RATE failed";
    case LTR390_ERROR_WRITE_GAIN:
      return "write GAIN failed";
    case LTR390_ERROR_WRITE_INT_CFG:
      return "write INT_CFG failed";
    case LTR390_ERROR_WRITE_MAIN_CTRL:
      return "write MAIN_CTRL failed";
    case LTR390_ERROR_NOT_PRESENT:
      return "sensor not initialized";
    case LTR390_ERROR_READ_STATUS:
      return "read MAIN_STATUS failed";
    case LTR390_ERROR_READ_UVS_DATA:
      return "read UVS data failed";
    case LTR390_ERROR_WRITE_THRESHOLD:
      return "write threshold failed";
    default:
      return "unknown";
  }
}
