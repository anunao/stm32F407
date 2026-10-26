/**
  ******************************************************************************
  * @file    BEEP.h
  * @brief   蜂鸣器驱动头文件
  * @note    当前蜂鸣器连接在 PF8。
  *          输出高电平时蜂鸣器响，输出低电平时蜂鸣器关闭。
  ******************************************************************************
  */

#ifndef __BEEP_H
#define __BEEP_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  打开蜂鸣器
 * @param  无
 * @retval 无
 */
void BEEP_On(void);

/**
 * @brief  关闭蜂鸣器
 * @param  无
 * @retval 无
 */
void BEEP_Off(void);

/**
 * @brief  翻转蜂鸣器当前状态
 * @param  无
 * @retval 无
 */
void BEEP_Toggle(void);

#ifdef __cplusplus
}
#endif

#endif /* __BEEP_H */
