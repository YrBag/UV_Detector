#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>
#include "stm32g0xx_hal.h"

typedef struct {
  GPIO_TypeDef *port;
  uint16_t pin;
  GPIO_PinState active_level;
  uint32_t debounce_ms;
  uint32_t long_press_ms;
  GPIO_PinState stable_level;
  GPIO_PinState last_raw_level;
  uint32_t last_change_ms;
  uint32_t pressed_since_ms;
  uint8_t pressed;
  uint8_t long_reported;
} Button;

typedef enum {
  BUTTON_EVENT_NONE = 0,
  BUTTON_EVENT_SHORT_PRESS,
  BUTTON_EVENT_LONG_PRESS
} ButtonEvent;

void Button_Init(Button *button,
                 GPIO_TypeDef *port,
                 uint16_t pin,
                 GPIO_PinState active_level,
                 uint32_t debounce_ms,
                 uint32_t long_press_ms,
                 uint32_t now_ms);
ButtonEvent Button_Update(Button *button, uint32_t now_ms);
uint8_t Button_IsPressed(const Button *button);

#endif
