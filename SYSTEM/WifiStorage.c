/**
  ******************************************************************************
  * @file    WifiStorage.c
  * @brief   AT24C02 Wi-Fi 配置存储程序
  * @note    使用固定地址保存字符串，代码简单，适合当前项目的首次配网。
  ******************************************************************************
  */

#include "WifiStorage.h"
#include "at24c02.h"

#include <string.h>

/* EEPROM 中的固定存储位置。每个区域都预留一个字节保存字符串结束符。 */
#define WIFI_STORAGE_SSID_ADDR       0U
#define WIFI_STORAGE_PASSWORD_ADDR   32U
#define WIFI_STORAGE_SSID_SIZE       (BT_CONFIG_SSID_MAX_LENGTH + 1U)
#define WIFI_STORAGE_PASSWORD_SIZE   (BT_CONFIG_PASSWORD_MAX_LENGTH + 1U)

/**
 * @brief  检查一段 EEPROM 字符串是否有效
 * @param  text 字符串缓冲区
 * @param  size 缓冲区大小
 * @retval 1 字符串有效；0 字符串为空、未结束或含非法字符
 */
static uint8_t WifiStorage_IsValidText(const char *text, uint16_t size)
{
    uint16_t i;

    if ((text == 0) || (size < 2U) || (text[size - 1U] != '\0'))
    {
        return 0U;
    }

    if (text[0] == '\0')
    {
        return 0U;
    }

    for (i = 0U; i < size - 1U; i++)
    {
        if (text[i] == '\0')
        {
            break;
        }

        /* 与蓝牙配网协议保持一致，不允许破坏 AT 指令的字符。 */
        if ((text[i] < 0x20) || (text[i] > 0x7E) ||
            (text[i] == '"') || (text[i] == '\\') || (text[i] == ','))
        {
            return 0U;
        }
    }

    return 1U;
}

uint8_t WifiStorage_Load(BtConfig_Data *config)
{
    char ssid[WIFI_STORAGE_SSID_SIZE];
    char password[WIFI_STORAGE_PASSWORD_SIZE];

    if (config == 0)
    {
        return 0U;
    }

    memset(config, 0, sizeof(*config));
    memset(ssid, 0, sizeof(ssid));
    memset(password, 0, sizeof(password));

    if (read_bytes_from_at24c02(WIFI_STORAGE_SSID_ADDR,
                                (uint8_t *)ssid,
                                sizeof(ssid)) < 0)
    {
        return 0U;
    }

    if (read_bytes_from_at24c02(WIFI_STORAGE_PASSWORD_ADDR,
                                (uint8_t *)password,
                                sizeof(password)) < 0)
    {
        return 0U;
    }

    if (!WifiStorage_IsValidText(ssid, sizeof(ssid)) ||
        !WifiStorage_IsValidText(password, sizeof(password)))
    {
        return 0U;
    }

    strcpy(config->ssid, ssid);
    strcpy(config->password, password);

    return 1U;
}

uint8_t WifiStorage_Save(const BtConfig_Data *config)
{
    char ssid[WIFI_STORAGE_SSID_SIZE];
    char password[WIFI_STORAGE_PASSWORD_SIZE];

    if (config == 0)
    {
        return 0U;
    }

    if (!WifiStorage_IsValidText(config->ssid, sizeof(config->ssid)) ||
        !WifiStorage_IsValidText(config->password, sizeof(config->password)))
    {
        return 0U;
    }

    memset(ssid, 0, sizeof(ssid));
    memset(password, 0, sizeof(password));
    strcpy(ssid, config->ssid);
    strcpy(password, config->password);

    if (write_bytes_to_at24c02(WIFI_STORAGE_SSID_ADDR,
                               (uint8_t *)ssid,
                               sizeof(ssid)) < 0)
    {
        return 0U;
    }

    if (write_bytes_to_at24c02(WIFI_STORAGE_PASSWORD_ADDR,
                               (uint8_t *)password,
                               sizeof(password)) < 0)
    {
        return 0U;
    }

    return 1U;
}
