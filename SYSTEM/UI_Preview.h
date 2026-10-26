/**
  ******************************************************************************
  * @file    UI_Preview.h
  * @brief   OLED 前台界面（预览模式）头文件
  * @note    前台模式布局（128x64）：
  *            - 右半边（列 64~127）显示 64x64 小猫表情；
  *            - 左半边（列 0~47）显示 5 组 16x16 状态图标：
  *               蓝牙(0,0)  WiFi(16,0)  云端(32,0)
  *               布防(0,2)  光照(16,2)
  *
  *          表情优先级：报警 > 布防警觉 > 手动表情 > 睡眠。
  *          手动表情（FACE:xxx）仅撤防时有效，5 秒后自动恢复睡眠。
  *          蓝牙采用软件活动判定：收到手机数据即视为已连接，60 秒
  *          无数据自动视为未连接。
  ******************************************************************************
  */

#ifndef __UI_PREVIEW_H
#define __UI_PREVIEW_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief  小猫表情类型
 */
typedef enum
{
    UI_FACE_SLEEP = 0,   /* 睡觉 */
    UI_FACE_ALERT,       /* 警觉 */
    UI_FACE_HAPPY,       /* 开心 */
    UI_FACE_SCARED       /* 惊吓 */
} UI_Face;

/**
 * @brief  初始化前台界面并显示初始布局
 * @param  无
 * @retval 无
 */
void UI_Preview_Init(void);

/**
 * @brief  推进前台界面状态机（主循环中反复调用）
 * @param  无
 * @retval 无
 * @note   内部做内容变化检测，只有状态真正变化时才重绘对应区域，
 *         避免软件 I2C 刷屏阻塞串口通信。
 */
void UI_Preview_Service(void);

/**
 * @brief  设置蓝牙连接状态
 * @param  connected 1=已连接，0=未连接
 * @note   状态来自 HC05 的 STATE 引脚（连接时高电平），由主循环读取后同步。
 */
void UI_Preview_SetBt(uint8_t connected);

/**
 * @brief  设置 Wi-Fi 连接状态
 * @param  connected 1=已连接，0=未连接
 */
void UI_Preview_SetWifi(uint8_t connected);

/**
 * @brief  设置巴法云连接状态
 * @param  connected 1=已连接，0=未连接
 */
void UI_Preview_SetCloud(uint8_t connected);

/**
 * @brief  设置布防状态
 * @param  armed 1=已布防，0=已撤防
 */
void UI_Preview_SetArmed(uint8_t armed);

/**
 * @brief  设置报警状态
 * @param  alarm 1=报警中，0=无报警
 */
void UI_Preview_SetAlarm(uint8_t alarm);

/**
 * @brief  设置光照明暗状态
 * @param  bright 1=亮，0=暗
 */
void UI_Preview_SetLight(uint8_t bright);

/**
 * @brief  设置手动表情（仅撤防时生效）
 * @param  face 目标表情
 * @retval 无
 * @note   手动表情显示 5 秒后自动恢复睡眠表情。
 */
void UI_Preview_SetManualFace(UI_Face face);

#ifdef __cplusplus
}
#endif

#endif /* __UI_PREVIEW_H */
