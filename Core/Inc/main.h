/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define FAN_Pin GPIO_PIN_1
#define FAN_GPIO_Port GPIOA
#define LCD_CS_Pin GPIO_PIN_2
#define LCD_CS_GPIO_Port GPIOA
#define LCD_WR_Pin GPIO_PIN_3
#define LCD_WR_GPIO_Port GPIOA
#define LCD_DATA_Pin GPIO_PIN_4
#define LCD_DATA_GPIO_Port GPIOA
#define TIME_ADD_Pin GPIO_PIN_10
#define TIME_ADD_GPIO_Port GPIOB
#define FUNCTION_SWITCH_Pin GPIO_PIN_11
#define FUNCTION_SWITCH_GPIO_Port GPIOB
#define LCD_BACKLIGHT_Pin GPIO_PIN_9
#define LCD_BACKLIGHT_GPIO_Port GPIOA
#define UV_INPUT_Pin GPIO_PIN_10
#define UV_INPUT_GPIO_Port GPIOA
#define LIGHT_SWITCH_Pin GPIO_PIN_0
#define LIGHT_SWITCH_GPIO_Port GPIOD
#define TIME_REDUCE_Pin GPIO_PIN_2
#define TIME_REDUCE_GPIO_Port GPIOD
#define DOOR_SECURITY_Pin GPIO_PIN_3
#define DOOR_SECURITY_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
