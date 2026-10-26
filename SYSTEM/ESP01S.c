/**
  ******************************************************************************
  * @file    ESP01S.c
  * @brief   ESP8266（ESP-01S）串口驱动程序
  * @note    串口接收使用单字节中断，收到的数据先保存到环形缓冲区，
  *          再由主循环读取。AT 命令的解析不放在中断中执行。
  ******************************************************************************
  */

#include "ESP01S.h"
#include "main.h"
#include "BtConfig.h"

#include <stdio.h>
#include <string.h>

/*
 * huart3 由 CubeMX 在 main.c 中定义。
 * 这里使用 extern 表示使用其他文件中的串口变量。
 */
extern UART_HandleTypeDef huart3;

/* 当前正在接收的一个字节，由 USART3 接收中断使用。 */
static uint8_t receive_byte;

/* ESP-01S 接收环形缓冲区：中断写入，主循环读取。 */
static uint8_t esp01s_rx_buffer[ESP01S_RX_BUFFER_SIZE];
static volatile uint16_t esp01s_rx_write_index;
static volatile uint16_t esp01s_rx_read_index;
static volatile uint8_t esp01s_rx_overflow;

/* Wi-Fi 状态机的几个步骤，每次只执行一个很短的动作。 */
typedef enum
{
    ESP01S_WIFI_IDLE = 0,
    ESP01S_WIFI_TEST_SEND,
    ESP01S_WIFI_TEST_WAIT,
    ESP01S_WIFI_MODE_SEND,
    ESP01S_WIFI_MODE_WAIT,
    ESP01S_WIFI_JOIN_SEND,
    ESP01S_WIFI_JOIN_WAIT,
    ESP01S_WIFI_RECONNECT_WAIT
} ESP01S_WiFiState;

/**
 * @brief  ESP-01S 单次命令的最大长度
 */
#define ESP01S_COMMAND_MAX_LENGTH    192

/**
 * @brief  ESP-01S 接收回复的最大长度
 */
#define ESP01S_RESPONSE_MAX_LENGTH   128
#define ESP01S_AT_TIMEOUT_MS         2000U
#define ESP01S_JOIN_TIMEOUT_MS       20000U
#define ESP01S_RECONNECT_DELAY_MS    5000U

/* Wi-Fi 配置只保存在 RAM 中，成功后由上层决定是否写入 AT24C02。 */
static char esp01s_ssid[BT_CONFIG_SSID_MAX_LENGTH + 1U];        /* Wi-Fi 热点名称，由 ESP01S_RequestWiFi 写入 */
static char esp01s_password[BT_CONFIG_PASSWORD_MAX_LENGTH + 1U]; /* Wi-Fi 密码，由 ESP01S_RequestWiFi 写入 */
static char esp01s_command[ESP01S_COMMAND_MAX_LENGTH];           /* AT 命令拼装缓冲区，用于组装 AT+CWJAP 等指令 */

static ESP01S_WiFiState esp01s_wifi_state;          /* Wi-Fi 连接状态机当前步骤 */
static uint32_t esp01s_wifi_deadline;               /* 当前等待步骤的超时截止时刻（ms） */
static uint8_t esp01s_wifi_connected;               /* Wi-Fi 是否已连接成功标志，1=已连接 */
static uint8_t esp01s_wifi_report_result;           /* 是否需要向上层报告连接结果，1=需要 */
static volatile uint8_t esp01s_wifi_result_ready;   /* 连接结果是否已就绪可供上层读取，1=就绪 */
static volatile uint8_t esp01s_wifi_result_success; /* 连接结果是否成功（result_ready 为 1 时有效） */

/* 当前一条 AT 回复中的关键信息。 */
static char esp01s_response[ESP01S_RESPONSE_MAX_LENGTH]; /* AT 回复行缓冲区，逐字节接收至遇到 \r\n */
static uint16_t esp01s_response_length;                  /* 当前回复行已接收的字节数 */
static uint8_t esp01s_response_ok;                       /* 回复包含 "OK" 或 "ready" 标志 */
static uint8_t esp01s_response_error;                    /* 回复包含 "ERROR"/"FAIL"/"busy" 标志 */
static uint8_t esp01s_wifi_got_ip;                       /* 回复包含 "WIFI GOT IP" 标志 */

/**
 * @brief  启动 ESP-01S 串口接收
 * @param  无
 * @retval 无
 * @note   需要在 USART3 初始化完成后调用一次。
 */
void ESP01S_StartReceive(void)
{
    esp01s_rx_write_index = 0U;
    esp01s_rx_read_index = 0U;
    esp01s_rx_overflow = 0U;

    esp01s_ssid[0] = '\0';
    esp01s_password[0] = '\0';
    esp01s_wifi_state = ESP01S_WIFI_IDLE;
    esp01s_wifi_connected = 0U;
    esp01s_wifi_report_result = 0U;
    esp01s_wifi_result_ready = 0U;
    esp01s_wifi_result_success = 0U;
    esp01s_response_length = 0U;
    esp01s_response_ok = 0U;
    esp01s_response_error = 0U;
    esp01s_wifi_got_ip = 0U;

    /* 启动第一次单字节中断接收，后续接收由回调函数自动续接。 */
    HAL_UART_Receive_IT(&huart3, &receive_byte, 1U);
}

/**
 * @brief  处理 ESP-01S 单字节接收完成事件
 * @param  无
 * @retval 无
 * @note   本函数由统一串口回调调用，只做入队和重新启动接收。
 */
void ESP01S_RxCpltCallback(void)
{
    uint16_t next_write_index;

    next_write_index = (uint16_t)(esp01s_rx_write_index + 1U);
    if (next_write_index >= ESP01S_RX_BUFFER_SIZE)
    {
        next_write_index = 0U;
    }

    /* 缓冲区已满时丢弃新字节，避免覆盖主循环尚未读取的数据。 */
    if (next_write_index == esp01s_rx_read_index)
    {
        esp01s_rx_overflow = 1U;
    }
    else
    {
        esp01s_rx_buffer[esp01s_rx_write_index] = receive_byte;
        esp01s_rx_write_index = next_write_index;
    }

    /* 单字节接收完成后必须重新启动下一次中断接收。 */
    HAL_UART_Receive_IT(&huart3, &receive_byte, 1U);
}

/**
  * @brief  从环形缓冲区读取一个字节
  * @param  byte  输出参数，存放读取到的字节
  * @retval 1 读取成功
  * @retval 0 缓冲区为空或参数无效
  */
uint8_t ESP01S_ReadByte(uint8_t *byte)
{
    if (byte == 0)                          /* 空指针保护 */
    {
        return 0U;
    }

    if (esp01s_rx_read_index == esp01s_rx_write_index)  /* 缓冲区为空 */
    {
        return 0U;
    }

    *byte = esp01s_rx_buffer[esp01s_rx_read_index];     /* 取出当前读指针处的字节 */
    esp01s_rx_read_index++;
    if (esp01s_rx_read_index >= ESP01S_RX_BUFFER_SIZE)  /* 读指针回绕 */
    {
        esp01s_rx_read_index = 0U;
    }

    return 1U;
}

/**
 * @brief  查询 ESP-01S 接收缓冲区是否发生溢出
 * @param  无
 * @retval 1 已发生溢出；0 未发生溢出
 */
uint8_t ESP01S_IsRxOverflow(void)
{
    return esp01s_rx_overflow;
}

/**
 * @brief  清除 ESP-01S 接收缓冲区溢出标志
 * @param  无
 * @retval 无
 */
void ESP01S_ClearRxOverflow(void)
{
    esp01s_rx_overflow = 0U;
}

/**
 * @brief  检查字符串是否可以安全放入 AT 指令
 * @param  text 待检查字符串
 * @param  max_length 允许的最大长度
 * @retval 1 合法；0 非法
 */
static uint8_t ESP01S_IsSafeText(const char *text, uint16_t max_length)
{
    uint16_t i;
    uint16_t length;

    if (text == 0)
    {
        return 0U;
    }

    length = (uint16_t)strlen(text);
    if ((length == 0U) || (length > max_length))
    {
        return 0U;
    }

    for (i = 0U; i < length; i++)
    {
        if ((text[i] == '"') || (text[i] == '\\') ||
            (text[i] == '\r') || (text[i] == '\n'))
        {
            return 0U;
        }
    }

    return 1U;
}

/**
 * @brief  判断状态机是否已经超时
 * @param  now 当前系统滴答
 * @param  deadline 截止时间
 * @retval 1 已超时；0 未超时
 */
static uint8_t ESP01S_IsDeadlineReached(uint32_t now, uint32_t deadline)
{
    return (uint8_t)((int32_t)(now - deadline) >= 0);
}

/**
 * @brief  清除本次 AT 命令的回复标志
 * @param  无
 * @retval 无
 */
static void ESP01S_ClearWiFiResponse(void)
{
    esp01s_response_length = 0U;
    esp01s_response_ok = 0U;
    esp01s_response_error = 0U;
    esp01s_wifi_got_ip = 0U;
}

/**
 * @brief  处理一行 ESP8266 回复
 * @param  无
 * @retval 无
 */
static void ESP01S_ProcessWiFiLine(void)
{
    esp01s_response[esp01s_response_length] = '\0';

    if (esp01s_response_length == 0U)
    {
        return;
    }

    if ((strcmp(esp01s_response, "OK") == 0) ||
        (strstr(esp01s_response, "ready") != 0))
    {
        esp01s_response_ok = 1U;
    }

    if ((strstr(esp01s_response, "ERROR") != 0) ||
        (strstr(esp01s_response, "FAIL") != 0) ||
        (strstr(esp01s_response, "busy") != 0))
    {
        esp01s_response_error = 1U;
    }

    if (strstr(esp01s_response, "WIFI GOT IP") != 0)
    {
        esp01s_wifi_got_ip = 1U;
    }
}

/**
 * @brief  从环形缓冲区读取 Wi-Fi 连接阶段的文本回复
 * @param  无
 * @retval 无
 * @note   只在 Wi-Fi 状态机等待 AT 回复时调用，在线后的 TCP 数据由 BEMFA 模块读取。
 */
static void ESP01S_ProcessWiFiResponse(void)
{
    uint8_t byte;

    while (ESP01S_ReadByte(&byte))
    {
        if (byte == '\r')
        {
            continue;
        }

        if (byte == '\n')
        {
            ESP01S_ProcessWiFiLine();
            esp01s_response_length = 0U;
        }
        else if (esp01s_response_length < ESP01S_RESPONSE_MAX_LENGTH - 1U)
        {
            esp01s_response[esp01s_response_length++] = (char)byte;
        }
        else
        {
            /* 回复行过长时丢弃当前行，避免写出缓冲区。 */
            esp01s_response_length = 0U;
        }
    }
}

/**
 * @brief  结束一次 Wi-Fi 请求并保存结果
 * @param  success 连接是否成功
 * @retval 无
 */
static void ESP01S_FinishWiFi(uint8_t success)
{
    esp01s_wifi_connected = success;
    esp01s_wifi_state = ESP01S_WIFI_IDLE;

    if (esp01s_wifi_report_result)
    {
        esp01s_wifi_result_success = success;
        esp01s_wifi_result_ready = 1U;
        esp01s_wifi_report_result = 0U;
    }

    /* 连接失败且已保存过凭据时，安排 5 秒后自动重连。 */
    if ((success == 0U) && (esp01s_ssid[0] != '\0'))
    {
        esp01s_wifi_deadline = HAL_GetTick() + ESP01S_RECONNECT_DELAY_MS;
        esp01s_wifi_state = ESP01S_WIFI_RECONNECT_WAIT;
    }
}

/**
 * @brief  提交 Wi-Fi 配置并启动非阻塞连接流程
 * @param  ssid Wi-Fi 名称
 * @param  password Wi-Fi 密码
 * @retval 1 配置已接受；0 参数非法
 */
uint8_t ESP01S_RequestWiFi(const char *ssid, const char *password)
{
    if (!ESP01S_IsSafeText(ssid, BT_CONFIG_SSID_MAX_LENGTH) ||
        !ESP01S_IsSafeText(password, BT_CONFIG_PASSWORD_MAX_LENGTH))
    {
        return 0U;
    }

    strcpy(esp01s_ssid, ssid);
    strcpy(esp01s_password, password);

    esp01s_wifi_connected = 0U;
    esp01s_wifi_report_result = 1U;
    esp01s_wifi_result_ready = 0U;
    esp01s_wifi_state = ESP01S_WIFI_TEST_SEND;

    return 1U;
}

/**
 * @brief  推进 ESP8266 Wi-Fi 非阻塞状态机
 * @param  无
 * @retval 无
 * @note   每次调用只推进一个小步骤，避免主循环长时间停在某条 AT 指令上。
 */
void ESP01S_Service(void)
{
    uint32_t now;
    int command_length;

    now = HAL_GetTick();

    /* 只有等待 AT 回复时才读取软件环形缓冲区。 */
    if ((esp01s_wifi_state == ESP01S_WIFI_TEST_WAIT) ||
        (esp01s_wifi_state == ESP01S_WIFI_MODE_WAIT) ||
        (esp01s_wifi_state == ESP01S_WIFI_JOIN_WAIT))
    {
        ESP01S_ProcessWiFiResponse();
    }

    switch (esp01s_wifi_state)
    {
    case ESP01S_WIFI_IDLE:              /* 空闲，无操作 */
        break;

    case ESP01S_WIFI_TEST_SEND:         /* 步骤1：发送 AT 测试模块通信 */
        ESP01S_ClearWiFiResponse();
        ESP01S_SendString("AT\r\n");
        esp01s_wifi_deadline = now + ESP01S_AT_TIMEOUT_MS;
        esp01s_wifi_state = ESP01S_WIFI_TEST_WAIT;
        break;

    case ESP01S_WIFI_TEST_WAIT:         /* 等待 AT 回复：OK 则进入下一步，错误/超时则失败 */
        if (esp01s_response_error ||
            ESP01S_IsDeadlineReached(now, esp01s_wifi_deadline))
        {
            ESP01S_FinishWiFi(0U);
        }
        else if (esp01s_response_ok)
        {
            esp01s_wifi_state = ESP01S_WIFI_MODE_SEND;
        }
        break;

    case ESP01S_WIFI_MODE_SEND:         /* 步骤2：设置 Station 模式 */
        ESP01S_ClearWiFiResponse();
        ESP01S_SendString("AT+CWMODE=1\r\n");
        esp01s_wifi_deadline = now + ESP01S_AT_TIMEOUT_MS;
        esp01s_wifi_state = ESP01S_WIFI_MODE_WAIT;
        break;

    case ESP01S_WIFI_MODE_WAIT:         /* 等待 CWMODE 回复：OK 则进入下一步，错误/超时则失败 */
        if (esp01s_response_error ||
            ESP01S_IsDeadlineReached(now, esp01s_wifi_deadline))
        {
            ESP01S_FinishWiFi(0U);
        }
        else if (esp01s_response_ok)
        {
            esp01s_wifi_state = ESP01S_WIFI_JOIN_SEND;
        }
        break;

    case ESP01S_WIFI_JOIN_SEND:         /* 步骤3：发送 AT+CWJAP 加入热点 */
        command_length = snprintf(
            esp01s_command,
            sizeof(esp01s_command),
            "AT+CWJAP=\"%s\",\"%s\"\r\n",
            esp01s_ssid,
            esp01s_password);

        if ((command_length <= 0) ||    /* 拼装失败或溢出，直接判定连接失败 */
            ((uint16_t)command_length >= sizeof(esp01s_command)))
        {
            ESP01S_FinishWiFi(0U);
        }
        else
        {
            ESP01S_ClearWiFiResponse();
            ESP01S_SendString(esp01s_command);
            esp01s_wifi_deadline = now + ESP01S_JOIN_TIMEOUT_MS;  /* 入网超时较长(20s) */
            esp01s_wifi_state = ESP01S_WIFI_JOIN_WAIT;
        }
        break;

    case ESP01S_WIFI_JOIN_WAIT:         /* 等待入网回复：OK/GOT IP 则成功，错误/超时则失败 */
        if (esp01s_response_error ||
            ESP01S_IsDeadlineReached(now, esp01s_wifi_deadline))
        {
            ESP01S_FinishWiFi(0U);
        }
        else if (esp01s_response_ok || esp01s_wifi_got_ip)
        {
            ESP01S_FinishWiFi(1U);
        }
        break;

    case ESP01S_WIFI_RECONNECT_WAIT:    /* 断线后等待 5 秒，到期重新发起完整连接序列 */
        if (ESP01S_IsDeadlineReached(now, esp01s_wifi_deadline))
        {
            ESP01S_ClearWiFiResponse();
            esp01s_wifi_state = ESP01S_WIFI_TEST_SEND;
        }
        break;

    default:                            /* 非法状态，复位为失败 */
        ESP01S_FinishWiFi(0U);
        break;
    }
}

/**
 * @brief  查询 ESP8266 是否已经连接 Wi-Fi
 * @param  无
 * @retval 1 已连接；0 未连接或正在连接
 */
uint8_t ESP01S_IsWiFiConnected(void)
{
    return esp01s_wifi_connected;
}

/**
 * @brief  取走一次 Wi-Fi 连接结果
 * @param  success 用于接收结果，1 表示成功，0 表示失败
 * @retval 1 取到新结果；0 当前没有新结果或参数无效
 */
uint8_t ESP01S_TakeWiFiResult(uint8_t *success)
{
    uint8_t ready;

    if (success == 0)
    {
        return 0U;
    }

    ready = esp01s_wifi_result_ready;
    if (ready)
    {
        *success = esp01s_wifi_result_success;
        esp01s_wifi_result_ready = 0U;
    }

    return ready;
}

/**
 * @brief  通知 ESP8266 Wi-Fi 已断开
 * @param  无
 * @retval 无
 */
void ESP01S_NotifyWiFiDisconnected(void)
{
    esp01s_wifi_connected = 0U;

    /* 有已保存的凭据时进入重连等待，5 秒后自动重连；否则保持空闲。 */
    if (esp01s_ssid[0] != '\0')
    {
        esp01s_wifi_deadline = HAL_GetTick() + ESP01S_RECONNECT_DELAY_MS;
        esp01s_wifi_state = ESP01S_WIFI_RECONNECT_WAIT;
    }
    else
    {
        esp01s_wifi_state = ESP01S_WIFI_IDLE;
    }
}

/**
 * @brief  向 ESP-01S 发送字符串
 * @param  text 要发送的字符串，必须以 '\0' 结尾
 * @retval 1 发送成功；0 参数无效或串口发送失败
 */
uint8_t ESP01S_SendString(const char *text)
{
    uint16_t length;

    if (text == 0)
    {
        return 0U;
    }

    length = (uint16_t)strlen(text);

    if (length == 0U)
    {
        return 0U;
    }

    return (HAL_UART_Transmit(
        &huart3,
        (uint8_t *)text,
        length,
        1000
    ) == HAL_OK) ? 1U : 0U;
}

/*
 * 旧版阻塞接口已被新的非阻塞状态机替代。
 * 暂时保留在条件编译代码中，便于后续查阅历史逻辑，但不会参与编译。
 */
#if 0
/**
 * @brief  等待 ESP-01S 返回指定内容
 * @param  expected 需要等待的字符串，例如 "OK" 或 ">"
 * @param  timeout 等待时间，单位为毫秒
 * @retval 1 表示收到指定内容
 * @retval 0 表示超时或收到错误
 */
uint8_t ESP01S_WaitResponse(const char *expected, uint32_t timeout)
{
    uint8_t receive_byte;
    char response[ESP01S_RESPONSE_MAX_LENGTH];
    uint16_t response_index = 0;
    uint32_t start_time;

    /* 期望字符串指针为空时直接返回失败，防止空指针解引用 */
    if (expected == 0)
    {
        return 0;
    }

    /*
     * 将回复缓冲区清零，方便使用 strstr 查找字符串。
     */
    memset(response, 0, sizeof(response));

    /* 记录当前系统滴答时间，作为超时计算的起点 */
    start_time = HAL_GetTick();

    /* 在总超时时间内循环接收并检查 ESP-01S 的回复 */
    while ((HAL_GetTick() - start_time) < timeout)
    {
        /* 兼容旧接口：数据已经由中断放入环形缓冲区，此处只读取软件缓冲区。 */
        while (ESP01S_ReadByte(&receive_byte))
        {
            /*
             * 将收到的字节存入回复缓冲区，分两种情况处理：
             *
             * 1) 缓冲区未满：直接追加到末尾，更新索引，并补 '\0'。
             *
             * 2) 缓冲区已满：执行滑动窗口策略——
             *    用 memmove 将缓冲区内容整体左移一位（丢弃最早的一个字符），
             *    再将新字节放到倒数第二个位置，末尾保持 '\0'。
             *    这样缓冲区始终保存最近接收的 (ESP01S_RESPONSE_MAX_LENGTH - 1) 个字符，
             *    即使回复很长，仍能在窗口内匹配到期望关键字。
             */
            if (response_index < ESP01S_RESPONSE_MAX_LENGTH - 1)
            {
                response[response_index] = (char)receive_byte;
                response_index++;
                response[response_index] = '\0';
            }
            else
            {
                memmove(
                    response,
                    response + 1,
                    ESP01S_RESPONSE_MAX_LENGTH - 2
                );

                response[ESP01S_RESPONSE_MAX_LENGTH - 2] =
                    (char)receive_byte;

                response[ESP01S_RESPONSE_MAX_LENGTH - 1] = '\0';
            }

            /*
             * 每收到一个字节后，立即在缓冲区中查找期望字符串。
             * 若找到，说明 ESP-01S 已返回预期内容，返回 1 表示成功。
             */
            if (strstr(response, expected) != 0)
            {
                return 1;
            }

            /*
             * 若缓冲区中出现 "ERROR"，说明 ESP-01S 报告命令执行失败，
             * 无需继续等待，直接返回 0。
             */
            if (strstr(response, "ERROR") != 0)
            {
                return 0;
            }
        }

        HAL_Delay(1U);
    }

    /* 总超时时间已到仍未收到期望内容，返回 0 表示超时失败 */
    return 0;
}

/**
 * @brief  连接 Wi-Fi
 * @param  ssid Wi-Fi 名称
 * @param  password Wi-Fi 密码
 * @retval 1 表示连接成功
 * @retval 0 表示连接失败
 */
uint8_t ESP01S_ConnectWiFi(const char *ssid, const char *password)
{
    char command[ESP01S_COMMAND_MAX_LENGTH];
    int command_length;

    if (ssid == 0 || password == 0)
    {
        return 0;
    }

    /*
     * 先测试 ESP-01S 是否正常工作。
     */
    ESP01S_SendString("AT\r\n");

    if (ESP01S_WaitResponse("OK", 1000) == 0)
    {
        return 0;
    }
    else
    {
        printf("ESP-01S 正常工作\r\n");
    }

    /*
     * 设置 ESP-01S 为 Station 模式，
     * 也就是连接路由器的模式。
     */
    ESP01S_SendString("AT+CWMODE=1\r\n");

    if (ESP01S_WaitResponse("OK", 1000) == 0)
    {
        return 0;
    }
    else
    {
        printf("ESP-01S 设置为 Station 模式成功\r\n");
    }

    /*
     * 生成连接 Wi-Fi 的 AT 命令。
     */
    command_length = snprintf(
        command,
        sizeof(command),
        "AT+CWJAP=\"%s\",\"%s\"\r\n",
        ssid,
        password
    );

    /*
     * snprintf 返回负数或超出缓冲区容量时，
     * 说明 Wi-Fi 信息太长。
     */
    if (command_length < 0 ||
        command_length >= (int)sizeof(command))
    {
        return 0;
    }

    ESP01S_SendString(command);

    /*
     * 连接 Wi-Fi 可能需要较长时间，
     * 这里最多等待 15 秒。
     */
    if (ESP01S_WaitResponse("OK", 15000) == 0)
    {
        return 0;
    }
    else
    {
        printf("ESP-01S 连接 Wi-Fi : %s 成功\r\n", ssid);
    }

    return 1;
}

/**
 * @brief  连接 TCP 服务器
 * @param  server_ip TCP 服务器 IP 地址或域名
 * @param  server_port TCP 服务器端口
 * @retval 1 表示连接成功
 * @retval 0 表示连接失败
 */
uint8_t ESP01S_ConnectTCP(const char *server_ip, uint16_t server_port)
{
    char command[ESP01S_COMMAND_MAX_LENGTH];
    int command_length;

    if (server_ip == 0)
    {
        printf("ESP-01S TCP连接失败：服务器IP为空\r\n");
        return 0;
    }

    /*
     * 设置为普通 TCP 透传前的单连接模式。
     */
    ESP01S_SendString("AT+CIPMUX=0\r\n");

    if (ESP01S_WaitResponse("OK", 1000) == 0)
    {
        printf("ESP-01S 设置单连接模式失败\r\n");
        return 0;
    }

    /*
     * 设置为普通 TCP 模式。
     */
    ESP01S_SendString("AT+CIPMODE=0\r\n");

    if (ESP01S_WaitResponse("OK", 1000) == 0)
    {
        printf("ESP-01S 设置普通TCP模式失败\r\n");
        return 0;
    }

        /*
     * 统一 ESP-01S 的网络数据接收格式为：
     * +IPD,<数据长度>:<数据内容>
     * BEMFA.c 按这个格式解析巴法云下发的数据。
     */
    ESP01S_SendString("AT+CIPDINFO=0\r\n");

    if (ESP01S_WaitResponse("OK", 1000) == 0)
    {
        printf("ESP-01S 无法设置 +IPD 接收格式\r\n");
        return 0;
    }

    /*
     * 生成连接 TCP 服务器的 AT 命令。
     */
    command_length = snprintf(
        command,
        sizeof(command),
        "AT+CIPSTART=\"TCP\",\"%s\",%u\r\n",
        server_ip,
        server_port
    );

    if (command_length < 0 ||
        command_length >= (int)sizeof(command))
    {
        printf("ESP-01S TCP连接命令过长，缓冲区溢出\r\n");
        return 0;
    }

    ESP01S_SendString(command);

    /*
     * ESP-01S 成功连接后通常会返回 CONNECT 和 OK。
     */
    if (ESP01S_WaitResponse("OK", 10000) == 0)
    {
        printf("ESP-01S 连接TCP服务器失败\r\n");
        return 0;
    }

    return 1;
}

/**
 * @brief  通过 TCP 连接发送字符串
 * @param  text 要发送的字符串，必须以 '\0' 结尾
 * @retval 1 表示发送成功
 * @retval 0 表示发送失败
 */
uint8_t ESP01S_SendTCP(const char *text)
{
    char command[32];
    uint16_t text_length;
    int command_length;

    if (text == 0)
    {
        return 0;
    }

    text_length = (uint16_t)strlen(text);

    /*
     * 生成发送数据长度命令。
     */
    command_length = snprintf(
        command,
        sizeof(command),
        "AT+CIPSEND=%u\r\n",
        text_length
    );

    if (command_length < 0 ||
        command_length >= (int)sizeof(command))
    {
        return 0;
    }

    ESP01S_SendString(command);

    /*
     * 收到 '>' 后，ESP-01S 才允许发送真正的数据。
     */
    if (ESP01S_WaitResponse(">", 3000) == 0)
    {
        return 0;
    }

    ESP01S_SendString(text);

    /*
     * 等待 ESP-01S 返回发送成功。
     */
    if (ESP01S_WaitResponse("SEND OK", 5000) == 0)
    {
        return 0;
    }

    return 1;
}
#endif
