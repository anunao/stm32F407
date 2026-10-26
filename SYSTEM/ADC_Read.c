/**
  ******************************************************************************
  * @file    ADC_Read.c
  * @brief   ADC 采样驱动程序
  * @note    ADC3 用于读取 PF7 光敏传感器。
  *          ADC1 用于读取 PA5 滑动变阻器。
  ******************************************************************************
  */

#include "ADC_Read.h"
#include "main.h"

/*
 * ADC 句柄由 CubeMX 在 main.c 中定义，
 * 这里使用 extern 表示使用其他文件中的变量。
 */
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc3;

/**
 * @brief  读取光敏传感器的 ADC 原始值
 * @param  无
 * @retval 光敏传感器的 ADC 值，范围为 0 到 4095
 */
uint16_t ADC_ReadLight(void)
{
    uint16_t value = 0;

    /*
     * 启动 ADC3 转换。
     * ADC3 的通道已经由 CubeMX 配置为 PF7 光敏传感器。
     */
    if (HAL_ADC_Start(&hadc3) != HAL_OK)
    {
        return 0;
    }

    /*
     * 等待 ADC3 转换完成，最长等待 100 毫秒。
     */
    if (HAL_ADC_PollForConversion(&hadc3, 100) == HAL_OK)
    {
        value = (uint16_t)HAL_ADC_GetValue(&hadc3);
    }

    /* 读取完成后停止 ADC3。 */
    HAL_ADC_Stop(&hadc3);

    return value;
}

/**
 * @brief  读取滑动变阻器的 ADC 原始值
 * @param  无
 * @retval 滑动变阻器的 ADC 值，范围为 0 到 4095
 */
uint16_t ADC_ReadReference(void)
{
    uint16_t value = 0;

    /*
     * 启动 ADC1 转换。
     * ADC1 的通道已经由 CubeMX 配置为 PA5 滑动变阻器。
     */
    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        return 0;
    }

    /*
     * 等待 ADC1 转换完成，最长等待 100 毫秒。
     */
    if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK)
    {
        value = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }

    /* 读取完成后停止 ADC1。 */
    HAL_ADC_Stop(&hadc1);

    return value;
}
