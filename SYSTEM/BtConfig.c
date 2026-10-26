/**
  ******************************************************************************
  * @file    BtConfig.c
  * @brief   蓝牙配网信息解析程序
  * @note    手机通过 HC-05 发送：
  *          WIFI:WiFi名称,WiFi密码
  ******************************************************************************
  */

#include "BtConfig.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief  蓝牙配网命令前缀
 */
#define BT_CONFIG_PREFIX    "WIFI:"

/**
 * @brief  解析蓝牙发送的 WiFi 配网信息
 * @param  text 蓝牙接收到的一整行字符串
 * @param  config 用于保存 WiFi 名称和密码
 * @retval 1 表示解析成功
 * @retval 0 表示格式错误或内容超长
 */
uint8_t BtConfig_Parse(
    const char *text,
    BtConfig_Data *config
)
{
    const char *ssid_start;
    const char *separator;
    const char *password_start;

    uint16_t ssid_length;
    uint16_t password_length;

    /*
     * 检查传入的指针。
     */
    if (text == 0 || config == 0)
    {
        printf("BtConfig 错误：传入指针为空\r\n");
        return 0;
    }

    /*
     * 先清空保存配网信息的结构体，
     * 防止解析失败后继续使用旧数据。
     */
    memset(config, 0, sizeof(BtConfig_Data));

    /*
     * 检查字符串是否以 WIFI: 开头。
     */
    if (strncmp(
            text,
            BT_CONFIG_PREFIX,
            strlen(BT_CONFIG_PREFIX)
        ) != 0)
    {
        printf("BtConfig 错误：字符串不以 WIFI: 开头\r\n");
        return 0;
    }

    /*
     * WiFi 名称从 WIFI: 后面开始。
     */
    ssid_start = text + strlen(BT_CONFIG_PREFIX);

    /*
     * 查找 WiFi 名称和密码之间的逗号。
     */
    separator = strchr(ssid_start, ',');

    /*
     * 没有逗号，说明格式不正确。
     */
    if (separator == 0)
    {
        printf("BtConfig 错误：未找到 WiFi 名称和密码之间的逗号\r\n");
        return 0;
    }

    /*
     * 逗号紧跟在 WIFI: 后面，说明 WiFi 名称为空。
     */
    if (separator == ssid_start)
    {
        printf("BtConfig 错误:WiFi 名称为空\r\n");
        return 0;
    }

    /*
     * 密码从逗号后面开始。
     */
    password_start = separator + 1;

    /*
     * 逗号后面没有内容，说明密码为空。
     */
    if (*password_start == '\0')
    {
        printf("BtConfig 错误:WiFi 密码为空\r\n");
        return 0;
    }

    /*
     * 当前协议不允许密码中再次出现逗号。
     */
    if (strchr(password_start, ',') != 0)
    {
        printf("BtConfig 错误：密码中包含多余的逗号\r\n");
        return 0;
    }

    /*
     * 计算 WiFi 名称和密码长度。
     */
    ssid_length = (uint16_t)(separator - ssid_start);
    password_length = (uint16_t)strlen(password_start);

    /*
     * 检查 WiFi 名称是否超过允许长度。
     */
    if (ssid_length > BT_CONFIG_SSID_MAX_LENGTH)
    {
        printf("BtConfig 错误: WiFi 名称超过最大长度 %d\r\n", BT_CONFIG_SSID_MAX_LENGTH);
        return 0;
    }

    /*
     * 检查 WiFi 密码是否超过允许长度。
     */
    if (password_length > BT_CONFIG_PASSWORD_MAX_LENGTH)
    {
        printf("BtConfig 错误:WiFi 密码超过最大长度 %d\r\n", BT_CONFIG_PASSWORD_MAX_LENGTH);
        return 0;
    }

    /*
     * 复制 WiFi 名称，并补充字符串结束符。
     */
    memcpy(
        config->ssid,
        ssid_start,
        ssid_length
    );

    config->ssid[ssid_length] = '\0';

    /*
     * 复制 WiFi 密码，并补充字符串结束符。
     */
    memcpy(
        config->password,
        password_start,
        password_length
    );

    config->password[password_length] = '\0';

    return 1;
}
