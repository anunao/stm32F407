/**
 * @file    Oled.c
 * @brief   GPIO 软件模拟 I2C 的 SSD1306 (128x64) OLED 驱动（STM32F4）
 * @version V6.0.0（软件模拟 I2C 版）
 *
 * 【为什么要从「硬件 I2C1」改成「软件模拟 I2C」】
 *   硬件 I2C1 上还挂着 AT24C02（EEPROM，存 Wi-Fi 配置，地址 0xA0）。
 *   两个设备挤在同一个 hi2c1 句柄上会互相拖累：
 *     - 任一方传输异常锁死 BUSY 位，另一方跟着一起死；
 *     - 若将来一方改 _IT 中断模式、另一方用阻塞，必然 HAL_BUSY 状态机错乱。
 *   现在 OLED 独占 PC6/PC7 走软件 I2C，与 AT24C02 的硬件 I2C1 **物理隔离**，
 *   互不干涉，也省掉了「AT24C02 必须接到 PB8/PB9」这个硬件约束。
 *
 * 【本版相对 V5.0.0（硬件 I2C1 版）的变化】
 *   1) 底层传输全部重写：HAL_I2C_Master_Transmit -> 位操作翻转 GPIO；
 *   2) 不再依赖 I2C1 外设 / hi2c1 句柄 / NVIC，也不再需要外设级
 *      DeInit + 复位（引脚本就是 GPIO，解锁时直接打脉冲即可）；
 *   3) 不再需要 129 字节发送暂存 buffer（直接逐字节从源发送），省 RAM；
 *   4) 上层显示 API（ShowChar / DrawBMP / DrawFont16/32 / DrawPoint 等）
 *      **完全不变**，调用方零改动。
 *
 * @note 硬件：SCL = PC6，SDA = PC7，均配为 GPIO_MODE_OUTPUT_OD（开漏）+ 内部上拉。
 *       开漏是**必需**的：主机输出 1 等于「释放总线」，此时读 IDR 才能拿到
 *       从机拉低的 ACK；若用推挽，主机强行拉高会与从机的 ACK 拉低打架。
 * @note 时序：由 SW_I2C_Delay() 控制，默认约 300~500 kHz（SSD1306 上限 400 kHz）。
 *       若屏幕花屏/无显示，把 SW_I2C_DLY_CNT 调大（降速）重试。
 * @note 屏幕：128 x 64，页地址模式，8 页 x 128 列
 * @note 坐标：x ∈ [0,127] 列；y ∈ [0,7] 页（每页 8 像素高）
 * @note 错误：写命令/数据时收不到 ACK 一律 Error_Handler()；
 *       另有 OLED_IsReady() 供「查询但不死机」的场合使用。
 */

#include "main.h"                 /* stm32f4xx_hal.h + Error_Handler 原型 */
#include "Oled.h"
#include "oledFonts.h"      /* ASCII_1608 / Hzk / Tfont16 / Tfont32 */
#include "bmp.h"            /* BMP1 / BMP2 / BMP_LONG / BMP_GUOQI */
#include <string.h>               /* memset / memcpy */

/*---------------------------------------------------------------------------*/
/*  私有宏：引脚定义                                                           */
/*---------------------------------------------------------------------------*/

/**
 * SCL / SDA 用的 GPIO
 *
 * @note 直接引用 CubeMX 在 main.h 里生成的宏（User Label = IIC_SOFT_SCL / IIC_SOFT_SDA）。
 *       换引脚只需在 CubeMX 里改，本文件无需任何改动。
 * @warning 若在 CubeMX 里删掉这两个引脚或改了 User Label，这里会**编译报错** ——
 *          按新的宏名同步改一下即可。宁可报错，也不要静默用错引脚。
 */
#define OLED_SCL_PORT    IIC_SOFT_SCL_GPIO_Port
#define OLED_SCL_PIN     IIC_SOFT_SCL_Pin      /* 目标 PC6 */
#define OLED_SDA_PORT    IIC_SOFT_SDA_GPIO_Port
#define OLED_SDA_PIN     IIC_SOFT_SDA_Pin      /* 目标 PC7 */

/*---------------------------------------------------------------------------*/
/*  私有宏：时序与位操作                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief SCL 半周期的忙等循环次数（越大越慢）
 * @details 168 MHz 下每次循环约 4~5 个周期（≈30 ns），取 30 → 半周期约 1 us
 *          → SCL ≈ 500 kHz。实际随编译优化等级浮动，SSD1306 上限 400 kHz，
 *          若屏幕花屏 / 无显示，把它调大（比如 60）降速重试。
 * @warning 计数变量必须 volatile，否则 -O2 会把空循环整个优化掉。
 */
#define SW_I2C_DLY_CNT   30u

/** SCL = 1（BSRR 低 16 位写 1 置位；比 HAL_GPIO_WritePin 快一个数量级） */
#define SCL_H()   (OLED_SCL_PORT->BSRR = (uint32_t)OLED_SCL_PIN)
/** SCL = 0（BSRR 高 16 位写 1 复位） */
#define SCL_L()   (OLED_SCL_PORT->BSRR = ((uint32_t)OLED_SCL_PIN << 16u))
/** SDA = 1 —— 开漏下等价于「释放总线」，之后可安全读 IDR 取 ACK */
#define SDA_H()   (OLED_SDA_PORT->BSRR = (uint32_t)OLED_SDA_PIN)
/** SDA = 0 */
#define SDA_L()   (OLED_SDA_PORT->BSRR = ((uint32_t)OLED_SDA_PIN << 16u))
/** 读 SDA 真实电平（开漏释放后读 IDR 即总线状态）：1=高，0=低 */
#define SDA_RD()  ((OLED_SDA_PORT->IDR & OLED_SDA_PIN) ? 1u : 0u)

/** SSD1306 的 I2C 控制字节 */
#define OLED_CTRL_CMD    0x00u   /* Co=0, D/C#=0 -> 后续字节为命令 */
#define OLED_CTRL_DATA   0x40u   /* Co=0, D/C#=1 -> 后续字节为显存数据 */

/*---------------------------------------------------------------------------*/
/*  私有全局变量                                                               */
/*---------------------------------------------------------------------------*/

/**
 * @brief 影子显存（1 KB），供 IIC_OLED_DrawPoint 做读-改-写
 * @details SSD1306 在 I2C 模式下无法回读 GDDRAM，画单个像素必须先缓存整屏。
 * @warning 只有 IIC_OLED_Clear / DrawPoint / Refresh 维护它；
 *          ShowChar / ShowString / DrawBMP / DrawFont16/32 是「直接写屏」，
 *          不会更新影子。混用后影子失效，需 IIC_OLED_ClrShadow() + IIC_OLED_Clear()。
 */
static uint8_t s_oled_gram[Max_Page][Max_Column];

/*===========================================================================*/
/*  私有函数：软件模拟 I2C 底层时序                                             */
/*===========================================================================*/

/**
 * @brief  SCL 半周期延时
 * @note   计数变量 volatile，防止被编译器优化掉。改速度请调 SW_I2C_DLY_CNT。
 */
static void SW_I2C_Delay(void)
{
    volatile uint16_t d = SW_I2C_DLY_CNT;

    while (d--)
    {
        __NOP();
    }
}

/**
 * @brief  GPIO 初始化：PC6/PC7 配成开漏输出并释放总线
 * @note   由 IIC_OLED_Init() 开头调用一次，**不依赖 CubeMX 配置**。
 * @note   开漏 + 内部上拉：输出 1 时由上拉电阻拉高，从机可安全拉低回 ACK。
 *         若你的 OLED 模块自带 4.7k 上拉，内部上拉与之并联，无害。
 */
static void SW_I2C_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio;

    __HAL_RCC_GPIOC_CLK_ENABLE();

    gpio.Pin   = OLED_SCL_PIN | OLED_SDA_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_OD;      /* 开漏，必须 */
    gpio.Pull  = GPIO_PULLUP;              /* 内部上拉兜底 */
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(OLED_SCL_PORT, &gpio);

    SDA_H();                               /* 释放 SDA */
    SCL_H();                               /* 释放 SCL，总线进入空闲态 */
    SW_I2C_Delay();
}

/**
 * @brief  产生 I2C 起始条件：SCL 高电平期间 SDA 由 1 -> 0
 */
static void SW_I2C_Start(void)
{
    SDA_H();
    SCL_H();
    SW_I2C_Delay();
    SDA_L();
    SW_I2C_Delay();
    SCL_L();                               /* 钳住 SCL，准备发数据 */
    SW_I2C_Delay();
}

/**
 * @brief  产生 I2C 停止条件：SCL 高电平期间 SDA 由 0 -> 1
 */
static void SW_I2C_Stop(void)
{
    SCL_L();
    SDA_L();
    SW_I2C_Delay();
    SCL_H();
    SW_I2C_Delay();
    SDA_H();
    SW_I2C_Delay();
}

/**
 * @brief  发送 1 个字节（高位在前），从机在 SCL 上升沿采样
 * @param  byte 待发送字节
 */
static void SW_I2C_SendByte(uint8_t byte)
{
    uint8_t i;

    for (i = 0u; i < 8u; i++)
    {
        SCL_L();
        if (byte & 0x80u) { SDA_H(); } else { SDA_L(); }
        byte <<= 1;
        SW_I2C_Delay();
        SCL_H();                           /* 上升沿：从机采样 */
        SW_I2C_Delay();
    }

    SCL_L();
    SW_I2C_Delay();
}

/**
 * @brief  等待从机应答（第 9 个时钟）
 * @retval 0 = 收到 ACK（SDA 被从机拉低）；1 = NACK（从机无应答）
 */
static uint8_t SW_I2C_WaitAck(void)
{
    uint8_t ack;

    SDA_H();                               /* 释放 SDA，总线交给从机 */
    SW_I2C_Delay();
    SCL_H();
    SW_I2C_Delay();
    ack = SDA_RD();                        /* 读到的 0 就是从机拉低的 ACK */
    SCL_L();
    SW_I2C_Delay();

    return ack;
}

/*===========================================================================*/
/*  私有函数：向 OLED 写命令 / 数据                                            */
/*===========================================================================*/

/**
 * @brief  向 OLED 写一个命令字节
 * @param  cmd 命令字节
 * @note   帧格式：START + 0x78 + ACK + 0x00 + ACK + cmd + ACK + STOP
 *         软件 I2C 下每一步都要自己等 ACK，任一步 NACK 就 Error_Handler()。
 */
static void I2Cx_WriteCmd(uint8_t cmd)
{
    SW_I2C_Start();
    SW_I2C_SendByte(OLED_ADDR);
    if (SW_I2C_WaitAck()) { SW_I2C_Stop(); Error_Handler(); return; }

    SW_I2C_SendByte(OLED_CTRL_CMD);
    if (SW_I2C_WaitAck()) { SW_I2C_Stop(); Error_Handler(); return; }

    SW_I2C_SendByte(cmd);
    if (SW_I2C_WaitAck()) { SW_I2C_Stop(); Error_Handler(); return; }

    SW_I2C_Stop();
}

/**
 * @brief  向 OLED 写一个显存数据字节
 * @param  dat 数据字节
 * @note   帧格式：START + 0x78 + ACK + 0x40 + ACK + dat + ACK + STOP
 */
static void I2Cx_WriteData(uint8_t dat)
{
    SW_I2C_Start();
    SW_I2C_SendByte(OLED_ADDR);
    if (SW_I2C_WaitAck()) { SW_I2C_Stop(); Error_Handler(); return; }

    SW_I2C_SendByte(OLED_CTRL_DATA);
    if (SW_I2C_WaitAck()) { SW_I2C_Stop(); Error_Handler(); return; }

    SW_I2C_SendByte(dat);
    if (SW_I2C_WaitAck()) { SW_I2C_Stop(); Error_Handler(); return; }

    SW_I2C_Stop();
}

/**
 * @brief  向 OLED 批量写 n 个显存数据字节（Clear / DrawBMP / Refresh 的加速通道）
 * @param  p 数据首地址（可直接指向 Flash 里的 const 数组）
 * @param  n 数据字节数
 *
 * @note   全程只有 1 次 START + 1 次 STOP，中间连续发 n 个数据字节，
 *         等价于硬件 I2C 的「1 帧 129 字节」，但**不需要 129 字节暂存 buffer**
 *         （软件 I2C 可以从源地址逐字节取），省了 RAM 也省了 memcpy。
 *         SSD1306 在页地址模式下列地址自动 +1，所以连续写是正确且高效的。
 */
static void I2Cx_WriteDataBuf(const uint8_t *p, uint16_t n)
{
    SW_I2C_Start();
    SW_I2C_SendByte(OLED_ADDR);
    if (SW_I2C_WaitAck()) { SW_I2C_Stop(); Error_Handler(); return; }

    SW_I2C_SendByte(OLED_CTRL_DATA);
    if (SW_I2C_WaitAck()) { SW_I2C_Stop(); Error_Handler(); return; }

    while (n > 0u)
    {
        SW_I2C_SendByte(*p++);
        if (SW_I2C_WaitAck()) { SW_I2C_Stop(); Error_Handler(); return; }
        n--;
    }

    SW_I2C_Stop();
}

/*===========================================================================*/
/*  私有函数：总线检测与恢复                                                   */
/*===========================================================================*/

/**
 * @brief  总线解锁（软件 I2C 版，比硬件 I2C 简单得多）
 *
 * @details 引脚本来就是 GPIO 开漏，无需 DeInit 外设、无需切 AF、无需复位 RCC：
 *          直接在 SCL 上打 9 个脉冲，让从机把卡住的那个字节走完并释放 SDA，
 *          再补一个 STOP 条件即可。
 */
static void I2Cx_BusRecover(void)
{
    uint8_t i;

    SDA_H();
    SCL_H();
    SW_I2C_Delay();

    for (i = 0u; i < 9u; i++)
    {
        SCL_L();
        SW_I2C_Delay();
        SCL_H();
        SW_I2C_Delay();
    }

    /* STOP：SCL 保持高电平期间 SDA 由 0 -> 1 */
    SDA_L();
    SW_I2C_Delay();
    SCL_H();
    SW_I2C_Delay();
    SDA_H();
    SW_I2C_Delay();
}

/**
 * @brief  探测 OLED 是否在总线上应答，失败时自动尝试一次总线恢复
 * @param  trials 探测次数（每次 = 1 次 START + 地址字节 + 读 ACK）
 * @retval HAL_OK      设备应答
 *         HAL_TIMEOUT 设备无应答（地址不对 / 模块没上电 / 焊接 / 缺上拉电阻）
 *
 * @note   原理：发 START + 设备地址(写)，看第 9 个时钟有没有 ACK。
 *         全部失败时先做一次总线解锁，再重试一轮。
 * @note   调用点：
 *           - IIC_OLED_Init()  开头，trials=3，失败 -> Error_Handler()
 *           - OLED_IsReady()   trials=1，只返回结果，不进 Error_Handler
 */
static HAL_StatusTypeDef I2Cx_IsDeviceReady(uint16_t trials)
{
    uint16_t t;

    for (t = 0u; t < trials; t++)
    {
        SW_I2C_Start();
        SW_I2C_SendByte(OLED_ADDR);
        if (SW_I2C_WaitAck() == 0u)          /* 0 = ACK，设备在线 */
        {
            SW_I2C_Stop();
            return HAL_OK;
        }
        SW_I2C_Stop();
    }

    /* 全部失败：解锁一次再试最后一轮 */
    I2Cx_BusRecover();

    for (t = 0u; t < trials; t++)
    {
        SW_I2C_Start();
        SW_I2C_SendByte(OLED_ADDR);
        if (SW_I2C_WaitAck() == 0u)
        {
            SW_I2C_Stop();
            return HAL_OK;
        }
        SW_I2C_Stop();
    }

    return HAL_TIMEOUT;
}

/*===========================================================================*/
/*  对外 API                                                                   */
/*===========================================================================*/

/**
 * @brief  查询 OLED 在线状态（对外 API，失败不进 Error_Handler）
 * @retval 1 在线；0 离线 / 总线故障
 */
uint8_t OLED_IsReady(void)
{
    return (I2Cx_IsDeviceReady(1) == HAL_OK) ? 1u : 0u;
}

/**
 * @brief  统一写入接口：根据 cmd 标志区分命令或数据
 * @param  dat 要写入的字节
 * @param  cmd OLED_CMD(0) 写命令 / OLED_DATA(1) 写数据
 * @note   签名与旧版完全一致，上层函数零改动即可自动切到软件模拟 I2C。
 */
void IIC_OLED_WR_Byte(uint8_t dat, uint8_t cmd)
{
    if (cmd)
    {
        I2Cx_WriteData(dat);
    }
    else
    {
        I2Cx_WriteCmd(dat);
    }
}

/*---------------------------------------------------------------------------*/
/*  基础控制                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief  设置显示起始坐标（页地址 + 列地址）
 * @param  x 列地址 0~127
 * @param  y 页地址 0~7
 * @note   三条命令：0xB0+y（页）、0x10|(x>>4)（列高 4 位）、0x0F&x（列低 4 位）
 */
void IIC_OLED_Set_Pos(unsigned char x, unsigned char y)
{
    I2Cx_WriteCmd((uint8_t)(0xB0 + y));
    I2Cx_WriteCmd((uint8_t)(((x & 0xF0) >> 4) | 0x10));
    I2Cx_WriteCmd((uint8_t)(x & 0x0F));
}

/**
 * @brief  开启 OLED 显示（退出休眠）
 * @details 启用内部电荷泵并开启显示
 */
void IIC_OLED_Display_On(void)
{
    I2Cx_WriteCmd(0x8D);   /* SET DCDC  */
    I2Cx_WriteCmd(0x14);   /* DCDC ON   */
    I2Cx_WriteCmd(0xAF);   /* DISPLAY ON */
}

/**
 * @brief  关闭 OLED 显示（进入超低功耗休眠，功耗 <10 μA）
 */
void IIC_OLED_Display_Off(void)
{
    I2Cx_WriteCmd(0x8D);   /* SET DCDC   */
    I2Cx_WriteCmd(0x10);   /* DCDC OFF   */
    I2Cx_WriteCmd(0xAE);   /* DISPLAY OFF */
}

/**
 * @brief  清空整个 OLED 屏幕（填充 0）+ 清空影子显存
 * @note   旧版：8 页 x 128 次 x 2 字节 = 1024 次 I2C 传输
 *         新版：8 次 x 129 字节 = 8 次 I2C 传输，速度约提升 100 倍
 */
void IIC_OLED_Clear(void)
{
    uint8_t page;

    /* static 存储期的数组由 C 标准保证零初始化，无需显式清零，也不进 .data 段 */
    static uint8_t s_zero_blk[Max_Column];

    for (page = 0u; page < Max_Page; page++)
    {
        IIC_OLED_Set_Pos(0, page);
        I2Cx_WriteDataBuf(s_zero_blk, Max_Column);
    }

    memset(s_oled_gram, 0, sizeof(s_oled_gram));
}

/**
 * @brief  全屏点亮（用于连通性测试）
 * @warning 点亮所有像素会加速 OLED 老化，仅用于验证硬件链路，勿长时间使用
 * @note   沿用旧版语义：填充 0x01（旧版注释写 0xFF 但实际写的是 1）
 */
void IIC_OLED_On(void)
{
    uint8_t  page;
    uint16_t i;

    /* 非 static，避免长期占用 128 字节 RAM；每次调用重新填充 */
    uint8_t s_on_blk[Max_Column];

    for (i = 0u; i < Max_Column; i++)
    {
        s_on_blk[i] = 0x01u;
    }

    for (page = 0u; page < Max_Page; page++)
    {
        IIC_OLED_Set_Pos(0, page);
        I2Cx_WriteDataBuf(s_on_blk, Max_Column);
    }

    memset(s_oled_gram, 0x01, sizeof(s_oled_gram));
}

/*---------------------------------------------------------------------------*/
/*  文本与图形显示                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief  快速幂函数（用于数字分解）
 * @param  m 底数
 * @param  n 指数
 * @return m^n
 * @note   仅内部使用，不对外暴露
 */
static uint32_t oled_pow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;
    while (n--)
        result *= m;
    return result;
}

/**
 * @brief  显示单个 ASCII 字符（16x8 点阵）
 * @param  x 起始列（0~127）
 * @param  y 起始页（0~7）
 * @param  chr ASCII 字符（空格 ' ' 起）
 * @note   自动处理换行：若 x > Max_Column-1，则 x=0, y+=2
 */
void IIC_OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr)
{
    uint8_t t = 0, c = 0;

    c = chr - ' ';                  /* 字模表从空格开始 */

    if (x > Max_Column - 1u)
    {
        x = 0;
        y = y + 2;
    }

    /* 上半页 */
    IIC_OLED_Set_Pos(x, y);
    for (t = 0; t < 8; t++)
        IIC_OLED_WR_Byte(ASCII_1608[c][t], OLED_DATA);

    /* 下半页 */
    IIC_OLED_Set_Pos(x, y + 1);
    for (t = 0; t < 8; t++)
        IIC_OLED_WR_Byte(ASCII_1608[c][8 + t], OLED_DATA);
}

/**
 * @brief  显示无符号整数（右对齐，前导补空格）
 * @param  x 起始列
 * @param  y 起始页
 * @param  num 要显示的数字
 * @param  len 总显示宽度（位数）
 * @note   例如：num=5, len=3 -> 显示 "  5"
 */
void IIC_OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len)
{
    uint8_t t, temp;
    uint8_t enshow = 0;

    for (t = 0; t < len; t++)
    {
        temp = (num / oled_pow(10, len - t - 1)) % 10;
        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0)
            {
                IIC_OLED_ShowChar(x + 8 * t, y, ' ');
                continue;
            }
            else
            {
                enshow = 1;
            }
        }
        IIC_OLED_ShowChar(x + 8 * t, y, temp + '0');
    }
}

/**
 * @brief  显示 ASCII 字符串
 * @param  x 起始列
 * @param  y 起始页
 * @param  p 以 '\0' 结尾的字符串
 * @note   自动换行：当 x > 120 时换行到下一页组
 */
void IIC_OLED_ShowString(uint8_t x, uint8_t y, const char *p)
{
    unsigned char j = 0;

    while (p[j] != '\0')
    {
        IIC_OLED_ShowChar(x, y, (uint8_t)p[j]);
        x += 8;
        if (x > 120)
        {
            x = 0;
            y += 2;
        }
        j++;
    }
}

/**
 * @brief  显示预定义汉字（使用 Hzk 数组索引）
 * @param  x 起始列
 * @param  y 起始页
 * @param  no 汉字在 Hzk[] 中的序号（0 起）
 * @note   每个汉字占 32 字节（16x16），分两页显示
 */
void IIC_OLED_ShowCHinese(uint8_t x, uint8_t y, uint8_t no)
{
    uint8_t t;

    IIC_OLED_Set_Pos(x, y);
    for (t = 0; t < 16; t++)
    {
        IIC_OLED_WR_Byte((uint8_t)Hzk[2 * no][t], OLED_DATA);
    }

    IIC_OLED_Set_Pos(x, y + 1);
    for (t = 0; t < 16; t++)
    {
        IIC_OLED_WR_Byte((uint8_t)Hzk[2 * no + 1][t], OLED_DATA);
    }
}

/**
 * @brief  显示位图图片
 * @param  x0 起始列（含）
 * @param  y0 起始页（含）
 * @param  x1 终止列（不含），全屏填 128
 * @param  y1 终止页（不含），全屏填 8 —— 注意单位是「页」不是像素行
 * @param  BMP 位图数据（按页连续线性存放，每页 128 字节）
 *
 * @note   全屏调用：IIC_OLED_DrawBMP(0, 0, 128, 8, BMP_LONG);
 *         BMP_LONG 为 1024 字节 = 8 页 x 128 列，与参数完全吻合，不会越界。
 * @note   旧版每像素一次 2 字节传输（1024 次）；新版每页一次 129 字节传输（8 次），
 *         速度约提升 100 倍。索引方式保持线性 j++，与旧版取模数据完全兼容。
 */
void IIC_OLED_DrawBMP(unsigned char x0, unsigned char y0,
                      unsigned char x1, unsigned char y1,
                      const unsigned char *BMP)
{
    unsigned int  j = 0u;
    unsigned int  width;
    unsigned char x, y;

    if (x1 <= x0) return;
    if (y1 <= y0) return;

    width = (unsigned int)(x1 - x0);

    for (y = y0; y < y1; y++)
    {
        IIC_OLED_Set_Pos(x0, y);
        I2Cx_WriteDataBuf(&BMP[j], (uint16_t)width);
        j += width;                 /* 线性递增，与旧版 j++ 语义一致 */
    }
}

/**
 * @brief  绘制 / 擦除单个像素点
 * @param  x 列坐标 0~127
 * @param  y 像素行坐标 0~63 —— 注意这里与 ShowChar/DrawBMP 的「页」不同
 * @param  t 1 = 点亮，0 = 擦除
 *
 * @note   SSD1306 在 I2C 模式下无法回读 GDDRAM，单点读-改-写必须依赖 MCU 侧的
 *         影子显存 s_oled_gram。本函数：改影子 -> 定位 (x, page) -> 写回那 1 字节。
 * @note   每次调用占用一次 2 字节 I2C 传输。若需密集画点，建议改完一串点后
 *         调用 IIC_OLED_Refresh() 整屏刷（需要把本函数第 2 步注释掉）。
 *
 * @warning 影子显存只由 Clear / DrawPoint / Refresh 维护。若先显示过文字或图片
 *         再画线，影子已与屏幕不同步，请 IIC_OLED_ClrShadow() + IIC_OLED_Clear()
 *         后重新开始。
 */
void IIC_OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t t)
{
    uint8_t page;
    uint8_t bit;
    uint8_t val;

    if (x >= Max_Column) return;
    if (y >= 64u)        return;            /* 8 页 x 8 行 = 64 像素行 */

    page = (uint8_t)(y / 8u);               /* 所在页 0~7 */
    bit  = (uint8_t)(y % 8u);               /* 页内位 0~7（LSB = 该页最上面一行） */

    /* 1) 读-改-写影子显存 */
    if (t)
    {
        s_oled_gram[page][x] |=  (uint8_t)(1u << bit);
    }
    else
    {
        s_oled_gram[page][x] &= (uint8_t)(~(1u << bit));
    }
    val = s_oled_gram[page][x];

    /* 2) 定位到该列该页，写回这 1 个字节 */
    IIC_OLED_Set_Pos(x, page);
    I2Cx_WriteData(val);
}

/**
 * @brief  清空内部影子显存（不清屏）
 */
void IIC_OLED_ClrShadow(void)
{
    memset(s_oled_gram, 0, sizeof(s_oled_gram));
}

/**
 * @brief  把影子显存整屏刷到 OLED（8 帧 x 129 字节）
 * @note   与 DrawPoint 配合的批量画法：若想改成「先改影子、最后一次性刷新」，
 *         把 IIC_OLED_DrawPoint 的第 2 步（Set_Pos + WriteData）注释掉即可。
 */
void IIC_OLED_Refresh(void)
{
    uint8_t page;

    for (page = 0u; page < Max_Page; page++)
    {
        IIC_OLED_Set_Pos(0, page);
        I2Cx_WriteDataBuf(&s_oled_gram[page][0], Max_Column);
    }
}

/**
 * @brief  显示单个 16x16 中文字（从 Tfont16 查找）
 * @param  x 起始列
 * @param  y 起始页
 * @param  s 指向 GB2312 编码的汉字（2 字节）
 * @note   自动匹配 Tfont16 中的 Index，找到后立即绘制并 break 退出
 *
 * @note【编码约定 · 重要】字模表 Tfont16[].Index 存的是 GB2312 内码
 *       （例如“好” = 0xBA 0xC3），因此传入的字符串也必须是 GB2312 字节。
 *       在 UTF-8 源码里直接写汉字字面量 “好” 得到的是 E5 A5 BD（3 字节），
 *       与 0xBA 0xC3 不匹配，会静默查找失败（什么都不显示）。
 *       两种正确调用方式：
 *         1) 用内码转义：IIC_OLED_Show_Str(0, 0, "\xBA\xC3", 16);
 *         2) 把本文件与调用方文件统一存为 GBK 编码，直接写 “好”
 */
void IIC_OLED_DrawFont16(uint16_t x, uint16_t y, const char *s)
{
    uint8_t  x0 = 0u, y0 = 0u;
    uint16_t k  = 0u;
    uint16_t HZnum = 0u;

    /* 自动统计汉字数目 */
    HZnum = sizeof(Tfont16) / sizeof(typFNT_GB16);

    /* 循环寻找匹配的 Index[2] 成员值 */
    for (k = 0u; k < HZnum; k++)
    {
        if ((Tfont16[k].Index[0] == (uint8_t)*(s)) &&
            (Tfont16[k].Index[1] == (uint8_t)*(s + 1)))
        {
            /* 16x16 = 2 页 x 16 列 */
            for (y0 = 0u; y0 < 2u; y0++)
            {
                IIC_OLED_Set_Pos((unsigned char)x, (unsigned char)(y + y0));
                for (x0 = 0u; x0 < 16u; x0++)
                {
                    IIC_OLED_WR_Byte(Tfont16[k].Msk[y0 * 16u + x0], OLED_DATA);
                }
            }
            /* 完成绘字后立即 break，防止多个汉字重复取模显示 */
            break;
        }
    }
}

/**
 * @brief  显示单个 32x32 中文字（从 Tfont32 查找）
 * @param  x 起始列
 * @param  y 起始页
 * @param  s 指向 GB2312 编码的汉字（2 字节）
 *
 * @note【编码约定】同 DrawFont16：Index 存 GB2312 内码，入参须为 GB2312 字节，
 *       在 UTF-8 源码中请用 "\xBA\xC3" 形式，勿直接写汉字字面量。
 *
 * @note【Bug 修复】旧版 Oled.c:409 在查表比较时错用了 Tfont16[k].Index，导致：
 *         a) 汉字数超过 GB16_NUM(8) 时越界访问 Tfont16；
 *         b) 索引 k 是按 Tfont16 的匹配结果确定的，与 Tfont32 的排序不一致，
 *            画出的字模张冠李戴。
 *       已改为 Tfont32[k].Index。
 */
void IIC_OLED_DrawFont32(uint16_t x, uint16_t y, const char *s)
{
    uint8_t  x0 = 0u, y0 = 0u;
    uint16_t k  = 0u;
    uint16_t HZnum = 0u;

    /* 自动统计汉字数目 */
    HZnum = sizeof(Tfont32) / sizeof(typFNT_GB32);

    /* 循环寻找匹配的 Index[2] 成员值 */
    for (k = 0u; k < HZnum; k++)
    {
        /* 对应成员值匹配 —— 修复点：Tfont16[k] -> Tfont32[k] */
        if ((Tfont32[k].Index[0] == (uint8_t)*(s)) &&
            (Tfont32[k].Index[1] == (uint8_t)*(s + 1)))
        {
            /* 32x32 = 4 页 x 32 列 */
            for (y0 = 0u; y0 < 4u; y0++)
            {
                IIC_OLED_Set_Pos((unsigned char)x, (unsigned char)(y + y0));
                for (x0 = 0u; x0 < 32u; x0++)
                {
                    IIC_OLED_WR_Byte(Tfont32[k].Msk[y0 * 32u + x0], OLED_DATA);
                }
            }
            /* 完成绘字后立即 break，防止多个汉字重复取模显示 */
            break;
        }
    }
}

/**
 * @brief  混合显示中英文字符串（自动识别 GB2312）
 * @param  x 起始列
 * @param  y 起始页
 * @param  str 字符串指针（GB2312 编码，中文 2 字节）
 * @param  size 字体大小（16 或 32，其它值按 16 处理）
 *
 * @note【Bug 修复 1】旧版 Oled.c:468 换行用 y += size。y 的单位是「页」不是像素行，
 *       size=16 时 y+=16 会直接跳到页 16（越界）。正确做法是 y += 2（16 高占 2 页）。
 *
 * @note【Bug 修复 2】旧版 Oled.c:505 中文显示后 str += 3。一个 GB2312 汉字只占
 *       2 字节，+3 会吃掉下一个汉字的首字节，导致后续字符全部乱码。
 *       正确做法是 str += 2。
 *
 * @note   屏幕 128x64 只能放 4 行 16x16 中文（每行 8 个），或 2 行 32x32 中文
 *       （每行 4 个），超出会回卷到页 0。
 */
void IIC_OLED_Show_Str(uint16_t x, uint16_t y, const char *str, uint8_t size)
{
    uint16_t x0  = x;
    uint8_t  bHz = 0u;          /* 字符或者中文，首先默认是字符 */

    if (size != 32u)
    {
        size = 16u;             /* 默认 1608 */
    }

    while (*str != 0)           /* 判断是否为结束符 */
    {
        if (!bHz)               /* 判断是字符 */
        {
            /* 如果 x，y 坐标超出预设 lcd 屏大小则换行显示 */
            if (x > (128u - size / 2u))
            {
                x = 0u;                                 /* 显示靠前 */
                y = (uint16_t)((y + size / 8u) % 8u);   /* 显示换行 */
            }
            if (y > (8u - size / 8u))
            {
                y = 0u;
            }

            if ((uint8_t)*str > 0x80u)      /* 判断是否为中文（GB2312 首字节 > 0x80） */
            {
                bHz = 1u;                   /* 是中文，跳过字符显示改为显示中文 */
            }
            else                            /* 确定为字符 */
            {
                if (*str == 0x0D)           /* 判断是换行符号 */
                {
                    y += 2u;                /* 【修复】y += size -> y += 2，y 的单位是页 */
                    x  = x0;                /* 显示靠前 */
                    str++;                  /* 准备下一个字符 */
                }
                else                        /* 判断不是换行符 */
                {
                    IIC_OLED_ShowChar((uint8_t)x, (uint8_t)y, (uint8_t)*str);
                    x += size / 2u;         /* 显示完后右移，准备下次显示 */
                }
                str++;                      /* 显示地址自增，准备下一个字符 */
            }
        }
        else                                /* 判断是中文 */
        {
            /* 如果 x，y 坐标超出预设 lcd 屏大小则换行显示 */
            if (x > (128u - size))
            {
                x = 0u;                                 /* 显示靠前 */
                y = (uint16_t)((y + size / 8u) % 8u);   /* 显示换行 */
            }
            if (y > (8u - size / 8u))
            {
                y = 0u;
            }

            bHz = 0u;                       /* 改为默认字符用于下次字符判断 */

            if (size == 32u)                /* 判断是否为 32x32 大小的中文 */
            {
                IIC_OLED_DrawFont32(x, y, str);
            }
            else                            /* 否则为 16x16 大小的中文 */
            {
                IIC_OLED_DrawFont16(x, y, str);
            }

            str += 2u;                      /* 【修复】str += 3 -> str += 2，GB2312 汉字占 2 字节 */
            x   += size;                    /* 显示完后右移起始显示横坐标准备下次显示 */
        }
    }
}

/*---------------------------------------------------------------------------*/
/*  初始化                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief  OLED 初始化（软件 I2C 引脚配置 + SSD1306 寄存器配置）
 *
 * @note   本函数**自己**完成 PC6/PC7 的 GPIO 配置（开漏 + 上拉），
 *         既不依赖 CubeMX，也不依赖 I2C1 外设，因此在 main() 里
 *         MX_I2C1_Init() 之前或之后调用都可以。建议统一放在
 *         "Initialize all configured peripherals" 之后（USER CODE BEGIN 2），
 *         与其它外设初始化保持一致的阅读顺序。
 *
 * @note   与 AT24C02 的关系：AT24C02 走硬件 I2C1（PB8/PB9），本 OLED 走
 *         软件 I2C（PC6/PC7），两条链路**物理独立、互不干扰**，
 *         所以 AT24C02 接在哪、有没有接，都不影响 OLED 工作。
 */
void IIC_OLED_Init(void)
{
    /* 0) 先把 PC6/PC7 配成开漏输出并释放总线（软件 I2C 的引脚由本驱动自理） */
    SW_I2C_GPIO_Init();

    /* 1) 确认设备在线（内部带一次总线自恢复），不在线直接停在这里 */
    if (I2Cx_IsDeviceReady(3) != HAL_OK)
    {
        Error_Handler();
    }

    /* 2) SSD1306 初始化序列（与旧版字节序完全一致） */
    I2Cx_WriteCmd(0xAE);   /* --display off                       */
    I2Cx_WriteCmd(0x00);   /* ---set low column address           */
    I2Cx_WriteCmd(0x10);   /* ---set high column address          */
    I2Cx_WriteCmd(0x40);   /* --set start line address            */
    I2Cx_WriteCmd(0xB0);   /* --set page address                  */
    I2Cx_WriteCmd(0x81);   /* contract control                    */
    I2Cx_WriteCmd(0xFF);   /* --128                               */
    I2Cx_WriteCmd(0xA1);   /* set segment remap                   */
    I2Cx_WriteCmd(0xA6);   /* --normal / reverse                  */
    I2Cx_WriteCmd(0xA8);   /* --set multiplex ratio(1 to 64)      */
    I2Cx_WriteCmd(0x3F);   /* --1/32 duty                         */
    I2Cx_WriteCmd(0xC8);   /* Com scan direction                  */
    I2Cx_WriteCmd(0xD3);   /* -set display offset                 */
    I2Cx_WriteCmd(0x00);   /*                                     */

    I2Cx_WriteCmd(0xD5);   /* set osc division                    */
    I2Cx_WriteCmd(0x80);   /*                                     */

    I2Cx_WriteCmd(0xD8);   /* set area color mode off             */
    I2Cx_WriteCmd(0x05);   /*                                     */

    I2Cx_WriteCmd(0xD9);   /* Set Pre-Charge Period               */
    I2Cx_WriteCmd(0xF1);   /*                                     */

    I2Cx_WriteCmd(0xDA);   /* set com pin configuartion           */
    I2Cx_WriteCmd(0x12);   /*                                     */

    I2Cx_WriteCmd(0xDB);   /* set Vcomh                           */
    I2Cx_WriteCmd(0x30);   /*                                     */

    I2Cx_WriteCmd(0x8D);   /* set charge pump enable              */
    I2Cx_WriteCmd(0x14);   /*                                     */

    /* 3) 清屏（顺带把影子显存一起清掉，保证 DrawPoint 起点与屏幕一致） */
    IIC_OLED_Clear();

    /* 4) 开显示（旧版把 0xAF 放在序列末尾，这里拆出来便于失败定位） */
    I2Cx_WriteCmd(0xAF);   /* --turn on oled panel                */
}
