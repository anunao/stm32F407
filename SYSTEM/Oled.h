/**
 * @file    Oled.h
 * @brief   GPIO 软件模拟 I2C 的 SSD1306 (128x64) OLED 驱动头文件（STM32F4）
 * @version V6.0.0（软件模拟 I2C 版）
 *
 * 【为什么用软件模拟 I2C】
 *   硬件 I2C1 上还挂着 AT24C02（EEPROM，存 Wi-Fi 配置）。让 OLED 独占
 *   PC6/PC7 走软件 I2C 后，两条链路**物理隔离**：
 *     - AT24C02 继续用硬件 I2C1（PB8/PB9），完全不受 OLED 影响；
 *     - OLED 不受 I2C1 的 BUSY 死锁、NVIC、句柄竞争等任何牵连；
 *     - 也不再有「AT24C02 必须接到 PB8/PB9」的硬件约束。
 *
 * @note 硬件连接：**SCL = PC6，SDA = PC7**，由本驱动自己初始化为
 *       开漏输出 + 内部上拉（IIC_OLED_Init() 内部调 SW_I2C_GPIO_Init），
 *       **无需在 CubeMX 里配置这两个引脚**。
 * @note 屏幕规格：128(宽) x 64(高)，页地址模式（Page Addressing Mode），8 页 x 128 列
 * @note 坐标系：x ∈ [0,127] 列；y ∈ [0,7] 页（每页 8 像素高）
 *
 * ┌──────────────────────────────────────────────────────────────────────┐
 * │ 关于 NVIC / I2C 外设                                                 │
 * ├──────────────────────────────────────────────────────────────────────┤
 * │ 本驱动**完全不碰 I2C1 外设**，因此：                                   │
 * │   · 不依赖 hi2c1 句柄，也不依赖 CubeMX 的 I2C 配置；                   │
 * │   · 与 I2C1 的 NVIC（I2C1_EV / I2C1_ER）毫无关系，开不开都无所谓；      │
 * │   · CubeMX 里 I2C1 保持使能即可（给 AT24C02 用），不必为 OLED 改动。    │
 * │ 通信速率由 Oled.c 里的 SW_I2C_DLY_CNT 决定，默认约 300~500 kHz。       │
 * └──────────────────────────────────────────────────────────────────────┘
 *
 * @warning 【不要在中断里调用本驱动的任何函数】
 *          软件 I2C 靠 CPU 翻转 GPIO，整屏刷新要占 CPU 十几毫秒，
 *          若在 TIM6 等中断回调里调 IIC_OLED_Refresh()，既会阻塞其它中断，
 *          也可能与主循环的刷新重入冲突。
 *          正确做法：中断里只置标志位（如 oled_need_refresh），
 *          由主循环消费标志再刷新。
 *
 * @note 错误处理：写命令/数据收不到 ACK 一律跳 Error_Handler()，上层无需
 *       判断返回值。另有 OLED_IsReady() 供需要「查询但不死机」的场合使用。
 */

#ifndef __OLED_IIC_H
#define __OLED_IIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/*------------------ 设备地址 ------------------*/

/**
 * @brief OLED 从机地址（8-bit 写地址 = 7-bit 地址左移一位、末位 0）
 *
 * @details 软件 I2C 由本驱动自己拼这个地址字节，约定与 I2C 规范一致：
 *          - SSD1306 SA0=0（接地，最常见）→ 7-bit 0x3C → 8-bit 0x78  ← 当前值
 *          - SSD1306 SA0=1（接高）        → 7-bit 0x3D → 8-bit 0x7A
 *
 * @warning 若烧录后屏幕全黑无反应，**第一件事**就是把 0x78u 改成 0x7Au 重试。
 *          这是「屏幕没反应」最常见的原因，别急着怀疑时序和接线。
 */
#define OLED_ADDR          0x78u

/**
 * @brief 【已废弃】硬件 I2C 时代的一次传输超时（ms）
 * @details 本版改用软件 I2C 后不再有超时机制（改为逐字节等待 ACK）。
 *          保留此宏仅为兼容可能引用它的旧代码，可安全忽略。
 */
#define OLED_I2C_TIMEOUT   100u

/*------------------ OLED 工作模式定义 ------------------*/

#define OLED_CMD   0u   ///< 命令模式（I2C 控制字节 0x00）
#define OLED_DATA  1u   ///< 数据模式（I2C 控制字节 0x40）

/*------------------ 屏幕尺寸 ------------------*/

#define Max_Column  128u   ///< 最大列数（宽度）
#define Max_Page    8u     ///< 最大页数（64 / 8）

/*------------------ 函数声明 ------------------*/

/**
 * @defgroup OLED_IIC_Driver OLED 硬件 I2C 驱动接口
 * @{
 *
 * 所有 IIC_OLED_* 函数名与旧版软件模拟 I2C 版本完全一致，调用方无需修改。
 *
 * 坐标系约定：
 * - x ∈ [0, 127]：列地址（水平方向）
 * - y ∈ [0, 7]  ：页地址（垂直方向，每页 8 像素高）
 */

/**
 * @brief  统一写入接口（签名与旧版完全一致）
 * @param  dat 要写入的字节
 * @param  cmd OLED_CMD(0)=命令 / OLED_DATA(1)=显存数据
 */
void IIC_OLED_WR_Byte(uint8_t dat, uint8_t cmd);

/*------ OLED 控制与初始化 ------*/

/**
 * @brief  初始化 OLED（软件 I2C 引脚配置 + SSD1306 寄存器配置）
 * @note   内部自行把 PC6(SCL) / PC7(SDA) 配成开漏输出 + 内部上拉，
 *         **不需要在 CubeMX 里配置这两个引脚**，也不依赖 MX_I2C1_Init()。
 * @note   内部会先探测设备在线（含一次总线自恢复），OLED 没接好会直接
 *         Error_Handler()。若想「OLED 不在也能开机」，请自行改成判断
 *         OLED_IsReady() 后再调本函数。
 */
void IIC_OLED_Init(void);
void IIC_OLED_Clear(void);          ///< 清空整个屏幕（批量写入，8 帧完成）
void IIC_OLED_On(void);             ///< 全屏点亮测试（填充 0x01）
void IIC_OLED_Display_On(void);     ///< 开启显示（退出休眠）
void IIC_OLED_Display_Off(void);    ///< 关闭显示（进入低功耗休眠）

/*------ 文本与图形 ------*/

void IIC_OLED_Set_Pos(unsigned char x, unsigned char y);   ///< 设置写入起始位置（x:列, y:页）
void IIC_OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr); ///< 显示单个 ASCII 字符（16x8）
void IIC_OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len);  ///< 显示整数（右对齐）
void IIC_OLED_ShowString(uint8_t x, uint8_t y, const char *p);           ///< 显示 ASCII 字符串
void IIC_OLED_ShowCHinese(uint8_t x, uint8_t y, uint8_t no);             ///< 显示预定义汉字（Hzk 索引）
void IIC_OLED_DrawBMP(unsigned char x0, unsigned char y0,
                      unsigned char x1, unsigned char y1,
                      const unsigned char *BMP);                         ///< 显示位图（指定区域）
void IIC_OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t t);                ///< 绘制/擦除单个像素点
void IIC_OLED_DrawFont16(uint16_t x, uint16_t y, const char *s);         ///< 显示 16x16 中文
void IIC_OLED_DrawFont32(uint16_t x, uint16_t y, const char *s);         ///< 显示 32x32 中文
void IIC_OLED_Show_Str(uint16_t x, uint16_t y, const char *str, uint8_t size);  ///< 混合显示中英文

/*------ 新增：总线状态 / 影子显存 ------*/

/**
 * @brief  查询 OLED 是否在 I2C 总线上应答
 * @retval 1 = 设备在线且总线空闲；0 = 设备无应答 / 总线忙 / 超时
 * @note   与底层不同，本函数**不会**调用 Error_Handler()，仅返回状态。
 *         失败时内部会自动尝试一次 I2C 总线恢复（9 个 SCL 脉冲解锁）。
 *         典型用法：if (OLED_IsReady() == 0) {  点亮告警 LED  }
 */
uint8_t OLED_IsReady(void);

/**
 * @brief  把内部 1 KB 影子显存整屏刷到 OLED（8 帧 x 129 字节）
 * @note   配合 IIC_OLED_DrawPoint 使用。
 */
void IIC_OLED_Refresh(void);

/**
 * @brief  清空内部影子显存（不清屏）
 * @note   影子显存只由 Clear / DrawPoint / Refresh 维护；ShowChar / DrawBMP /
 *         DrawFont16/32 等是直接写屏，不会更新影子。混用后影子与屏幕不同步，
 *         重新开始画点前请先调用本函数再 IIC_OLED_Clear()。
 */
void IIC_OLED_ClrShadow(void);

/** @} */ // end of OLED_IIC_Driver

#ifdef __cplusplus
}
#endif

#endif /* __OLED_IIC_H */
