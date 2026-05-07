#include "lcd_status.h"
#include <stdio.h>
#include <string.h>
#include "board_io.h"
#include "main.h"

#define LCD_GPIO_PULSE_DELAY_CYCLES      8U

static uint8_t lcd_initialized;

/* 简单 GPIO 时序延时。后续若 LCD 时序更严格，可替换为定时器延时。 */
static void LCD_GPIO_Delay(void)
{
  volatile uint32_t i;

  for (i = 0U; i < LCD_GPIO_PULSE_DELAY_CYCLES; i++) {
    __NOP();
  }
}

/* LCD 片选控制：当前按常见低有效 CS 处理。 */
static void LCD_GPIO_Select(uint8_t selected)
{
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port,
                    LCD_CS_Pin,
                    selected ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/* 通过 PA3(DATA) + PA2(WR) 输出 1bit 数据。 */
static void LCD_GPIO_WriteBit(uint8_t bit_value)
{
  HAL_GPIO_WritePin(LCD_DATA_GPIO_Port,
                    LCD_DATA_Pin,
                    bit_value ? GPIO_PIN_SET : GPIO_PIN_RESET);
  LCD_GPIO_Delay();
  HAL_GPIO_WritePin(LCD_WR_GPIO_Port, LCD_WR_Pin, GPIO_PIN_RESET);
  LCD_GPIO_Delay();
  HAL_GPIO_WritePin(LCD_WR_GPIO_Port, LCD_WR_Pin, GPIO_PIN_SET);
  LCD_GPIO_Delay();
}

/* 通过 PA4(CS)、PA2(WR)、PA3(DATA) 发送 1 字节。 */
static void LCD_GPIO_WriteByte(uint8_t value)
{
  int8_t bit;

  LCD_GPIO_Select(1U);
  for (bit = 7; bit >= 0; bit--) {
    LCD_GPIO_WriteBit((uint8_t)((value >> bit) & 0x01U));
  }
  LCD_GPIO_Select(0U);
}

/*
 * LCD 文本输出占位层。当前只负责把状态字符串转成 GPIO 字节流；
 * 拿到具体 LCD 驱动芯片协议后，在这里区分命令/数据和显存地址。
 */
static void LCD_GPIO_WriteText(const char *text)
{
  while (*text != '\0') {
    LCD_GPIO_WriteByte((uint8_t)*text);
    text++;
  }
}

/* 应用状态转显示文本。 */
static const char *LCD_Status_StateText(LcdStatusState state)
{
  switch (state) {
    case LCD_STATUS_RUNNING:
      return "RUN";
    case LCD_STATUS_PAUSED:
      return "PAUSE";
    case LCD_STATUS_DOOR_OPEN:
      return "DOOR";
    case LCD_STATUS_FINISHED:
      return "DONE";
    case LCD_STATUS_OFF:
    default:
      return "OFF";
  }
}

/* LCD GPIO 接口初始化：打开背光，释放 CS，WR 默认置高。 */
void LCD_Status_Init(void)
{
  BoardIO_SetLcdBacklight(1U);
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_WR_GPIO_Port, LCD_WR_Pin, GPIO_PIN_SET);
  lcd_initialized = 1U;
}

/* 显示 UV 原始值、剩余时间、亮度、状态和传感器通信状态。 */
void LCD_Status_Show(LcdStatusState state,
                     uint32_t remain_seconds,
                     uint8_t brightness_percent,
                     uint32_t raw_uv,
                     uint8_t sensor_ok)
{
  char line[96];
  int len;

  if (lcd_initialized == 0U) {
    return;
  }

  len = snprintf(line,
                 sizeof(line),
                 "UV:%lu Time:%02lu:%02lu Light:%u%% State:%s Sensor:%s\r\n",
                 (unsigned long)raw_uv,
                 (unsigned long)(remain_seconds / 60U),
                 (unsigned long)(remain_seconds % 60U),
                 brightness_percent,
                 LCD_Status_StateText(state),
                 sensor_ok ? "OK" : "ERR");
  if (len <= 0) {
    return;
  }

  if ((size_t)len > sizeof(line)) {
    len = (int)sizeof(line);
  }

  (void)len;
  LCD_GPIO_WriteText(line);
}
