/**
  ******************************************************************************
  * @file    BEMFA.c
  * @brief   巴法云 TCP 非阻塞通信程序
  * @note    本文件负责巴法云 TCP 连接、主题订阅、消息发布和下发解析。
  *          ESP01S.c 负责 Wi-Fi 和串口接收，本文件不直接读取 UART 硬件。
  ******************************************************************************
  */

#include "BEMFA.h"
#include "ESP01S.h"
#include "main.h"

#include <stdio.h>
#include <string.h>

/* 缓冲区和各阶段超时时间。 */
#define BEMFA_COMMAND_MAX_LENGTH       256U
#define BEMFA_LINE_MAX_LENGTH          128U
#define BEMFA_PAYLOAD_MAX_LENGTH       256U
#define BEMFA_AT_TIMEOUT_MS            2000U
#define BEMFA_TCP_TIMEOUT_MS           10000U
#define BEMFA_PROMPT_TIMEOUT_MS        3000U
#define BEMFA_SEND_TIMEOUT_MS          5000U
#define BEMFA_SUBSCRIBE_TIMEOUT_MS     5000U
#define BEMFA_RETRY_DELAY_MS           5000U

/* ESP8266 普通 TCP 模式下的接收前缀。 */
#define BEMFA_IPD_PREFIX               "+IPD,"
#define BEMFA_IPD_PREFIX_LENGTH        5U

/* 巴法云连接状态机。每次只执行一个小步骤。 */
typedef enum
{
    BEMFA_STATE_OFFLINE = 0,
    BEMFA_STATE_RETRY_WAIT,
    BEMFA_STATE_MUX_SEND,
    BEMFA_STATE_MUX_WAIT,
    BEMFA_STATE_MODE_SEND,
    BEMFA_STATE_MODE_WAIT,
    BEMFA_STATE_TCP_SEND,
    BEMFA_STATE_TCP_WAIT,
    BEMFA_STATE_SUBSCRIBE_SEND,
    BEMFA_STATE_PAYLOAD_WAIT_PROMPT,
    BEMFA_STATE_PAYLOAD_WAIT_SEND,
    BEMFA_STATE_SUBSCRIBE_WAIT_ACK,
    BEMFA_STATE_ONLINE
} BEMFA_State;

/* 当前发送数据的类型，用于发送完成后决定下一步。 */
typedef enum
{
    BEMFA_PAYLOAD_NONE = 0,
    BEMFA_PAYLOAD_SUBSCRIBE,
    BEMFA_PAYLOAD_PUBLISH,
    BEMFA_PAYLOAD_HEARTBEAT
} BEMFA_PayloadType;

/* 当前云端连接状态。 */
static BEMFA_State bemfa_state = BEMFA_STATE_OFFLINE;
static BEMFA_PayloadType bemfa_payload_type = BEMFA_PAYLOAD_NONE;
static uint32_t bemfa_deadline;
static uint8_t bemfa_connected;
static uint8_t bemfa_heartbeat_pending;

/* 记录最近一次设备主动发布的完整报文，用于过滤服务器回显。 */
static char bemfa_last_publish_command[BEMFA_COMMAND_MAX_LENGTH];
static uint8_t bemfa_publish_echo_pending;
static uint32_t bemfa_publish_echo_deadline;

/* 当前一条发送数据，必须复制到模块内部，不能保存主函数的局部指针。 */
static char bemfa_tx_payload[BEMFA_PAYLOAD_MAX_LENGTH];
static uint16_t bemfa_tx_length;
static char bemfa_command[BEMFA_COMMAND_MAX_LENGTH];

/* AT 回复标志。 */
static uint8_t bemfa_response_ok;
static uint8_t bemfa_response_error;
static uint8_t bemfa_prompt_seen;
static uint8_t bemfa_send_ok;
static uint8_t bemfa_tcp_connected;
static uint8_t bemfa_tcp_closed;
static uint8_t bemfa_wifi_disconnected;
static uint8_t bemfa_subscribe_ack;

/* 普通文本回复解析缓冲区。 */
static char bemfa_line[BEMFA_LINE_MAX_LENGTH];
static uint16_t bemfa_line_length;

/* 巴法云下发消息缓冲区。 */
static char bemfa_message[BEMFA_MESSAGE_MAX_LENGTH];
static uint8_t bemfa_message_ready;

/* +IPD,<长度>: 数据解析状态。 */
static uint8_t bemfa_ipd_prefix_index;
static uint8_t bemfa_ipd_reading_length;
static uint8_t bemfa_ipd_reading_data;
static uint32_t bemfa_ipd_data_length;
static uint32_t bemfa_ipd_data_index;
static uint8_t bemfa_ipd_data_overflow;

/**
 * @brief  判断状态机是否超时
 * @param  now 当前系统滴答
 * @param  deadline 超时时间点
 * @retval 1 已超时；0 未超时
 */
static uint8_t BEMFA_IsTimeout(uint32_t now, uint32_t deadline)
{
    return (uint8_t)((int32_t)(now - deadline) >= 0);
}

/**
 * @brief  清除 +IPD 解析状态
 * @param  无
 * @retval 无
 */
static void BEMFA_ResetIpdState(void)
{
    bemfa_ipd_prefix_index = 0U;
    bemfa_ipd_reading_length = 0U;
    bemfa_ipd_reading_data = 0U;
    bemfa_ipd_data_length = 0U;
    bemfa_ipd_data_index = 0U;
    bemfa_ipd_data_overflow = 0U;
}

/**
 * @brief  清除当前 AT 事务的回复标志
 * @param  无
 * @retval 无
 */
static void BEMFA_ClearResponseFlags(void)
{
    bemfa_response_ok = 0U;
    bemfa_response_error = 0U;
    bemfa_prompt_seen = 0U;
    bemfa_send_ok = 0U;
    bemfa_tcp_connected = 0U;
    bemfa_tcp_closed = 0U;
    bemfa_wifi_disconnected = 0U;
    bemfa_subscribe_ack = 0U;
    bemfa_line_length = 0U;
}

/**
 * @brief  清除云端接收解析状态
 * @param  无
 * @retval 无
 */
static void BEMFA_ResetReceiveState(void)
{
    bemfa_message_ready = 0U;
    bemfa_line_length = 0U;
    BEMFA_ResetIpdState();
}

/**
 * @brief  处理一行 ESP8266 文本回复
 * @param  无
 * @retval 无
 * @note   这里只设置标志，真正的状态切换由 BEMFA_Service() 完成。
 */
static void BEMFA_ProcessLine(void)
{
    bemfa_line[bemfa_line_length] = '\0';

    if (bemfa_line_length == 0U)
    {
        return;
    }

    if ((strcmp(bemfa_line, "OK") == 0) ||
        (strstr(bemfa_line, "ready") != NULL))
    {
        bemfa_response_ok = 1U;
    }

    if (strstr(bemfa_line, "SEND OK") != NULL)
    {
        bemfa_send_ok = 1U;
    }

    if (strstr(bemfa_line, "CONNECT") != NULL)
    {
        bemfa_tcp_connected = 1U;
    }

    if (strstr(bemfa_line, "CLOSED") != NULL)
    {
        bemfa_tcp_closed = 1U;
    }

    if (strstr(bemfa_line, "WIFI DISCONNECT") != NULL)
    {
        bemfa_wifi_disconnected = 1U;
    }

    if ((strstr(bemfa_line, "ERROR") != NULL) ||
        (strstr(bemfa_line, "FAIL") != NULL) ||
        (strstr(bemfa_line, "busy") != NULL))
    {
        bemfa_response_error = 1U;
    }
}

/**
 * @brief  完成一条 +IPD 数据解析
 * @param  无
 * @retval 无
 */
static void BEMFA_FinishIpdMessage(void)
{
    uint16_t length;

    if (bemfa_ipd_data_overflow == 0U)
    {
        length = (uint16_t)bemfa_ipd_data_index;

        while ((length > 0U) &&
               ((bemfa_message[length - 1U] == '\r') ||
                (bemfa_message[length - 1U] == '\n')))
        {
            length--;
        }

        bemfa_message[length] = '\0';

        /*
         * 以下内容是巴法云协议回执，不是手机下发的业务指令：
         *   cmd=0&res=1：心跳回执
         *   cmd=1&res=1：订阅回执
         *   cmd=2&res=1：发布回执
         * 协议回执在这里直接消化，避免 main.c 打印“命令解析失败”。
         */
        if (strcmp(bemfa_message, "cmd=0&res=1") == 0)
        {
            /* 心跳回执无需交给应用层。 */
        }
        else if (strcmp(bemfa_message, "cmd=1&res=1") == 0)
        {
            bemfa_subscribe_ack = 1U;
        }
        else if (strcmp(bemfa_message, "cmd=2&res=1") == 0)
        {
            /* 发布回执无需交给应用层。 */
        }
        else if (bemfa_publish_echo_pending &&
                 !BEMFA_IsTimeout(HAL_GetTick(), bemfa_publish_echo_deadline) &&
                 (strcmp(bemfa_message, bemfa_last_publish_command) == 0))
        {
            /* 忽略设备自己发布后，服务器返回的同内容回显。 */
            bemfa_publish_echo_pending = 0U;
        }
        else if ((length > 0U) && !bemfa_message_ready)
        {
            /* 其它数据交给 CloudCmd 继续判断是否为有效控制命令。 */
            bemfa_message_ready = 1U;
        }
    }

    BEMFA_ResetIpdState();
}

/**
 * @brief  处理 ESP8266 的一个接收字节
 * @param  byte 从 ESP 软件环形缓冲区取出的字节
 * @retval 无
 * @note   +IPD 数据按长度读取，载荷中的回车换行不会提前结束解析。
 */
static void BEMFA_ProcessByte(uint8_t byte)
{
    if (bemfa_ipd_reading_data)
    {
        if (bemfa_ipd_data_index < BEMFA_MESSAGE_MAX_LENGTH - 1U)
        {
            bemfa_message[bemfa_ipd_data_index] = (char)byte;
        }
        else
        {
            bemfa_ipd_data_overflow = 1U;
        }

        bemfa_ipd_data_index++;
        if (bemfa_ipd_data_index >= bemfa_ipd_data_length)
        {
            BEMFA_FinishIpdMessage();
        }
        return;
    }

    if (bemfa_ipd_reading_length)
    {
        if ((byte >= '0') && (byte <= '9'))
        {
            bemfa_ipd_data_length = bemfa_ipd_data_length * 10U +
                                    (uint32_t)(byte - '0');
            if (bemfa_ipd_data_length >= BEMFA_MESSAGE_MAX_LENGTH)
            {
                bemfa_ipd_data_overflow = 1U;
            }
        }
        else if ((byte == ':') && (bemfa_ipd_data_length > 0U))
        {
            bemfa_ipd_reading_length = 0U;
            bemfa_ipd_reading_data = 1U;
            bemfa_ipd_data_index = 0U;
        }
        else
        {
            BEMFA_ResetIpdState();
        }
        return;
    }

    if (bemfa_ipd_prefix_index > 0U)
    {
        if (byte == (uint8_t)BEMFA_IPD_PREFIX[bemfa_ipd_prefix_index])
        {
            bemfa_ipd_prefix_index++;
            if (bemfa_ipd_prefix_index >= BEMFA_IPD_PREFIX_LENGTH)
            {
                bemfa_ipd_prefix_index = 0U;
                bemfa_ipd_reading_length = 1U;
                bemfa_ipd_data_length = 0U;
                bemfa_ipd_data_index = 0U;
                bemfa_ipd_data_overflow = 0U;
            }
            return;
        }

        /* 当前前缀不匹配，重新判断当前字节是否是下一条前缀的开始。 */
        bemfa_ipd_prefix_index = (byte == '+') ? 1U : 0U;
        if (bemfa_ipd_prefix_index > 0U)
        {
            return;
        }
    }
    else if (byte == '+')
    {
        bemfa_ipd_prefix_index = 1U;
        return;
    }

    if (byte == '>')
    {
        bemfa_prompt_seen = 1U;
    }

    if (byte == '\r')
    {
        return;
    }

    if (byte == '\n')
    {
        BEMFA_ProcessLine();
        bemfa_line_length = 0U;
        return;
    }

    if (bemfa_line_length < BEMFA_LINE_MAX_LENGTH - 1U)
    {
        bemfa_line[bemfa_line_length++] = (char)byte;
    }
    else
    {
        /* 文本回复过长时丢弃当前行，等待下一行。 */
        bemfa_line_length = 0U;
    }

}

/**
 * @brief  读取并解析当前已经收到的 ESP 字节
 * @param  无
 * @retval 无
 */
static void BEMFA_ProcessIncoming(void)
{
    uint8_t byte;

    while (!bemfa_message_ready && ESP01S_ReadByte(&byte))
    {
        BEMFA_ProcessByte(byte);
    }

    if (ESP01S_IsRxOverflow())
    {
        /* 缓冲区溢出后清理解析状态，避免把残缺数据拼成错误命令。 */
        ESP01S_ClearRxOverflow();
        BEMFA_ResetReceiveState();
        bemfa_response_error = 1U;
    }
}

/**
 * @brief  进入巴法云离线状态
 * @param  retry 1 表示 Wi-Fi 仍在线时等待后重试，0 表示等待 Wi-Fi
 * @retval 无
 */
static void BEMFA_SetOffline(uint8_t retry)
{
    bemfa_connected = 0U;
    bemfa_payload_type = BEMFA_PAYLOAD_NONE;
    bemfa_heartbeat_pending = 0U;
    bemfa_publish_echo_pending = 0U;
    bemfa_last_publish_command[0] = '\0';
    BEMFA_ClearResponseFlags();
    BEMFA_ResetReceiveState();

    if (retry && ESP01S_IsWiFiConnected())
    {
        bemfa_deadline = HAL_GetTick() + BEMFA_RETRY_DELAY_MS;
        bemfa_state = BEMFA_STATE_RETRY_WAIT;
    }
    else
    {
        bemfa_state = BEMFA_STATE_OFFLINE;
    }
}

/**
 * @brief  启动一次 CIPSEND 数据发送事务
 * @param  payload 要发送的 TCP 载荷
 * @param  type 载荷类型
 * @retval 1 已启动；0 参数无效或数据过长
 * @note   模块只保存一条正在发送的数据，不缓存离线应用消息。
 */
static uint8_t BEMFA_StartPayload(const char *payload, BEMFA_PayloadType type)
{
    int command_length;

    if (payload == NULL)
    {
        return 0U;
    }

    bemfa_tx_length = (uint16_t)strlen(payload);
    if ((bemfa_tx_length == 0U) ||
        (bemfa_tx_length >= BEMFA_PAYLOAD_MAX_LENGTH))
    {
        return 0U;
    }

    memcpy(bemfa_tx_payload, payload, bemfa_tx_length + 1U);
    bemfa_payload_type = type;
    BEMFA_ClearResponseFlags();

    command_length = snprintf(
        bemfa_command,
        sizeof(bemfa_command),
        "AT+CIPSEND=%u\r\n",
        (unsigned int)bemfa_tx_length);

    if ((command_length <= 0) ||
        ((uint16_t)command_length >= sizeof(bemfa_command)))
    {
        return 0U;
    }

    if (ESP01S_SendString(bemfa_command) == 0U)
    {
        bemfa_payload_type = BEMFA_PAYLOAD_NONE;
        return 0U;
    }

    bemfa_deadline = HAL_GetTick() + BEMFA_PROMPT_TIMEOUT_MS;
    bemfa_state = BEMFA_STATE_PAYLOAD_WAIT_PROMPT;

    return 1U;
}

/**
 * @brief  请求异步连接巴法云并订阅主题
 * @param  无
 * @retval 1 请求已接受；0 当前未连接 Wi-Fi 或状态机忙
 */
uint8_t BEMFA_RequestConnect(void)
{
    if (!ESP01S_IsWiFiConnected())
    {
        return 0U;
    }

    /* 退避状态由 BEMFA_Service() 自己推进，避免主循环重复重置计时器。 */
    if (bemfa_state != BEMFA_STATE_OFFLINE)
    {
        return 0U;
    }

    bemfa_connected = 0U;
    bemfa_heartbeat_pending = 0U;
    BEMFA_ClearResponseFlags();
    BEMFA_ResetReceiveState();
    bemfa_state = BEMFA_STATE_MUX_SEND;

    return 1U;
}

/**
 * @brief  推进巴法云 TCP 非阻塞状态机
 * @param  无
 * @retval 无
 * @note   必须在主循环中反复调用。
 */
void BEMFA_Service(void)
{
    uint32_t now;           /* 当前系统滴答计时，用于超时判断 */
    int command_length;     /* snprintf 格式化后的命令字符串长度 */

    /* ---- 前置检查：Wi-Fi 连通性 ---- */

    /* Wi-Fi 未连接时，云端只保持离线，不发送 TCP 设置指令。 */
    if (!ESP01S_IsWiFiConnected())
    {
        if (bemfa_state != BEMFA_STATE_OFFLINE)
        {
            BEMFA_SetOffline(0U);   /* 不重试，仅静默回到离线 */
        }
        return;
    }

    /* ---- 处理 ESP01S 串口接收缓冲区中的 AT 响应与 IPD 数据 ---- */
    BEMFA_ProcessIncoming();

    /* Wi-Fi 在运行中断开（收到 CLOSED 或 link failed），通知 ESP01S 并回到离线 */
    if (bemfa_wifi_disconnected)
    {
        ESP01S_NotifyWiFiDisconnected();
        BEMFA_SetOffline(0U);       /* 不重试，等待 Wi-Fi 重新连接 */
        return;
    }

    /* TCP 连接被远端关闭，回到离线并启动重试等待 */
    if (bemfa_tcp_closed)
    {
        BEMFA_SetOffline(1U);       /* 1U = 允许重试 */
        return;
    }

    /* ---- 状态机驱动 ---- */
    now = HAL_GetTick();

    switch (bemfa_state)
    {
    /* 离线态：等待外部调用 BEMFA_Connect() 触发重连 */
    case BEMFA_STATE_OFFLINE:
        break;

    /* 重试等待态：等待退避时间到期后再重新发起连接流程 */
    case BEMFA_STATE_RETRY_WAIT:
        if (BEMFA_IsTimeout(now, bemfa_deadline))
        {
            bemfa_state = BEMFA_STATE_MUX_SEND;
        }
        break;

    /* ---- 阶段 1：设置单连接模式 AT+CIPMUX=0 ---- */
    case BEMFA_STATE_MUX_SEND:
        BEMFA_ClearResponseFlags();
        if (ESP01S_SendString("AT+CIPMUX=0\r\n") == 0U)
        {
            BEMFA_SetOffline(1U);
            break;
        }
        bemfa_deadline = now + BEMFA_AT_TIMEOUT_MS;
        bemfa_state = BEMFA_STATE_MUX_WAIT;
        break;

    case BEMFA_STATE_MUX_WAIT:
        if (bemfa_response_error || BEMFA_IsTimeout(now, bemfa_deadline))
        {
            BEMFA_SetOffline(1U);   /* 设置失败，回到离线并重试 */
        }
        else if (bemfa_response_ok)
        {
            bemfa_state = BEMFA_STATE_MODE_SEND;    /* 成功，进入下一阶段 */
        }
        break;

    /* ---- 阶段 2：设置普通传输模式 AT+CIPMODE=0 ---- */
    case BEMFA_STATE_MODE_SEND:
        BEMFA_ClearResponseFlags();
        if (ESP01S_SendString("AT+CIPMODE=0\r\n") == 0U)
        {
            BEMFA_SetOffline(1U);
            break;
        }
        bemfa_deadline = now + BEMFA_AT_TIMEOUT_MS;
        bemfa_state = BEMFA_STATE_MODE_WAIT;
        break;

    case BEMFA_STATE_MODE_WAIT:
        if (bemfa_response_error || BEMFA_IsTimeout(now, bemfa_deadline))
        {
            BEMFA_SetOffline(1U);   /* 设置失败，回到离线并重试 */
        }
        else if (bemfa_response_ok)
        {
            bemfa_state = BEMFA_STATE_TCP_SEND;     /* 成功，进入下一阶段 */
        }
        break;

    /* ---- 阶段 3：发起 TCP 连接 AT+CIPSTART ---- */
    case BEMFA_STATE_TCP_SEND:
        /* 拼接 TCP 连接命令：AT+CIPSTART="TCP","主机",端口 */
        command_length = snprintf(
            bemfa_command,
            sizeof(bemfa_command),
            "AT+CIPSTART=\"TCP\",\"%s\",%u\r\n",
            BEMFA_SERVER_HOST,
            (unsigned int)BEMFA_SERVER_PORT);

        if ((command_length <= 0) ||
            ((uint16_t)command_length >= sizeof(bemfa_command)))
        {
            BEMFA_SetOffline(1U);   /* 命令格式化失败或截断，回到离线 */
        }
        else
        {
            BEMFA_ClearResponseFlags();
            if (ESP01S_SendString(bemfa_command) == 0U)
            {
                BEMFA_SetOffline(1U);
            }
            else
            {
                bemfa_deadline = now + BEMFA_TCP_TIMEOUT_MS;
                bemfa_state = BEMFA_STATE_TCP_WAIT;
            }
        }
        break;

    case BEMFA_STATE_TCP_WAIT:
        if (bemfa_response_error || BEMFA_IsTimeout(now, bemfa_deadline))
        {
            BEMFA_SetOffline(1U);   /* TCP 连接失败，回到离线并重试 */
        }
        else if (bemfa_response_ok || bemfa_tcp_connected)
        {
            bemfa_state = BEMFA_STATE_SUBSCRIBE_SEND;   /* TCP 已连接，进入订阅阶段 */
        }
        break;

    /* ---- 阶段 4：发送订阅请求 cmd=1&uid=xxx&topic=xxx ---- */
    case BEMFA_STATE_SUBSCRIBE_SEND:
        /* 拼接巴法云订阅协议报文 */
        command_length = snprintf(
            bemfa_command,
            sizeof(bemfa_command),
            "cmd=1&uid=%s&topic=%s\r\n",
            BEMFA_UID,
            BEMFA_TOPIC);

        if ((command_length <= 0) ||
            ((uint16_t)command_length >= sizeof(bemfa_command)) ||
            !BEMFA_StartPayload(bemfa_command, BEMFA_PAYLOAD_SUBSCRIBE))
        {
            BEMFA_SetOffline(1U);   /* 订阅报文格式化失败或发送启动失败 */
        }
        break;

    /* ---- 阶段 5：等待 ESP01S 发送提示符 ">" 后写入实际载荷 ---- */
    case BEMFA_STATE_PAYLOAD_WAIT_PROMPT:
        if (bemfa_response_error || BEMFA_IsTimeout(now, bemfa_deadline))
        {
            BEMFA_SetOffline(1U);   /* 未收到提示符，回到离线 */
        }
        else if (bemfa_prompt_seen)
        {
            /* 收到 ">" 提示符，将待发送载荷写入 ESP01S 串口 */
            BEMFA_ClearResponseFlags();
            if (ESP01S_SendString(bemfa_tx_payload) == 0U)
            {
                BEMFA_SetOffline(1U);
            }
            else
            {
                bemfa_deadline = now + BEMFA_SEND_TIMEOUT_MS;
                bemfa_state = BEMFA_STATE_PAYLOAD_WAIT_SEND;
            }
        }
        break;

    /* ---- 阶段 6：等待 SEND OK 确认载荷发送完成 ---- */
    case BEMFA_STATE_PAYLOAD_WAIT_SEND:
        if (bemfa_response_error || BEMFA_IsTimeout(now, bemfa_deadline))
        {
            BEMFA_SetOffline(1U);   /* 发送失败，回到离线 */
        }
        else if (bemfa_send_ok)
        {
            if (bemfa_payload_type == BEMFA_PAYLOAD_SUBSCRIBE)
            {
                /* 订阅报文需要等待服务器 ACK，进入等待确认态 */
                bemfa_deadline = now + BEMFA_SUBSCRIBE_TIMEOUT_MS;
                bemfa_state = BEMFA_STATE_SUBSCRIBE_WAIT_ACK;
            }
            else
            {
                /* 发布和心跳以 SEND OK 作为本次发送完成标志。 */
                bemfa_payload_type = BEMFA_PAYLOAD_NONE;
                bemfa_state = BEMFA_STATE_ONLINE;
            }
        }
        break;

    /* ---- 阶段 7：等待巴法云服务器返回订阅确认 ---- */
    case BEMFA_STATE_SUBSCRIBE_WAIT_ACK:
        if (bemfa_response_error || BEMFA_IsTimeout(now, bemfa_deadline))
        {
            BEMFA_SetOffline(1U);   /* 订阅超时或出错，回到离线 */
        }
        else if (bemfa_subscribe_ack)
        {
            /* 收到订阅确认，标记已连接，进入在线态 */
            bemfa_connected = 1U;
            bemfa_payload_type = BEMFA_PAYLOAD_NONE;
            bemfa_state = BEMFA_STATE_ONLINE;
        }
        break;

    /* ---- 在线态：处理心跳发送 ---- */
    case BEMFA_STATE_ONLINE:
        if (bemfa_heartbeat_pending)
        {
            /* 有待发送的心跳，构造 ping 报文并发送 */
            bemfa_heartbeat_pending = 0U;
            if (!BEMFA_StartPayload("ping\r\n", BEMFA_PAYLOAD_HEARTBEAT))
            {
                BEMFA_SetOffline(1U);   /* 心跳发送启动失败，回到离线 */
            }
        }
        break;

    /* 非法状态，回到离线并重试 */
    default:
        BEMFA_SetOffline(1U);
        break;
    }
}

/**
 * @brief  兼容旧主函数的连接接口
 * @param  无
 * @retval 1 请求已接受；0 请求失败
 * @note   新主循环应使用 BEMFA_RequestConnect()，并持续调用 BEMFA_Service()。
 */
uint8_t BEMFA_Connect(void)
{
    return BEMFA_RequestConnect();
}

/**
 * @brief  向巴法云主题提交一条异步发布消息
 * @param  message 消息内容，不允许包含 '&'、回车或换行
 * @retval 1 消息已提交；0 当前离线、忙或消息非法
 */
uint8_t BEMFA_Publish(const char *message)
{
    int command_length;

    if ((message == NULL) || !bemfa_connected ||
        (bemfa_state != BEMFA_STATE_ONLINE))
    {
        return 0U;
    }

    if ((strchr(message, '&') != NULL) ||
        (strchr(message, '\r') != NULL) ||
        (strchr(message, '\n') != NULL))
    {
        return 0U;
    }

    command_length = snprintf(
        bemfa_command,
        sizeof(bemfa_command),
        "cmd=2&uid=%s&topic=%s&msg=%s\r\n",
        BEMFA_UID,
        BEMFA_TOPIC,
        message);

    if ((command_length <= 0) ||
        ((uint16_t)command_length >= sizeof(bemfa_command)))
    {
        return 0U;
    }

    /* 在 BEMFA_StartPayload() 覆盖命令缓冲区前，先保存实际发布报文。 */
    strcpy(bemfa_last_publish_command, bemfa_command);
    {
        uint16_t last_length = (uint16_t)strlen(bemfa_last_publish_command);

        /* 接收的 +IPD 载荷会去掉行尾，因此回显记录也去掉 \r\n。 */
        if ((last_length >= 2U) &&
            (bemfa_last_publish_command[last_length - 2U] == '\r') &&
            (bemfa_last_publish_command[last_length - 1U] == '\n'))
        {
            bemfa_last_publish_command[last_length - 2U] = '\0';
        }
    }

    /* 实际发送仍使用带有协议行尾的原始报文。 */
    if (!BEMFA_StartPayload(bemfa_command, BEMFA_PAYLOAD_PUBLISH))
    {
        bemfa_last_publish_command[0] = '\0';
        return 0U;
    }

    bemfa_publish_echo_pending = 1U;
    bemfa_publish_echo_deadline = HAL_GetTick() + 5000U;

    return 1U;
}

/**
 * @brief  提交一条巴法云心跳
 * @param  无
 * @retval 1 已发送或已加入等待队列；0 当前未连接
 * @note   如果发布事务正在执行，只保存一个心跳标志，不缓存多条心跳。
 */
uint8_t BEMFA_SendHeartbeat(void)
{
    if (!bemfa_connected)
    {
        return 0U;
    }

    if (bemfa_state != BEMFA_STATE_ONLINE)
    {
        bemfa_heartbeat_pending = 1U;
        return 1U;
    }

    return BEMFA_StartPayload("ping\r\n", BEMFA_PAYLOAD_HEARTBEAT);
}

/**
 * @brief  判断巴法云是否已经连接并完成订阅
 * @param  无
 * @retval 1 在线；0 离线或正在连接
 */
uint8_t BEMFA_IsConnected(void)
{
    return bemfa_connected;
}

/**
 * @brief  通知巴法云模块 Wi-Fi 已断开
 * @param  无
 * @retval 无
 */
void BEMFA_OnWiFiDisconnected(void)
{
    BEMFA_SetOffline(0U);
}

/**
 * @brief  读取一条已经解析完成的巴法云下发消息
 * @param  message 用于接收消息的缓冲区
 * @param  message_size 缓冲区容量
 * @retval 1 读取成功；0 当前没有完整消息或参数无效
 * @note   本函数只读取软件缓冲区，不会等待 UART 硬件。
 */
uint8_t BEMFA_ReadMessage(char *message, uint16_t message_size)
{
    if ((message == NULL) || (message_size == 0U) || !bemfa_message_ready)
    {
        return 0U;
    }

    strncpy(message, bemfa_message, message_size - 1U);
    message[message_size - 1U] = '\0';
    bemfa_message_ready = 0U;

    return 1U;
}
