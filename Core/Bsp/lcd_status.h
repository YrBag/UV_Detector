#ifndef LCD_STATUS_H
#define LCD_STATUS_H

#include <stdint.h>

typedef enum {
  LCD_STATUS_OFF = 0,
  LCD_STATUS_RUNNING,
  LCD_STATUS_PAUSED,
  LCD_STATUS_DOOR_OPEN,
  LCD_STATUS_FINISHED
} LcdStatusState;

void LCD_Status_Init(void);
void LCD_Status_Show(LcdStatusState state,
                     uint32_t remain_seconds,
                     uint8_t brightness_percent,
                     uint32_t raw_uv,
                     uint8_t sensor_ok);

#endif
