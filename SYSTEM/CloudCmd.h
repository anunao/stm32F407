/**
  ******************************************************************************
  * @file    CloudCmd.h
  * @brief   指令解析头文件（巴法云指令 + 蓝牙表情命令）
  * @note    本模块只负责识别指令，不直接控制硬件。
  *
  *          云端（巴法云）支持的指令：
  *          ARM / DISARM / LED:ON / LED:OFF / BEEP:ON / BEEP:OFF / QUERY
  *
  *          蓝牙支持的表情命令：
  *          FACE:sleep / FACE:alert / FACE:happy / FACE:scared
  ******************************************************************************
  */

#ifndef __CLOUD_CMD_H
#define __CLOUD_CMD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief  云端指令类型
 */
typedef enum
{
    CLOUD_CMD_NONE = 0,
    CLOUD_CMD_ARM,
    CLOUD_CMD_DISARM,
    CLOUD_CMD_LED_ON,
    CLOUD_CMD_LED_OFF,
    CLOUD_CMD_BEEP_ON,
    CLOUD_CMD_BEEP_OFF,
    CLOUD_CMD_QUERY
} CloudCmd_Type;

/**
 * @brief  云端指令解析结果
 */
typedef struct
{
    /* 识别到的指令类型 */
    CloudCmd_Type type;
} CloudCmd_Data;

/**
 * @brief  表情类型（与 UI_Preview.h 的 UI_Face 对齐）
 */
typedef enum
{
    CLOUD_FACE_SLEEP = 0,   /* 睡觉 */
    CLOUD_FACE_ALERT,       /* 警觉 */
    CLOUD_FACE_HAPPY,       /* 开心 */
    CLOUD_FACE_SCARED       /* 惊吓 */
} CloudCmd_Face;

/**
 * @brief  解析巴法云下发的指令
 * @param  text 巴法云下发的完整文本
 * @param  command 用于保存指令解析结果
 * @retval 1 表示成功识别到有效指令
 * @retval 0 表示内容为空、主题错误或指令无法识别
 * @note   text 格式为巴法云 TCP 下发格式：
 *          cmd=2&uid=xxx&topic=主题&msg=指令
 */
uint8_t CloudCmd_Parse(const char *text, CloudCmd_Data *command);

/**
 * @brief  解析蓝牙表情命令
 * @param  text 蓝牙收到的完整文本（如 "FACE:happy"）
 * @param  face 用于保存表情类型
 * @retval 1 表示识别到有效表情命令
 * @retval 0 表示不是表情命令或格式错误
 */
uint8_t CloudCmd_ParseFace(const char *text, CloudCmd_Face *face);

#ifdef __cplusplus
}
#endif

#endif /* __CLOUD_CMD_H */
