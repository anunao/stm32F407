/**
  ******************************************************************************
  * @file    KEY.h
  * @brief   按键驱动头文件
  * @note    当前工程使用 KEY0、KEY1、KEY2 和 KEY3。
  *          按键使用上拉输入，按下时读取到低电平。
  ******************************************************************************
  */

#ifndef __KEY_H
#define __KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief  按键编号
 */
typedef enum
{
    KEY_ID_0 = 0,
    KEY_ID_1,
    KEY_ID_2,
    KEY_ID_3
} KEY_ID;

/**
 * @brief  读取按键当前状态
 * @param  key 按键编号，只能使用 KEY_ID_0、KEY_ID_1、KEY_ID_2 或 KEY_ID_3
 * @retval 1 表示按键按下
 * @retval 0 表示按键松开
 */
uint8_t KEY_IsPressed(KEY_ID key);

#ifdef __cplusplus
}
#endif

#endif /* __KEY_H */
