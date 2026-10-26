/**
  ******************************************************************************
  * @file    UI_Terminal.c
  * @brief   OLED 后台终端界面（调试日志）实现
  * @note    顶部 5 组状态图标 + 下方 6 行日志。日志由 fputc 逐字符送入，
  *          主循环 Service() 负责真正的刷新，不在中断里绘制。
  ******************************************************************************
  */

#include "UI_Terminal.h"
#include "main.h"
#include "Oled.h"
#include "UI_Images.h"
#include "UI_Terminal_Font.h"
#include <string.h>

/* 日志行数 / 每行字符数（5 列字形 + 1 列间距 = 6 列/字符，128/6=21） */
#define TERM_LINES          6
#define TERM_COLS           21

/* 顶部图标列坐标（16 宽 + 12 间距 = 28 步进，5 个均分 128 宽） */
#define BT_X                0u
#define WIFI_X              28u
#define CLOUD_X             56u
#define ARMED_X             84u
#define LIGHT_X             112u
#define ICON_PAGE           0u

/* 日志区起始页（页 2~7） */
#define LOG_PAGE            2u

/* ---------------- 状态变量 ---------------- */

static uint8_t s_bt_connected = 0;
static uint8_t s_wifi_connected = 0;
static uint8_t s_cloud_connected = 0;
static uint8_t s_armed = 0;
static uint8_t s_light_bright = 0;
static uint8_t last_bt = 0xFF;
static uint8_t last_wifi = 0xFF;
static uint8_t last_cloud = 0xFF;
static uint8_t last_armed = 0xFF;
static uint8_t last_light = 0xFF;

/* 日志缓冲：6 行，每行 21 字符 + 结束符 */
static char    log_buf[TERM_LINES][TERM_COLS + 1];
static uint8_t cur_col = 0;
static uint8_t log_dirty = 1;

/* 用于清空一页的全 0 位图 */
static const unsigned char blank_128[128] = {0};

/* ---------------- 点阵查找表 ---------------- */

static const unsigned char *const bt_icon[2] =
    { bt_disconnected, bt_connected };

static const unsigned char *const wifi_icon[2] =
    { wifi_disconnected, wifi_connected };

static const unsigned char *const cloud_icon[2] =
    { cloud_disconnected, cloud_connected };

static const unsigned char *const armed_icon[2] =
    { disarmed, armed };

static const unsigned char *const light_icon[2] =
    { light_dark, light_bright };

/* ---------------- 内部函数 ---------------- */

static void UI_Terminal_DrawIcon(uint8_t x, const unsigned char *icon)
{
    IIC_OLED_DrawBMP(x, ICON_PAGE, (unsigned char)(x + 16u),
                     (unsigned char)(ICON_PAGE + 2u), icon);
}

/* 日志上滚：整体上移一行，最下行清空 */
static void UI_Terminal_Scroll(void)
{
    uint8_t i;
    for (i = 0; i < TERM_LINES - 1u; i++)
    {
        memcpy(log_buf[i], log_buf[i + 1u], TERM_COLS + 1u);
    }
    memset(log_buf[TERM_LINES - 1u], 0, TERM_COLS + 1u);
    cur_col = 0;
    log_dirty = 1;
}

/* 清空日志区某一页 */
static void UI_Terminal_ClearPage(uint8_t page)
{
    IIC_OLED_DrawBMP(0, page, 128, (unsigned char)(page + 1u), blank_128);
}

/* 重绘日志区某一行（5x7 小字体） */
static void UI_Terminal_DrawLine(uint8_t line_idx)
{
    uint8_t page = (uint8_t)(LOG_PAGE + line_idx);
    char    *s = log_buf[line_idx];
    uint8_t col = 0;

    UI_Terminal_ClearPage(page);
    IIC_OLED_Set_Pos(0, page);

    while (*s != '\0' && col < TERM_COLS)
    {
        const uint8_t *glyph = font5x7[(uint8_t)(*s) - 0x20u];
        uint8_t t;
        for (t = 0; t < 5u; t++)
        {
            IIC_OLED_WR_Byte(glyph[t], OLED_DATA);
        }
        IIC_OLED_WR_Byte(0x00, OLED_DATA);  /* 字符间距 */
        col++;
        s++;
    }
}

/* ---------------- 对外接口 ---------------- */

void UI_Terminal_Init(void)
{
    IIC_OLED_Clear();

    s_bt_connected = 0;
    s_wifi_connected = 0;
    s_cloud_connected = 0;
    s_armed = 0;
    s_light_bright = 0;

    last_bt = 0;
    last_wifi = 0;
    last_cloud = 0;
    last_armed = 0;
    last_light = 0;

    memset(log_buf, 0, sizeof(log_buf));
    cur_col = 0;
    log_dirty = 1;

    UI_Terminal_DrawIcon(BT_X, bt_disconnected);
    UI_Terminal_DrawIcon(WIFI_X, wifi_disconnected);
    UI_Terminal_DrawIcon(CLOUD_X, cloud_disconnected);
    UI_Terminal_DrawIcon(ARMED_X, disarmed);
    UI_Terminal_DrawIcon(LIGHT_X, light_dark);
}

void UI_Terminal_PutChar(char c)
{
    if (c == '\n')
    {
        UI_Terminal_Scroll();
        return;
    }
    if (c == '\r')
    {
        return;  /* 忽略回车，由 \n 统一处理换行 */
    }
    if (c >= 0x20 && c <= 0x7E)
    {
        if (cur_col < TERM_COLS)
        {
            log_buf[TERM_LINES - 1u][cur_col] = c;
            cur_col++;
            log_buf[TERM_LINES - 1u][cur_col] = '\0';
            log_dirty = 1;
        }
        if (cur_col >= TERM_COLS)
        {
            UI_Terminal_Scroll();  /* 行满自动换行 */
        }
    }
    /* 其他字符（中文等）忽略 */
}

void UI_Terminal_SetBt(uint8_t connected)
{
    s_bt_connected = (connected != 0) ? 1u : 0u;
}

void UI_Terminal_SetWifi(uint8_t connected)
{
    s_wifi_connected = (connected != 0) ? 1u : 0u;
}

void UI_Terminal_SetCloud(uint8_t connected)
{
    s_cloud_connected = (connected != 0) ? 1u : 0u;
}

void UI_Terminal_SetArmed(uint8_t v)
{
    s_armed = (v != 0) ? 1u : 0u;
}

void UI_Terminal_SetLight(uint8_t bright)
{
    s_light_bright = (bright != 0) ? 1u : 0u;
}

void UI_Terminal_Service(void)
{
    uint8_t  i;

    if (s_bt_connected != last_bt)
    {
        UI_Terminal_DrawIcon(BT_X, bt_icon[s_bt_connected]);
        last_bt = s_bt_connected;
    }
    if (s_wifi_connected != last_wifi)
    {
        UI_Terminal_DrawIcon(WIFI_X, wifi_icon[s_wifi_connected]);
        last_wifi = s_wifi_connected;
    }
    if (s_cloud_connected != last_cloud)
    {
        UI_Terminal_DrawIcon(CLOUD_X, cloud_icon[s_cloud_connected]);
        last_cloud = s_cloud_connected;
    }
    if (s_armed != last_armed)
    {
        UI_Terminal_DrawIcon(ARMED_X, armed_icon[s_armed]);
        last_armed = s_armed;
    }
    if (s_light_bright != last_light)
    {
        UI_Terminal_DrawIcon(LIGHT_X, light_icon[s_light_bright]);
        last_light = s_light_bright;
    }

    if (log_dirty)
    {
        log_dirty = 0;
        for (i = 0; i < TERM_LINES; i++)
        {
            UI_Terminal_DrawLine(i);
        }
    }
}
