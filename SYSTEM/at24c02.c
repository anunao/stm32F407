#include "at24c02.h"

/*
 * AT24C02 (2Kbit I2C EEPROM) 驱动 —— CubeMX 版
 *
 * 与旧版的区别：
 *   1. 不再定义 I2C_HandleTypeDef hi2c1，改用 main.c 中 MX 生成的句柄
 *   2. 不再配置 GPIO / I2C 时钟，这些由 MX_I2C1_Init() + HAL_I2C_MspInit() 完成
 *   3. 不再依赖 delay.h，改用 HAL_Delay()（基于 SysTick）
 *   4. 所有等待都带超时，硬件异常时不会死循环
 *   5. 多字节写改为"按 8 字节对齐的页写"，比逐字节写快约 8 倍
 *   6. HAL 传输超时由 HAL_MAX_DELAY 改为有限值，防止总线挂死
 */

/* 需要串口打印调试信息时置 1（工程需自行重定向 printf） */
#ifndef AT24C02_LOG_EN
#define AT24C02_LOG_EN   1
#endif
#if AT24C02_LOG_EN
#include <stdio.h>
#define AT24C02_LOG(...)   printf(__VA_ARGS__)
#else
#define AT24C02_LOG(...)   ((void)0)
#endif

/* 传输/等待超时：tWR 最大 5ms，给 100ms 余量足够 */
#define AT24C02_TIMEOUT    100u

extern I2C_HandleTypeDef hi2c1;



/**
 * @brief  器件探测（I2C 外设已由 MX 初始化，这里只确认芯片在线并空闲）
 * @retval 0 成功；-1 无应答（检查接线/上拉/地址/WP）
 */
int at24c02_init(void)
{
    if (HAL_I2C_IsDeviceReady(&hi2c1, AT24C02_ADDR_WRITE, 3, 100) != HAL_OK)
    {
        AT24C02_LOG("AT24C02: no ack\r\n");
        return -1;
    }
    else
    {
        AT24C02_LOG("AT24C02: ack online\r\n");
    }
    return 0;
}

/**
 * @brief  器件是否处于内部写周期 tWR
 * @retval 1 = 忙（无应答，还在写）；0 = 空闲
 */
int at24c02_is_busy(void)
{
    if (HAL_I2C_IsDeviceReady(&hi2c1, AT24C02_ADDR_WRITE, 1, 10) == HAL_OK)
        return 0;
    return 1;
}

/**
 * @brief  等待写周期结束（ACK 轮询），带超时
 * @retval 0 已空闲；-1 超时
 */
int at24c02_wait_idle(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while (at24c02_is_busy())
    {
        if (HAL_GetTick() - start > timeout_ms)
        {
            AT24C02_LOG("AT24C02: wait idle timeout\r\n");
            return -1;
        }
        HAL_Delay(1);
    }
    return 0;
}

/* 兼容旧函数名：写操作之后调用 */
void wait_at24c02_finished_to_write(void)
{
    (void)at24c02_wait_idle(AT24C02_TIMEOUT);
}

/**
 * @brief  写一个字节
 * @param  addr 内部地址 0~255
 * @param  data 要写入的数据
 * @retval 0 成功；-1 失败
 */
int write_a_byte_to_at24c02(uint8_t addr, uint8_t data)
{
    if (at24c02_wait_idle(AT24C02_TIMEOUT) != 0)
        return -1;

    if (HAL_I2C_Mem_Write(&hi2c1, AT24C02_ADDR_WRITE, addr,
                          I2C_MEMADD_SIZE_8BIT, &data, 1,
                          AT24C02_TIMEOUT) != HAL_OK)
    {
        AT24C02_LOG("AT24C02: write byte fail\r\n");
        return -1;
    }

    wait_at24c02_finished_to_write();
    return 0;
}

/**
 * @brief  写多个字节（自动处理 8 字节页边界，规避页写回卷）
 * @param  addr 起始地址 0~255
 * @param  data 数据缓冲区
 * @param  size 长度（addr + size <= 256）
 * @retval 实际写入字节数；-1 参数错误或通信失败
 */
int write_bytes_to_at24c02(uint8_t addr, uint8_t *data, uint16_t size)
{
    uint16_t written = 0;

    if (data == NULL || size == 0)
        return -1;
    if ((uint16_t)addr + size > AT24C02_SIZE)
    {
        AT24C02_LOG("AT24C02: out of range\r\n");
        return -1;
    }

    while (written < size)
    {
        uint8_t  cur  = (uint8_t)(addr + written);
        uint16_t left = AT24C02_PAGE_SIZE - (cur & (AT24C02_PAGE_SIZE - 1));
        uint16_t n    = ((size - written) < left) ? (size - written) : left;

        if (at24c02_wait_idle(AT24C02_TIMEOUT) != 0)
            return -1;

        if (HAL_I2C_Mem_Write(&hi2c1, AT24C02_ADDR_WRITE, cur,
                              I2C_MEMADD_SIZE_8BIT, &data[written], n,
                              AT24C02_TIMEOUT) != HAL_OK)
        {
            AT24C02_LOG("AT24C02: page write fail\r\n");
            return -1;
        }

        wait_at24c02_finished_to_write();   /* 一次页写只等一次 tWR */
        written += n;
    }

    return (int)written;
}

/**
 * @brief  读多个字节（HAL 内部自动完成"假写地址 + 重起始 + 顺序读"）
 * @param  addr 起始地址 0~255
 * @param  data 数据缓冲区
 * @param  size 长度（addr + size <= 256）
 * @retval 实际读取字节数；-1 失败
 */
int read_bytes_from_at24c02(uint8_t addr, uint8_t *data, uint16_t size)
{
    if (data == NULL || size == 0)
        return -1;
    if ((uint16_t)addr + size > AT24C02_SIZE)
        return -1;

    if (at24c02_wait_idle(AT24C02_TIMEOUT) != 0)
        return -1;

    if (HAL_I2C_Mem_Read(&hi2c1, AT24C02_ADDR_WRITE, addr,
                         I2C_MEMADD_SIZE_8BIT, data, size,
                         AT24C02_TIMEOUT) != HAL_OK)
    {
        AT24C02_LOG("AT24C02: read fail\r\n");
        return -1;
    }

    return (int)size;
}
