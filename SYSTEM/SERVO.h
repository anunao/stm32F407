/**
  ******************************************************************************
  * @file    SERVO.h
  * @brief   舵机驱动头文件
  * @note    舵机信号线连接到 PE9，也就是 TIM1_CH1。
  *          舵机角度范围为 0 到 180 度。
  *
  *          工作模式：
  *            - 撤防（DISARM）：固定 0 度；
  *            - 布防扫描（SCAN）：10° → 170° → 10° 往返，完整周期 4 秒；
  *            - 报警（ALARM）：转到预设报警角度（默认 90 度），停止扫描。
  ******************************************************************************
  */

#ifndef __SERVO_H
#define __SERVO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief  舵机最小角度
 */
#define SERVO_MIN_ANGLE    0

/**
 * @brief  舵机最大角度
 */
#define SERVO_MAX_ANGLE    180

/**
 * @brief  布防扫描的起始角度
 */
#define SERVO_SCAN_MIN     10

/**
 * @brief  布防扫描的终止角度
 */
#define SERVO_SCAN_MAX     170

/**
 * @brief  报警时舵机的固定角度
 */
#define SERVO_ALARM_ANGLE  90

/**
 * @brief  舵机工作模式
 */
typedef enum
{
    SERVO_MODE_DISARM = 0,   /* 撤防：固定 0 度 */
    SERVO_MODE_SCAN,         /* 布防扫描：10°→170°→10° */
    SERVO_MODE_ALARM         /* 报警：转到预设角度并停止扫描 */
} SERVO_Mode;

/**
 * @brief  启动舵机 PWM 输出
 * @param  无
 * @retval 无
 */
void SERVO_Start(void);

/**
 * @brief  设置舵机角度
 * @param  angle 舵机角度，范围为 0 到 180 度
 * @retval 无
 * @note   如果角度超过 180 度，驱动程序会按 180 度处理。
 */
void SERVO_SetAngle(uint8_t angle);

/**
 * @brief  设置舵机工作模式
 * @param  mode 目标模式
 * @retval 无
 * @note   切换模式时立即执行一次角度动作：
 *          撤防 -> 0 度；报警 -> 预设角度；扫描 -> 从 10 度开始。
 */
void SERVO_SetMode(SERVO_Mode mode);

/**
 * @brief  推进舵机状态机（主循环中反复调用）
 * @param  无
 * @retval 无
 * @note   仅在扫描模式下每 25 ms 改变 2 度；其余模式直接返回。
 */
void SERVO_Service(void);

#ifdef __cplusplus
}
#endif

#endif /* __SERVO_H */
