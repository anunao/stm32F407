/**
  ******************************************************************************
  * @file    UI_Terminal.h
  * @brief   OLED 后台终端界面（调试日志）头文件
  * @note    布局（128x64）：
  *            - 顶部（页 0~1）显示 5 组 16x16 状态图标；
  *            - 下方（页 2~7）显示最近 6 行调试日志（5x7 小字体，每行 21 字符）。
  *
  *          日志与 printf 串口输出同步：main.c 的 fputc() 把每个字符同时
  *          送入串口和本模块。本模块内部不调用 printf，避免递归。
  ******************************************************************************
  */

#ifndef __UI_TERMINAL_H
#define __UI_TERMINAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief  初始化终端界面并显示初始布局
 */
void UI_Terminal_Init(void);

/**
 * @brief  推进终端界面状态机（主循环中反复调用）
 * @note   内部做内容变化检测，图标/日志变化时才重绘对应区域。
 */
void UI_Terminal_Service(void);

/**
 * @brief  接收一个字符（由 main.c 的 fputc 调用，与串口输出同步）
 * @param  c 要显示的字符
 * @note   只把字符写入日志缓冲并标记脏，真正的 OLED 刷新在 Service() 完成。
 *         中文字符（非可打印 ASCII）被忽略，串口仍完整输出。
 */
void UI_Terminal_PutChar(char c);

/**
 * @brief  设置蓝牙连接状态
 * @param  connected 1=已连接，0=未连接
 */
void UI_Terminal_SetBt(uint8_t connected);

/**
 * @brief  设置 Wi-Fi 连接状态
 */
void UI_Terminal_SetWifi(uint8_t connected);

/**
 * @brief  设置巴法云连接状态
 */
void UI_Terminal_SetCloud(uint8_t connected);

/**
 * @brief  设置布防状态
 */
void UI_Terminal_SetArmed(uint8_t armed);

/**
 * @brief  设置光照明暗状态
 */
void UI_Terminal_SetLight(uint8_t bright);

#ifdef __cplusplus
}
#endif

#endif /* __UI_TERMINAL_H */
