/**
  ******************************************************************************
  * @file    BEMFA.h
  * @brief   巴法云 TCP 通信接口头文件
  * @note    巴法云连接、订阅、发布和下发消息由主循环状态机推进。
  *          ESP8266 只负责 Wi-Fi 和串口底层数据接收。
  ******************************************************************************
  */

#ifndef __BEMFA_H
#define __BEMFA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief  巴法云服务器地址
 */
#define BEMFA_SERVER_HOST       "bemfa.com"

/**
 * @brief  巴法云 TCP 服务器端口
 */
#define BEMFA_SERVER_PORT       8344

/**
 * @brief  巴法云用户私钥 UID
 * @note   这是旧项目中使用的 UID。
 *         如果不是你的巴法云账号，请替换成你自己的 UID。
 */
#define BEMFA_UID               0//"340287b3820244c3b6334de90100acbe0"

/**
 * @brief  巴法云主题
 */
#define BEMFA_TOPIC             0//"5nMkozart002"

/**
 * @brief  心跳发送间隔，单位为毫秒
 */
#define BEMFA_HEARTBEAT_TIME    30000U

/**
 * @brief  环境数据上报间隔，单位为毫秒
 */
#define BEMFA_UPLOAD_TIME       5000U

/**
 * @brief  巴法云下发消息的最大长度
 */
#define BEMFA_MESSAGE_MAX_LENGTH    128

/**
 * @brief  兼容旧主函数的巴法云连接接口
 * @param  无
 * @retval 1 请求已接受；0 请求未接受
 * @note   新代码应使用 BEMFA_RequestConnect()，并持续调用 BEMFA_Service()。
 */
uint8_t BEMFA_Connect(void);

/**
 * @brief  请求异步连接巴法云并订阅当前主题
 * @param  无
 * @retval 1 请求已接受；0 当前未连接 Wi-Fi 或已有事务正在执行
 * @note   请求接受后，需要在主循环中持续调用 BEMFA_Service()。
 */
uint8_t BEMFA_RequestConnect(void);

/**
 * @brief  推进巴法云 TCP 状态机
 * @param  无
 * @retval 无
 * @note   本函数不会使用 HAL_UART_Receive() 等待串口。
 */
void BEMFA_Service(void);

/**
 * @brief  向巴法云主题发布一条消息
 * @param  message 要发布的消息内容
 * @retval 1 表示发送成功
 * @retval 0 表示发送失败
 */
uint8_t BEMFA_Publish(const char *message);

/**
 * @brief  向巴法云发送心跳
 * @param  无
 * @retval 1 表示发送成功
 * @retval 0 表示发送失败
 */
uint8_t BEMFA_SendHeartbeat(void);

/**
 * @brief  通知巴法云模块 Wi-Fi 已断开
 * @param  无
 * @retval 无
 * @note   清除云连接状态，等待 Wi-Fi 恢复后再次请求连接。
 */
void BEMFA_OnWiFiDisconnected(void);

/**
 * @brief  判断巴法云是否已经连接
 * @param  无
 * @retval 1 表示已经连接
 * @retval 0 表示未连接
 */
uint8_t BEMFA_IsConnected(void);

/**
 * @brief  读取巴法云下发的一整条消息
 * @param  message 用于保存消息内容的数组
 * @param  message_size 数组容量
 * @retval 1 表示读取到完整消息
 * @retval 0 表示当前没有完整消息
 * @note   读取到的内容格式通常为：
 *         5nMkozart002&ARM
 */
uint8_t BEMFA_ReadMessage(
    char *message,
    uint16_t message_size
);


#ifdef __cplusplus
}
#endif

#endif /* __BEMFA_H */
