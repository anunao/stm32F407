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
#define KEY1_Pin GPIO_PIN_2
#define KEY1_GPIO_Port GPIOE
#define KEY1_EXTI_IRQn EXTI2_IRQn
#define KEY2_Pin GPIO_PIN_3
#define KEY2_GPIO_Port GPIOE
#define KEY2_EXTI_IRQn EXTI3_IRQn
#define KEY3_Pin GPIO_PIN_4
#define KEY3_GPIO_Port GPIOE
#define KEY3_EXTI_IRQn EXTI4_IRQn
#define R_ADC_Pin GPIO_PIN_7
#define R_ADC_GPIO_Port GPIOF
#define BEEP_Pin GPIO_PIN_8
#define BEEP_GPIO_Port GPIOF
#define LED0_Pin GPIO_PIN_9
#define LED0_GPIO_Port GPIOF
#define LED1_Pin GPIO_PIN_10
#define LED1_GPIO_Port GPIOF
#define KEY0_Pin GPIO_PIN_0
#define KEY0_GPIO_Port GPIOA
#define KEY0_EXTI_IRQn EXTI0_IRQn
#define HC05_TX_Pin GPIO_PIN_2
#define HC05_TX_GPIO_Port GPIOA
#define HC05_RX_Pin GPIO_PIN_3
#define HC05_RX_GPIO_Port GPIOA
#define Light_Senser_ADC_Pin GPIO_PIN_5
#define Light_Senser_ADC_GPIO_Port GPIOA
#define Servo_PWM_Pin GPIO_PIN_9
#define Servo_PWM_GPIO_Port GPIOE
#define LED2_Pin GPIO_PIN_13
#define LED2_GPIO_Port GPIOE
#define LED3_Pin GPIO_PIN_14
#define LED3_GPIO_Port GPIOE
#define ESP01S_TX_Pin GPIO_PIN_10
#define ESP01S_TX_GPIO_Port GPIOB
#define ESP01s_RX_Pin GPIO_PIN_11
#define ESP01s_RX_GPIO_Port GPIOB
#define IIC_SOFT_SCL_Pin GPIO_PIN_6
#define IIC_SOFT_SCL_GPIO_Port GPIOC
#define IIC_SOFT_SDA_Pin GPIO_PIN_7
#define IIC_SOFT_SDA_GPIO_Port GPIOC
#define HC05_STATE_Pin GPIO_PIN_8
#define HC05_STATE_GPIO_Port GPIOC
#define CH340_TX_Pin GPIO_PIN_9
#define CH340_TX_GPIO_Port GPIOA
#define CH340_RX_Pin GPIO_PIN_10
#define CH340_RX_GPIO_Port GPIOA
#define I2C1_SCL_At24_Pin GPIO_PIN_8
#define I2C1_SCL_At24_GPIO_Port GPIOB
#define I2C1_SDA_At24_Pin GPIO_PIN_9
#define I2C1_SDA_At24_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
