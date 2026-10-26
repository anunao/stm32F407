/**
  ******************************************************************************
  * @file    BtConfig.h
  * @brief   蓝牙配网数据定义和解析接口
  * @note    手机通过 HC-05 发送以下格式的配网信息：
  *
  *          WIFI:WiFi名称,WiFi密码
  *
  *          当前为了适配 HC05_LINE_MAX_LENGTH = 64：
  *          WiFi 名称最多 24 个字符；
  *          WiFi 密码最多 32 个字符；
  *          WiFi 名称和密码中不能包含逗号。
  ******************************************************************************
  */

#ifndef __BT_CONFIG_H
#define __BT_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief  WiFi 名称允许的最大长度
 */
#define BT_CONFIG_SSID_MAX_LENGTH       24

/**
 * @brief  WiFi 密码允许的最大长度
 */
#define BT_CONFIG_PASSWORD_MAX_LENGTH   32

/**
 * @brief  蓝牙配网数据
 */
typedef struct
{
    /**
     * @brief  WiFi 名称
     */
    char ssid[BT_CONFIG_SSID_MAX_LENGTH + 1];

    /**
     * @brief  WiFi 密码
     */
    char password[BT_CONFIG_PASSWORD_MAX_LENGTH + 1];

} BtConfig_Data;

/**
 * @brief  解析蓝牙发送的 WiFi 配网信息
 * @param  text 蓝牙接收到的一整行字符串
 * @param  config 用于保存 WiFi 名称和密码
 * @retval 1 表示解析成功
 * @retval 0 表示格式错误或内容超长
 * @note   正确格式为：
 *         WIFI:WiFi名称,WiFi密码
 */
uint8_t BtConfig_Parse(
    const char *text,
    BtConfig_Data *config
);

#ifdef __cplusplus
}
#endif

#endif /* __BT_CONFIG_H */

