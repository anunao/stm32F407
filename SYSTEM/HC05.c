/**
  ******************************************************************************
  * @file    HC05.c
  * @brief   HC-05 蓝牙串口驱动程序
  * @note    HC-05 使用 USART2，通过中断逐字节接收数据。
  *          接收到换行符后，表示一整行数据接收完成。
  ******************************************************************************
  */

#include "HC05.h"
#include "main.h"
#include <string.h>
#include <stdio.h>

/*
 * huart2 由 CubeMX 在 main.c 中定义，
 * 这里使用 extern 表示使用其他文件中的串口变量。
 */
extern UART_HandleTypeDef huart2;

/* 当前正在接收的一个字节 */
static uint8_t receive_byte;

/* 保存一整行接收数据的缓冲区 */
static char receive_line[HC05_LINE_MAX_LENGTH];

/* 当前已经接收的字符数量 */
static volatile uint16_t receive_index;

/* 1 表示已经接收到一整行数据 */
static volatile uint8_t line_ready;

/* 1 表示当前数据超过了缓冲区长度 */
static volatile uint8_t line_overflow;

/**
 * @brief  启动 HC-05 串口接收
 * @param  无
 * @retval 无
 */
void HC05_StartReceive(void)
{
    receive_index = 0;          // 初始化接收索引
    line_ready = 0;             // 初始化行接收标志位
    line_overflow = 0;          // 初始化溢出标志位

    /*
     * 启动 USART2 的单字节中断接收。
     * 接收到一个字节后，会自动进入 HAL_UART_RxCpltCallback。
     */
    HAL_UART_Receive_IT(&huart2, &receive_byte, 1);
}

/**
 * @brief  向 HC-05 发送字符串
 * @param  text 要发送的字符串，必须以 '\0' 结尾
 * @retval 无
 */
void HC05_SendString(const char *text)
{
    if (text == 0)
    {
        return;
    }

    HAL_UART_Transmit(
        &huart2,
        (uint8_t *)text,
        (uint16_t)strlen(text),
        1000
    );
}

/**
 * @brief  读取 HC-05 接收到的一整行数据
 * @param  line 用于保存接收内容的数组
 * @param  line_size 数组的最大容量
 * @retval 1 表示成功读取到一整行
 * @retval 0 表示当前还没有完整数据
 */
uint8_t HC05_ReadLine(char *line, uint16_t line_size)
{
    uint16_t copy_index;

    /* 参数有效性检查：目标数组指针为空或容量为 0 时直接返回失败 */
    if (line == 0 || line_size == 0)
    {
        return 0;
    }

    /* 状态检查：当前没有完整的一行数据可读，直接返回失败 */
    if (line_ready == 0)
    {
        return 0;
    }

    /*
     * 暂时关闭中断，防止复制数据时接收中断同时修改缓冲区。
     */
    __disable_irq();

    copy_index = 0;

    /*
     * 逐字节将 receive_line 中的内容复制到 line 中：
     *   - copy_index < line_size - 1    ：不超出目标数组容量，预留 '\0' 的位置
     *   - copy_index < HC05_LINE_MAX_LENGTH - 1：不超出源缓冲区最大长度
     *   - receive_line[copy_index] != '\0'：遇到字符串结束符即停止复制
     */
    while ((copy_index < line_size - 1) &&
           (copy_index < HC05_LINE_MAX_LENGTH - 1) &&
           (receive_line[copy_index] != '\0'))
    {
        line[copy_index] = receive_line[copy_index];
        copy_index++;
    }

    /* 在复制内容的末尾添加字符串结束符，确保 line 为合法的 C 字符串 */
    line[copy_index] = '\0';

    /*
     * 清除行就绪标志，允许接收中断向缓冲区写入下一行数据。
     * 必须在中断关闭期间修改，否则可能产生竞态条件。
     */
    line_ready = 0;

    /* 恢复中断，此后 UART 接收中断可继续向缓冲区写入新数据 */
    __enable_irq();

    /* 返回 1 表示成功读取到一整行数据 */
    return 1;
}

/**
 * @brief  处理 HC-05 单字节接收完成事件
 * @param  无
 * @retval 无
 * @note   统一串口回调位于 stm32f4xx_it.c，本函数只处理 USART2 的数据。
 *         整体逻辑为逐字节接收的状态机：
 *         - line_ready == 0：上一行已读取，可以保存新数据
 *         - line_ready == 1：上一行尚未读取，丢弃新数据
 *         收到换行符 '\n' 时，一行数据接收完成；
 *         回车符 '\r' 被忽略，用于兼容 "\r\n" 换行格式；
 *         超出缓冲区长度时置 line_overflow 标志，丢弃当前行。
 */
void HC05_RxCpltCallback(void)
{
    /* 串口回调已经确认是 USART2，这里只处理 HC-05 的接收字节。 */
        /*
         * 流控保护：只有当主循环已读取上一行数据（line_ready == 0）时，
         * 才继续向缓冲区保存新字节，避免覆盖尚未读取的数据。
         */
        if (line_ready == 0)
        {
            if (receive_byte == '\n')
            {
                /*
                 * 收到换行符，一整行数据接收完成。
                 * 若未发生溢出，在缓冲区末尾添加字符串结束符 '\0'，
                 * 并置 line_ready = 1 通知主循环可以读取。
                 * 若曾发生溢出，则丢弃当前行，不通知主循环。
                 */
                if (line_overflow == 0)
                {
                    receive_line[receive_index] = '\0';
                    line_ready = 1;
                }

                /* 重置索引和溢出标志，为接收下一行数据做准备 */
                receive_index = 0;
                line_overflow = 0;
            }
            else if (receive_byte != '\r')
            {
                /*
                 * 忽略回车符 '\r'，仅保存普通字符。
                 * 这样可以兼容 "\r\n"（Windows）和 "\n"（Linux）换行格式。
                 */
                if (receive_index < HC05_LINE_MAX_LENGTH - 1)
                {
                    /* 缓冲区未满，将字节存入缓冲区并推进索引 */
                    receive_line[receive_index] = (char)receive_byte;
                    receive_index++;
                }
                else
                {
                    /*
                     * 缓冲区已满，置溢出标志丢弃当前行。
                     * 后续字节不再保存，直到收到换行符后
                     * 重置状态，重新开始接收下一行。
                     */
                    line_overflow = 1;
                }
            }
        }

        /*
         * 重新启动单字节中断接收，确保持续接收后续数据。
         * 若不调用此函数，UART 中断接收将停止。
         */
        HAL_UART_Receive_IT(&huart2, &receive_byte, 1);
}
