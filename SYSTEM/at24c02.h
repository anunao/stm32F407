#ifndef __AT24C02_H__
#define __AT24C02_H__

#include "main.h"

/*
 * AT24C02 驱动（适配 CubeMX 生成的工程）
 * ---------------------------------------------------------------
 * I2C1 控制器与 GPIO(PB8/PB9) 全部由 CubeMX 生成的 MX_I2C1_Init() 负责，
 * 本驱动不再定义 hi2c1、不再做任何底层初始化，只做器件级读写。
 *
 * 引脚提醒：
 *   PB8 -> I2C1_SCL    PB9 -> I2C1_SDA   (复用开漏 AF4 + 外部上拉 4.7k~10k)
 *   WP 引脚必须接地，否则写入会静默失败
 */

/* 7 位设备地址（A2A1A0 全接地）= 0x50
   HAL 的 DevAddress 要求"左对齐的 8 位地址"，所以实际用 AT24C02_ADDR << 1 = 0xA0 */
#define AT24C02_ADDR         0x50u
#define AT24C02_ADDR_WRITE   ((uint16_t)(AT24C02_ADDR << 1))

/* 24C02：256 字节、页写 8 字节、写周期 tWR ≤ 5ms */
#define AT24C02_SIZE         256u
#define AT24C02_PAGE_SIZE    8u

/* 其它返回值：0 = 成功，-1 = 失败 */
int  at24c02_init(void);                                            /* 只做在线探测 */
int  at24c02_is_busy(void);                                         /* 1=忙(处于tWR) 0=空闲 */
int  at24c02_wait_idle(uint32_t timeout_ms);                        /* 带超时等待，防死锁 */
void wait_at24c02_finished_to_write(void);                           /* 兼容旧名：等 tWR 结束 */

int  write_a_byte_to_at24c02(uint8_t addr, uint8_t data);
int  write_bytes_to_at24c02(uint8_t addr, uint8_t *data, uint16_t size);
int  read_bytes_from_at24c02(uint8_t addr, uint8_t *data, uint16_t size);

#endif
