#ifndef BOARD_IO_H
#define BOARD_IO_H

#include <stdint.h>
#include "stm32g0xx_hal.h"

void BoardIO_SetFan(uint8_t on);
void BoardIO_SetLcdBacklight(uint8_t on);
uint8_t BoardIO_IsDoorOpen(void);
uint8_t BoardIO_IsFunctionSwitchOn(void);

#endif
