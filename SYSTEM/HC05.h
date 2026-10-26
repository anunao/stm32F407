/**
  ******************************************************************************
  * @file    HC05.h
  * @brief   HC-05 蓝牙串口驱动头文件
  * @note    HC-05 使用 USART2，串口参数由 CubeMX 配置为：
  *          波特率 9600，数据位 8 位，无校验，停止位 1 位。
  *
  *          本驱动按一行数据接收内容，建议手机发送时以回车换行结束。
  ******************************************************************************
  */

#ifndef __HC05_H
#define __HC05_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief  HC-05 接收一行数据时允许的最大长度
 */
#define HC05_LINE_MAX_LENGTH    64

/**
 * @brief  启动 HC-05 串口接收
 * @param  无
 * @retval 无
 * @note   需要在 main.c 初始化 USART2 后调用一次。
 */
void HC05_StartReceive(void);

/**
 * @brief  处理 HC-05 单字节接收完成事件
 * @param  无
 * @retval 无
 * @note   由统一的 HAL_UART_RxCpltCallback() 调用，只处理 USART2 数据。
 */
void HC05_RxCpltCallback(void);

/**
 * @brief  向 HC-05 发送字符串
 * @param  text 要发送的字符串，必须以 '\0' 结尾
 * @retval 无
 */
void HC05_SendString(const char *text);

/**
 * @brief  读取 HC-05 接收到的一整行数据
 * @param  line 用于保存接收内容的数组
 * @param  line_size 数组的最大容量
 * @retval 1 表示成功读取到一整行
 * @retval 0 表示当前还没有完整数据
 * @note   line_size 不应大于 HC05_LINE_MAX_LENGTH。
 */
uint8_t HC05_ReadLine(char *line, uint16_t line_size);

#ifdef __cplusplus
}
#endif

#endif /* __HC05_H */
