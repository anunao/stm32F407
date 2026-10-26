/**
  ******************************************************************************
  * @file    LED.c
  * @brief   LED 灯驱动程序
  * @note    当前开发板的 LED 为低电平点亮，高电平熄灭。
  ******************************************************************************
  */

#include "LED.h"
#include "main.h"

/**
 * @brief  点亮指定的 LED
 * @param  led LED 编号，只能使用 LED_ID_0 到 LED_ID_3
 * @retval 无
 */
void LED_On(LED_ID led)
{
    switch (led)
    {
        case LED_ID_0:
            HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_RESET);
            break;

        case LED_ID_1:
            HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
            break;

        case LED_ID_2:
            HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
            break;

        case LED_ID_3:
            HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);
            break;

        default:
            /* LED 编号错误时不进行任何操作 */
            break;
    }
}

/**
 * @brief  熄灭指定的 LED
 * @param  led LED 编号，只能使用 LED_ID_0 到 LED_ID_3
 * @retval 无
 */
void LED_Off(LED_ID led)
{
    switch (led)
    {
        case LED_ID_0:
            HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_SET);
            break;

        case LED_ID_1:
            HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
            break;

        case LED_ID_2:
            HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
            break;

        case LED_ID_3:
            HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
            break;

        default:
            /* LED 编号错误时不进行任何操作 */
            break;
    }
}

/**
 * @brief  翻转指定 LED 的状态
 * @param  led LED 编号，只能使用 LED_ID_0 到 LED_ID_3
 * @retval 无
 */
void LED_Toggle(LED_ID led)
{
    switch (led)
    {
        case LED_ID_0:
            HAL_GPIO_TogglePin(LED0_GPIO_Port, LED0_Pin);
            break;

        case LED_ID_1:
            HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
            break;

        case LED_ID_2:
            HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin);
            break;

        case LED_ID_3:
            HAL_GPIO_TogglePin(LED3_GPIO_Port, LED3_Pin);
            break;

        default:
            /* LED 编号错误时不进行任何操作 */
            break;
    }
}
