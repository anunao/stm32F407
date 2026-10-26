/**
  ******************************************************************************
  * @file    CloudCmd.c
  * @brief   巴法云下发指令解析程序
  * @note    本文件解析巴法云 TCP 下发格式：
  *          cmd=2&uid=用户UID&topic=主题&msg=指令
  *
 *          例如：
 *          cmd=2&uid=xxx&topic=5nMkozart002&msg=LED:ON
  ******************************************************************************
  */

#include "CloudCmd.h"
#include "BEMFA.h"

#include <string.h>

/**
 * @brief  从巴法云完整数据中取出 msg 内容
 * @param  text 巴法云下发的完整数据
 * @retval 指向指令内容的指针
 * @retval NULL 表示格式、UID 或主题错误
 */
static const char *CloudCmd_GetMessage(const char *text)
{
    const char *uid_start;
    const char *topic_mark;
    const char *topic_start;
    const char *message_mark;
    uint16_t uid_length;
    uint16_t topic_length;

    /*
     * 巴法云下发数据必须以 cmd=2&uid= 开头。
     * cmd=2 表示服务器下发消息。
     */
    if (strncmp(text, "cmd=2&uid=", 10) != 0)
    {
        return 0;
    }

    uid_start = text + 10;

    /*
     * 查找主题字段。
     */
    topic_mark = strstr(uid_start, "&topic=");
    if (topic_mark == 0)
    {
        return 0;
    }

    /*
     * 检查 UID 是否为本设备配置的 UID。
     */
    uid_length = (uint16_t)(topic_mark - uid_start);

    if (uid_length != strlen(BEMFA_UID))
    {
        return 0;
    }

    if (strncmp(uid_start, BEMFA_UID, uid_length) != 0)
    {
        return 0;
    }

    /*
     * 取出主题内容。
     */
    topic_start = topic_mark + 7;

    /*
     * 查找消息字段。
     */
    message_mark = strstr(topic_start, "&msg=");
    if (message_mark == 0)
    {
        return 0;
    }

    /*
     * 严格检查主题是否为本项目主题。
     */
    topic_length = (uint16_t)(message_mark - topic_start);

    if (topic_length != strlen(BEMFA_TOPIC))
    {
        return 0;
    }

    if (strncmp(topic_start, BEMFA_TOPIC, topic_length) != 0)
    {
        return 0;
    }

    /*
     * msg= 后面不能没有内容。
     */
    if (message_mark[5] == '\0')
    {
        return 0;
    }

    return message_mark + 5;
}

/**
 * @brief  解析巴法云下发的控制指令
 * @param  text 巴法云下发的完整数据
 * @param  command 用于保存解析结果
 * @retval 1 表示识别到有效指令
 * @retval 0 表示数据格式错误或指令无效
 */
uint8_t CloudCmd_Parse(
    const char *text,
    CloudCmd_Data *command
)
{
    const char *message;

    if (text == 0 || command == 0)
    {
        return 0;
    }

    /*
     * 先清除上一次的解析结果。
     */
    command->type = CLOUD_CMD_NONE;

    /*
     * 从完整巴法云数据中取出 msg。
     */
    message = CloudCmd_GetMessage(text);

    if (message == 0)
    {
        return 0;
    }

    if (strcmp(message, "ARM") == 0)
    {
        command->type = CLOUD_CMD_ARM;
        return 1;
    }

    if (strcmp(message, "DISARM") == 0)
    {
        command->type = CLOUD_CMD_DISARM;
        return 1;
    }

    if (strcmp(message, "LED:ON") == 0)
    {
        command->type = CLOUD_CMD_LED_ON;
        return 1;
    }

    if (strcmp(message, "LED:OFF") == 0)
    {
        command->type = CLOUD_CMD_LED_OFF;
        return 1;
    }

    if (strcmp(message, "BEEP:ON") == 0)
    {
        command->type = CLOUD_CMD_BEEP_ON;
        return 1;
    }

    if (strcmp(message, "BEEP:OFF") == 0)
    {
        command->type = CLOUD_CMD_BEEP_OFF;
        return 1;
    }

    if (strcmp(message, "QUERY") == 0)
    {
        command->type = CLOUD_CMD_QUERY;
        return 1;
    }

    return 0;
}

/**
 * @brief  解析蓝牙表情命令
 * @param  text 蓝牙收到的完整文本（如 "FACE:happy"）
 * @param  face 用于保存表情类型
 * @retval 1 表示识别到有效表情命令
 * @retval 0 表示不是表情命令或格式错误
 */
uint8_t CloudCmd_ParseFace(const char *text, CloudCmd_Face *face)
{
    if (text == 0 || face == 0)
    {
        return 0;
    }

    if (strcmp(text, "FACE:sleep") == 0)
    {
        *face = CLOUD_FACE_SLEEP;
        return 1;
    }
    if (strcmp(text, "FACE:alert") == 0)
    {
        *face = CLOUD_FACE_ALERT;
        return 1;
    }
    if (strcmp(text, "FACE:happy") == 0)
    {
        *face = CLOUD_FACE_HAPPY;
        return 1;
    }
    if (strcmp(text, "FACE:scared") == 0)
    {
        *face = CLOUD_FACE_SCARED;
        return 1;
    }

    return 0;
}

