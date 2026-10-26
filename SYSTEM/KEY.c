/**
  ******************************************************************************
  * @file    KEY.c
  * @brief   按键驱动程序
  * @note    当前按键使用上拉输入，按下时读取到低电平。
  ******************************************************************************
  */

#include "KEY.h"
#include "main.h"

/**
 * @brief  读取按键当前状态
 * @param  key 按键编号
 * @retval 1 表示按键按下
 * @retval 0 表示按键松开或按键编号错误
 */
uint8_t KEY_IsPressed(KEY_ID key)
{
    GPIO_PinState pin_state;            // 定义按键引脚状态变量(1:按下,0:松开)

    switch (key)
    {
        case KEY_ID_0:
            pin_state = HAL_GPIO_ReadPin(KEY0_GPIO_Port, KEY0_Pin);
            break;

        case KEY_ID_1:
            pin_state = HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin);
            break;

        case KEY_ID_2:
            pin_state = HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin);
            break;

        case KEY_ID_3:

            pin_state = HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin);
            break;

        default:
            return 0;
    }

    /*
     * 按键使用上拉输入：
     * 低电平表示按下，高电平表示松开。
     */
    if (pin_state == GPIO_PIN_RESET)
    {
        return 1;
    }

    return 0;
}
