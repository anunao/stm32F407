/**
  ******************************************************************************
  * @file    SERVO.c
  * @brief   舵机驱动程序
  * @note    舵机信号线连接到 PE9，也就是 TIM1_CH1。
  *          TIM1 配置为 1 MHz 计数频率、20 ms PWM 周期。
  *
  *          扫描模式：10°→170°→10°，每 25 ms 变 2°，完整往返 4 秒。
  ******************************************************************************
  */

#include "SERVO.h"
#include "main.h"

/*
 * htim1 由 CubeMX 在 main.c 中定义，
 * 这里使用 extern 表示使用其他文件中的定时器变量。
 */
extern TIM_HandleTypeDef htim1;

/* 扫描步进角度 / 步进间隔 */
#define SCAN_STEP_ANGLE     2u
#define SCAN_STEP_MS        25u

/* ---------------- 工作状态 ---------------- */

static SERVO_Mode s_mode = SERVO_MODE_DISARM;    /* 当前工作模式 */
static uint8_t    s_scan_angle = SERVO_SCAN_MIN; /* 当前扫描角度 */
static int8_t     s_scan_dir = 1;                /* 1=递增，-1=递减 */
static uint32_t   s_last_step_tick = 0;          /* 上一步步进时间 */

/**
 * @brief  启动舵机 PWM 输出
 */
void SERVO_Start(void)
{
    /*
     * 启动时先让舵机转到 0 度位置。
     * 500 微秒高电平对应大约 0 度。
     */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 500);

    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
}

/**
 * @brief  设置舵机角度
 */
void SERVO_SetAngle(uint8_t angle)
{
    uint16_t pulse_width;

    /*
     * 防止输入角度超过舵机允许范围。
     */
    if (angle > SERVO_MAX_ANGLE)
    {
        angle = SERVO_MAX_ANGLE;
    }

    /*
     * 角度与高电平时间的对应关系：
     * 0 度   -> 500 微秒
     * 180 度 -> 2500 微秒
     */
    pulse_width = 500 + ((uint32_t)angle * 2000 / 180);

    /*
     * 修改 TIM1 通道 1 的比较值，
     * 从而改变 PWM 高电平时间。
     */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pulse_width);
}

/**
 * @brief  设置舵机工作模式
 */
void SERVO_SetMode(SERVO_Mode mode)
{
    if (mode == s_mode)
    {
        return;
    }

    s_mode = mode;

    if (mode == SERVO_MODE_DISARM)
    {
        SERVO_SetAngle(0);
    }
    else if (mode == SERVO_MODE_ALARM)
    {
        SERVO_SetAngle(SERVO_ALARM_ANGLE);
    }
    else /* SERVO_MODE_SCAN */
    {
        s_scan_angle = SERVO_SCAN_MIN;
        s_scan_dir = 1;
        SERVO_SetAngle(s_scan_angle);
    }

    s_last_step_tick = HAL_GetTick();
}

/**
 * @brief  推进舵机扫描状态机
 */
void SERVO_Service(void)
{
    uint32_t now;
    int16_t  next;

    if (s_mode != SERVO_MODE_SCAN)
    {
        return;
    }

    now = HAL_GetTick();
    if (now - s_last_step_tick < SCAN_STEP_MS)
    {
        return;
    }
    s_last_step_tick = now;

    next = (int16_t)s_scan_angle + (int16_t)s_scan_dir * (int16_t)SCAN_STEP_ANGLE;

    if (next >= SERVO_SCAN_MAX)
    {
        s_scan_angle = SERVO_SCAN_MAX;
        s_scan_dir = -1;
    }
    else if (next <= SERVO_SCAN_MIN)
    {
        s_scan_angle = SERVO_SCAN_MIN;
        s_scan_dir = 1;
    }
    else
    {
        s_scan_angle = (uint8_t)next;
    }

    SERVO_SetAngle(s_scan_angle);
}
