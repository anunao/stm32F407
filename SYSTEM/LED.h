/**
  ******************************************************************************
  * @file    LED.h
  * @brief   LED 灯驱动头文件
  * @note    本文件只声明 LED 的操作函数，具体代码在 LED.c 中实现。
  *          当前工程中的 LED 为低电平点亮。
  ******************************************************************************
  */

#ifndef __LED_H
#define __LED_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  LED 编号
 */
typedef enum
{
    LED_ID_0 = 0,
    LED_ID_1,
    LED_ID_2,
    LED_ID_3
} LED_ID;

/**
 * @brief  点亮指定的 LED
 * @param  led LED 编号，只能使用 LED_ID_0 到 LED_ID_3
 * @retval 无
 */
void LED_On(LED_ID led);

/**
 * @brief  熄灭指定的 LED
 * @param  led LED 编号，只能使用 LED_ID_0 到 LED_ID_3
 * @retval 无
 */
void LED_Off(LED_ID led);

/**
 * @brief  翻转指定 LED 的状态
 * @param  led LED 编号，只能使用 LED_ID_0 到 LED_ID_3
 * @retval 无
 */
void LED_Toggle(LED_ID led);

#ifdef __cplusplus
}
#endif

#endif /* __LED_H */
