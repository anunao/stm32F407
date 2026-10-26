/**
  ******************************************************************************
  * @file    BEEP.c
  * @brief   蜂鸣器驱动程序
  * @note    当前蜂鸣器连接在 PF8。
  *          PF8 输出高电平时蜂鸣器响，输出低电平时蜂鸣器关闭。
  ******************************************************************************
  */

#include "BEEP.h"
#include "main.h"

/**
 * @brief  打开蜂鸣器
 * @param  无
 * @retval 无
 */
void BEEP_On(void)
{
    HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_SET);
}

/**
 * @brief  关闭蜂鸣器
 * @param  无
 * @retval 无
 */
void BEEP_Off(void)
{
    HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);
}

/**
 * @brief  翻转蜂鸣器当前状态
 * @param  无
 * @retval 无
 */
void BEEP_Toggle(void)
{
    HAL_GPIO_TogglePin(BEEP_GPIO_Port, BEEP_Pin);
}
