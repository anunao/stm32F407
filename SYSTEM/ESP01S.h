/**
  ******************************************************************************
  * @file    ESP01S.h
  * @brief   ESP8266（ESP-01S）串口驱动头文件
  * @note    ESP-01S 使用 USART3。
  *          当前工程中：
  *          PB10 -> ESP01S_TX
  *          PB11 -> ESP01S_RX
  *          波特率为 115200。
  ******************************************************************************
  */

#ifndef __ESP01S_H
#define __ESP01S_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief  ESP-01S 串口接收环形缓冲区大小
 */
#define ESP01S_RX_BUFFER_SIZE    512U

/**
 * @brief  启动 ESP-01S 串口接收
 * @param  无
 * @retval 无
 * @note   需要在 USART3 初始化完成后调用一次。
 */
void ESP01S_StartReceive(void);

/**
 * @brief  处理 ESP-01S 单字节接收完成事件
 * @param  无
 * @retval 无
 * @note   由统一的 HAL_UART_RxCpltCallback() 调用，只保存字节并重新启动接收。
 */
void ESP01S_RxCpltCallback(void);

/**
 * @brief  从 ESP-01S 环形缓冲区取出一个字节
 * @param  byte 用于保存取出字节的指针，不可为 NULL
 * @retval 1 成功取出一个字节
 * @retval 0 当前没有数据或参数无效
 */
uint8_t ESP01S_ReadByte(uint8_t *byte);

/**
 * @brief  查询 ESP-01S 接收缓冲区是否发生溢出
 * @param  无
 * @retval 1 已发生溢出
 * @retval 0 未发生溢出
 */
uint8_t ESP01S_IsRxOverflow(void);

/**
 * @brief  清除 ESP-01S 接收缓冲区溢出标志
 * @param  无
 * @retval 无
 */
void ESP01S_ClearRxOverflow(void);

/**
 * @brief  提交 Wi-Fi 配置并启动非阻塞连接流程
 * @param  ssid Wi-Fi 名称，长度为 1 到 24 个字符
 * @param  password Wi-Fi 密码，长度为 1 到 32 个字符
 * @retval 1 配置已接受；0 参数非法
 * @note   Wi-Fi 凭据只保存在 RAM 中，调用后由 ESP01S_Service() 推进连接。
 */
uint8_t ESP01S_RequestWiFi(const char *ssid, const char *password);

/**
 * @brief  推进 ESP8266 Wi-Fi 非阻塞状态机
 * @param  无
 * @retval 无
 * @note   应在主循环中反复调用，不会使用 HAL_UART_Receive() 等待串口。
 */
void ESP01S_Service(void);

/**
 * @brief  查询 ESP8266 是否已经连接 Wi-Fi
 * @param  无
 * @retval 1 已连接；0 未连接或正在连接
 */
uint8_t ESP01S_IsWiFiConnected(void);

/**
 * @brief  取走一次 Wi-Fi 连接结果
 * @param  success 用于接收结果，1 表示成功，0 表示失败
 * @retval 1 取到新结果；0 当前没有新结果或参数无效
 */
uint8_t ESP01S_TakeWiFiResult(uint8_t *success);

/**
 * @brief  通知 ESP8266 Wi-Fi 已断开
 * @param  无
 * @retval 无
 * @note   本函数只清除在线标志，后续是否重新连接由主循环决定。
 */
void ESP01S_NotifyWiFiDisconnected(void);

/**
 * @brief  向 ESP-01S 发送字符串
 * @param  text 要发送的字符串，必须以 '\0' 结尾
 * @retval 无
 */
uint8_t ESP01S_SendString(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* __ESP01S_H */
