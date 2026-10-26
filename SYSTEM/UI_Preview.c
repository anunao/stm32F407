/**
  ******************************************************************************
  * @file    UI_Preview.c
  * @brief   OLED 前台界面（预览模式）实现
  * @note    只负责界面显示，不参与业务控制；所有状态由主循环通过 setter
  *          接口同步进来。内部只做「内容变化才刷新」，不在中断里绘制。
  * @note    内部状态变量统一加 s_ 前缀，避免与 UI_Images.h 中点阵数组名
  *          （bt_connected / wifi_connected 等）重名。
  ******************************************************************************
  */

#include "UI_Preview.h"
#include "main.h"
#include "Oled.h"
#include "UI_Images.h"

/* 手动表情显示 5 秒后自动恢复睡眠 */
#define MANUAL_FACE_MS      5000U

/* 16x16 图标：占 2 页；64x64 表情：占 8 页 */
#define ICON_PAGES          2u
#define FACE_PAGES          8u

/* 左半区图标布局（列坐标）：16 宽图标 + 8 列间距 = 24 步进 */
#define BT_X                0u
#define WIFI_X              24u
#define CLOUD_X             48u
#define ARMED_X             0u
#define LIGHT_X             24u
#define ICON_ROW0_PAGE      0u
#define ICON_ROW1_PAGE      2u

/* ---------------- 状态变量 ---------------- */

static uint8_t s_bt_connected = 0;      /* 蓝牙连接状态 */
static uint8_t s_wifi_connected = 0;    /* Wi-Fi 连接状态 */
static uint8_t s_cloud_connected = 0;   /* 云端连接状态 */
static uint8_t s_armed = 0;             /* 布防状态 */
static uint8_t s_alarm = 0;             /* 报警状态 */
static uint8_t s_light_bright = 0;      /* 光照状态：1=亮 0=暗 */

static uint8_t  s_manual_active = 0;    /* 手动表情是否生效 */
static UI_Face  s_manual_face = UI_FACE_SLEEP;
static uint32_t s_manual_start = 0;     /* 手动表情开始时间 */

/* 上一次已绘制状态，用于变化检测 */
static uint8_t last_bt = 0xFF;
static uint8_t last_wifi = 0xFF;
static uint8_t last_cloud = 0xFF;
static uint8_t last_armed = 0xFF;
static uint8_t last_light = 0xFF;
static uint8_t last_face = 0xFF;

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

static const unsigned char *const face_table[4] =
    { face_sleep, face_alert, face_happy, face_scared };

/* ---------------- 内部函数 ---------------- */

/**
 * @brief  在指定位置绘制一个 16x16 状态图标
 */
static void UI_Preview_DrawIcon(uint8_t x, uint8_t page, const unsigned char *icon)
{
    IIC_OLED_DrawBMP(x, page, (unsigned char)(x + 16u),
                     (unsigned char)(page + ICON_PAGES), icon);
}

/**
 * @brief  在右侧绘制 64x64 小猫表情
 */
static void UI_Preview_DrawFace(UI_Face face)
{
    IIC_OLED_DrawBMP(64, 0, 128, FACE_PAGES, face_table[face]);
}

/* ---------------- 对外接口 ---------------- */

void UI_Preview_Init(void)
{
    IIC_OLED_Clear();

    /* 初始状态：全部未连接 / 撤防 / 暗 / 睡眠 */
    s_bt_connected = 0;
    s_wifi_connected = 0;
    s_cloud_connected = 0;
    s_armed = 0;
    s_alarm = 0;
    s_light_bright = 0;
    s_manual_active = 0;
    s_manual_face = UI_FACE_SLEEP;

    last_bt = 0;
    last_wifi = 0;
    last_cloud = 0;
    last_armed = 0;
    last_light = 0;
    last_face = UI_FACE_SLEEP;

    UI_Preview_DrawIcon(BT_X, ICON_ROW0_PAGE, bt_disconnected);
    UI_Preview_DrawIcon(WIFI_X, ICON_ROW0_PAGE, wifi_disconnected);
    UI_Preview_DrawIcon(CLOUD_X, ICON_ROW0_PAGE, cloud_disconnected);
    UI_Preview_DrawIcon(ARMED_X, ICON_ROW1_PAGE, disarmed);
    UI_Preview_DrawIcon(LIGHT_X, ICON_ROW1_PAGE, light_dark);
    UI_Preview_DrawFace(UI_FACE_SLEEP);
}

void UI_Preview_SetBt(uint8_t connected)
{
    s_bt_connected = (connected != 0) ? 1u : 0u;
}

void UI_Preview_SetWifi(uint8_t connected)
{
    s_wifi_connected = (connected != 0) ? 1u : 0u;
}

void UI_Preview_SetCloud(uint8_t connected)
{
    s_cloud_connected = (connected != 0) ? 1u : 0u;
}

void UI_Preview_SetArmed(uint8_t v)
{
    s_armed = (v != 0) ? 1u : 0u;
}

void UI_Preview_SetAlarm(uint8_t v)
{
    s_alarm = (v != 0) ? 1u : 0u;
}

void UI_Preview_SetLight(uint8_t bright)
{
    s_light_bright = (bright != 0) ? 1u : 0u;
}

void UI_Preview_SetManualFace(UI_Face face)
{
    s_manual_face = face;
    s_manual_active = 1;
    s_manual_start = HAL_GetTick();
}

void UI_Preview_Service(void)
{
    uint32_t now = HAL_GetTick();
    UI_Face  target;

    /* 手动表情超时，恢复默认 */
    if (s_manual_active && (now - s_manual_start) >= MANUAL_FACE_MS)
    {
        s_manual_active = 0;
    }

    /* 计算目标表情：报警 > 布防警觉 > 手动 > 睡眠 */
    if (s_alarm)
    {
        target = UI_FACE_SCARED;
    }
    else if (s_armed)
    {
        target = UI_FACE_ALERT;
    }
    else if (s_manual_active)
    {
        target = s_manual_face;
    }
    else
    {
        target = UI_FACE_SLEEP;
    }

    /* 内容变化才刷新 */
    if (s_bt_connected != last_bt)
    {
        UI_Preview_DrawIcon(BT_X, ICON_ROW0_PAGE, bt_icon[s_bt_connected]);
        last_bt = s_bt_connected;
    }

    if (s_wifi_connected != last_wifi)
    {
        UI_Preview_DrawIcon(WIFI_X, ICON_ROW0_PAGE, wifi_icon[s_wifi_connected]);
        last_wifi = s_wifi_connected;
    }

    if (s_cloud_connected != last_cloud)
    {
        UI_Preview_DrawIcon(CLOUD_X, ICON_ROW0_PAGE, cloud_icon[s_cloud_connected]);
        last_cloud = s_cloud_connected;
    }

    if (s_armed != last_armed)
    {
        UI_Preview_DrawIcon(ARMED_X, ICON_ROW1_PAGE, armed_icon[s_armed]);
        last_armed = s_armed;
    }

    if (s_light_bright != last_light)
    {
        UI_Preview_DrawIcon(LIGHT_X, ICON_ROW1_PAGE, light_icon[s_light_bright]);
        last_light = s_light_bright;
    }

    if (target != last_face)
    {
        UI_Preview_DrawFace(target);
        last_face = target;
    }
}
