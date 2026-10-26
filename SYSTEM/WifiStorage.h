/**
  ******************************************************************************
  * @file    WifiStorage.h
  * @brief   使用 AT24C02 保存和读取 Wi-Fi 配置
  * @note    AT24C02 的容量为 256 字节。本模块只使用两个固定区域：
  *          地址 0 保存 SSID，地址 32 保存密码。
  ******************************************************************************
  */

#ifndef __WIFI_STORAGE_H
#define __WIFI_STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "BtConfig.h"

#include <stdint.h>

/**
 * @brief  从 AT24C02 读取 Wi-Fi 配置
 * @param  config 用于接收 SSID 和密码的结构体
 * @retval 1 读取到有效配置；0 没有有效配置或参数无效
 * @note   EEPROM 中只要有一个字段为空，就认为配置无效。
 */
uint8_t WifiStorage_Load(BtConfig_Data *config);

/**
 * @brief  将 Wi-Fi 配置写入 AT24C02
 * @param  config 要保存的 SSID 和密码
 * @retval 1 保存成功；0 参数无效或 EEPROM 写入失败
 * @note   本函数应在 Wi-Fi 连接成功后调用，避免错误配置覆盖旧配置。
 */
uint8_t WifiStorage_Save(const BtConfig_Data *config);

#ifdef __cplusplus
}
#endif

#endif /* __WIFI_STORAGE_H */
