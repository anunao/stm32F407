/**
  ******************************************************************************
  * @file    ADC_Read.h
  * @brief   ADC 采样驱动头文件
  * @note    当前工程使用两个 ADC 输入：
  *          1. PA5：光敏传感器，使用 ADC1
  *          2. PF7：滑动变阻器，使用 ADC3
  *
  *          ADC 返回值范围为 0 到 4095。
  ******************************************************************************
  */

#ifndef __ADC_READ_H
#define __ADC_READ_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief  读取光敏传感器的 ADC 原始值
 * @param  无
 * @retval 光敏传感器的 ADC 值，范围为 0 到 4095
 */
uint16_t ADC_ReadLight(void);

/**
 * @brief  读取滑动变阻器的 ADC 原始值
 * @param  无
 * @retval 滑动变阻器的 ADC 值，范围为 0 到 4095
 */
uint16_t ADC_ReadReference(void);

#ifdef __cplusplus
}
#endif

#endif /* __ADC_READ_H */
