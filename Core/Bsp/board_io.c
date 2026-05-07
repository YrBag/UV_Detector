#include "board_io.h"
#include "app_config.h"
#include "main.h"

void BoardIO_SetFan(uint8_t on)
{
  HAL_GPIO_WritePin(FAN_GPIO_Port, FAN_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void BoardIO_SetLcdBacklight(uint8_t on)
{
  HAL_GPIO_WritePin(LCD_BACKLIGHT_GPIO_Port,
                    LCD_BACKLIGHT_Pin,
                    on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

uint8_t BoardIO_IsDoorOpen(void)
{
  return (HAL_GPIO_ReadPin(DOOR_SECURITY_GPIO_Port, DOOR_SECURITY_Pin) ==
          APP_DOOR_OPEN_LEVEL);
}

uint8_t BoardIO_IsFunctionSwitchOn(void)
{
  return (HAL_GPIO_ReadPin(FUNCTION_SWITCH_GPIO_Port, FUNCTION_SWITCH_Pin) ==
          APP_FUNCTION_SWITCH_ON_LEVEL);
}
