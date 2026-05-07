#include "button.h"

/* 初始化一个按键对象：记录 GPIO、有效电平、去抖时间和长按时间。 */
void Button_Init(Button *button,
                 GPIO_TypeDef *port,
                 uint16_t pin,
                 GPIO_PinState active_level,
                 uint32_t debounce_ms,
                 uint32_t long_press_ms,
                 uint32_t now_ms)
{
  GPIO_PinState level = HAL_GPIO_ReadPin(port, pin);

  button->port = port;
  button->pin = pin;
  button->active_level = active_level;
  button->debounce_ms = debounce_ms;
  button->long_press_ms = long_press_ms;
  button->stable_level = level;
  button->last_raw_level = level;
  button->last_change_ms = now_ms;
  button->pressed_since_ms = now_ms;
  button->pressed = (level == active_level);
  button->long_reported = 0U;
}

/*
 * 按键扫描服务：
 * - 先做去抖
 * - 松开时产生短按事件
 * - 按住超过 long_press_ms 产生一次长按事件
 */
ButtonEvent Button_Update(Button *button, uint32_t now_ms)
{
  GPIO_PinState raw_level = HAL_GPIO_ReadPin(button->port, button->pin);
  ButtonEvent event = BUTTON_EVENT_NONE;

  if (raw_level != button->last_raw_level) {
    /* 原始电平变化后重新开始去抖计时。 */
    button->last_raw_level = raw_level;
    button->last_change_ms = now_ms;
  }

  if ((now_ms - button->last_change_ms) < button->debounce_ms) {
    return BUTTON_EVENT_NONE;
  }

  if (raw_level != button->stable_level) {
    button->stable_level = raw_level;

    if (raw_level == button->active_level) {
      /* 稳定进入按下状态，开始长按计时。 */
      button->pressed = 1U;
      button->pressed_since_ms = now_ms;
      button->long_reported = 0U;
    } else {
      if ((button->pressed != 0U) && (button->long_reported == 0U)) {
        /* 未触发长按就松开，判定为短按。 */
        event = BUTTON_EVENT_SHORT_PRESS;
      }
      button->pressed = 0U;
    }
  }

  if ((button->pressed != 0U) &&
      (button->long_reported == 0U) &&
      ((now_ms - button->pressed_since_ms) >= button->long_press_ms)) {
    /* 每次按下只上报一次长按事件。 */
    button->long_reported = 1U;
    event = BUTTON_EVENT_LONG_PRESS;
  }

  return event;
}

/* 查询当前稳定按下状态。 */
uint8_t Button_IsPressed(const Button *button)
{
  return button->pressed;
}
