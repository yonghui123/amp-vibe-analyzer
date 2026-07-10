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
#include "stm32f4xx_hal.h"

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

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LCD_RESET_Pin GPIO_PIN_3
#define LCD_RESET_GPIO_Port GPIOC
#define TP_INT_Pin GPIO_PIN_4
#define TP_INT_GPIO_Port GPIOC
#define TP_NCS_Pin GPIO_PIN_5
#define TP_NCS_GPIO_Port GPIOC
#define LCD_PWM_NOUSE_Pin GPIO_PIN_0
#define LCD_PWM_NOUSE_GPIO_Port GPIOB
#define AD7606_RESET_Pin GPIO_PIN_1
#define AD7606_RESET_GPIO_Port GPIOB
#define AD7606_OS2_Pin GPIO_PIN_10
#define AD7606_OS2_GPIO_Port GPIOB
#define AD7606_RAGE_Pin GPIO_PIN_11
#define AD7606_RAGE_GPIO_Port GPIOB
#define AD7606_BUSY_Pin GPIO_PIN_14
#define AD7606_BUSY_GPIO_Port GPIOB
#define AD7606_BUSY_EXTI_IRQn EXTI15_10_IRQn
#define AD7606_OS1_Pin GPIO_PIN_8
#define AD7606_OS1_GPIO_Port GPIOA
#define LCD_BUSY_Pin GPIO_PIN_3
#define LCD_BUSY_GPIO_Port GPIOD
#define AD7606_OS0_Pin GPIO_PIN_6
#define AD7606_OS0_GPIO_Port GPIOD
#define SPI3_SCK_Pin GPIO_PIN_3
#define SPI3_SCK_GPIO_Port GPIOB
#define SPI3_MISO_Pin GPIO_PIN_4
#define SPI3_MISO_GPIO_Port GPIOB
#define SPI3_MOSI_Pin GPIO_PIN_5
#define SPI3_MOSI_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
