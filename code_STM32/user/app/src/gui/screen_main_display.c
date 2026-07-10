/**
 * @file    screen_main_display.c
 * @brief   SignalX GUI - 主监控画面 (Tab 3)
 *
 *  整体布局（从上到下四部分）：
 *   1. 标题/状态栏：左侧标题+状态 | 右侧日期时间
 *   2. 修整监控 - 通用信号曲线区（已实现折线图）
 *   3. 修整轮廓 - 振动信号曲线区（待实现）
 *   4. 底部操作栏：开始采集等（待实现）
 *
 *  当前已实现：第1部分 - 标题/状态栏；第2部分 - 可滚动图表区框架；
 *  第4部分 - 底部操作栏
 *
 *  标题栏布局：
 *   ┌────────────────────────────────────────────────────────┐
 *   │ Main Display  ● Normal        2026-01-01 00:00:00      │
 *   └────────────────────────────────────────────────────────┘
 *   左侧：标题 + 绿色LED + 系统状态（点击弹出报警列表）
 *   右侧：日期+时间（精确到秒，每秒刷新）
 *   淡灰色背景，所有元素上下居中
 *
 *  Extern objects for gui_core.c to push updates:
 *   gui_main_label_time, gui_main_status_led, gui_main_status_label,
 *   gui_main_btn_alarm_reset, gui_main_btn_start_stop, gui_main_label_sp1/sp2
 */

/* ==================== Includes ==================== */
#include "lvgl.h"
#include "gui_core.h"
#include "gui_widgets.h"
#include "config.h"
#include "channel_data.h"
#include "alarm_manager.h"
#include "acq_task.h"
#include "signal_algo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* 平台头文件: STM32 用 FatFs 读取包络线 CSV 文件 */
#if defined(PC_SIMULATOR)
  #include <time.h>
#else
  #include "ff.h"
  #include "stm32f4xx_hal.h"
#endif

/* ==================== 布局常量 ==================== */
#define STATUS_BAR_H         GUI_STATUS_BAR_H   /* 状态栏高度 35px */
#define STATUS_BAR_PAD       10                  /* 状态栏左右内边距 */
#define STATUS_LED_SIZE      10                  /* 状态指示灯直径 */
#define TITLE_STATUS_GAP      8                  /* 标题与状态区间距 */
#define STATUS_LED_TEXT_GAP   5                  /* LED与状态文字间距 */

/* 图表区域布局 */
#define CHART_HEIGHT        165                  /* 图表绘图区高度 */
#define CHART_X_LABEL_H      22                  /* X轴刻度标签预留高度 */
#define CHART_INNER_PAD      10                  /* legend 容器内边距 */
#define CHART_POINTS_MAX   GUI_CHART_POINTS_MAX
#define CHART_DIV_H           5                  /* 水平分割线数 */
#define CHART_DIV_V          12                  /* 垂直分割线数 */
#define CHART_X_TICKS         7                  /* X轴主刻度：0~窗口秒，步进 5s */
#define CHART_Y_TICKS         6                  /* Y轴主刻度个数 (0~100) */
#define CHART_Y_LABEL_W      44                  /* Y轴名称列宽：折行竖排，每行一个单词，无需旋转 */
#define CHART_AXIS_FONT      &lv_font_montserrat_10 /* 坐标轴刻度小字体 */
#define CHART_ZOOM_X        256                  /* 256=100% 缩放，整窗时间轴一屏展示 */
#define SECTION_GAP          10                  /* 图表区块间距 */
#define SCROLL_PAD            8                  /* 滚动区内边距 */

/* 振动信号图表与通用图共用点数/时间窗（config.h ACQ_UI_UPDATE_MS） */
#define VIB_X_TICKS         CHART_X_TICKS
#define VIB_ENV_MAX_PTS    200                   /* 包络线文件最大数据点数 */

/* 整体布局：Tab栏 + 状态栏 + 滚动区 + 底部操作栏 */
#define TAB_BAR_H            36                  /* Tabview 标签栏高度 */
#define FOOTER_BAR_H         42                  /* 底部操作栏高度 */
#define MAIN_SCROLL_H        (LCD_HEIGHT - TAB_BAR_H - STATUS_BAR_H - FOOTER_BAR_H)

/* 通用信号系列颜色（最多8通道） */
static const lv_color_t s_gen_colors[CHANNEL_DATA_MAX] = {
    LV_COLOR_MAKE(0x42, 0xA5, 0xF5),
    LV_COLOR_MAKE(0x66, 0xBB, 0x6A),
    LV_COLOR_MAKE(0xFF, 0xA7, 0x26),
    LV_COLOR_MAKE(0xAB, 0x47, 0xBC),
    LV_COLOR_MAKE(0xEF, 0x53, 0x50),
    LV_COLOR_MAKE(0x26, 0xA6, 0xDA),
    LV_COLOR_MAKE(0x8D, 0x6E, 0x63),
    LV_COLOR_MAKE(0xEC, 0x40, 0x7A),
};

/* 振动信号系列颜色：红、粉、青、黄 */
static const lv_color_t s_vib_colors[4] = {
    LV_COLOR_MAKE(0xEF, 0x53, 0x50),
    LV_COLOR_MAKE(0xEC, 0x40, 0x7A),
    LV_COLOR_MAKE(0x26, 0xC6, 0xDA),
    LV_COLOR_MAKE(0xFF, 0xEE, 0x58)
};

/* ==================== Extern objects (used by gui_core.c) ==================== */
/* 曲线相关 */
lv_obj_t *gui_main_chart_general   = NULL;
lv_obj_t *gui_main_chart_vibration = NULL;
lv_chart_series_t *gui_main_series_general[CHANNEL_DATA_MAX] = {NULL};
lv_chart_series_t *gui_main_series_vib[4]     = {NULL};
lv_chart_series_t *gui_main_series_env_upper  = NULL;
lv_chart_series_t *gui_main_series_env_lower  = NULL;
uint8_t gui_main_gen_series_count = 0;

/* 状态栏控件 */
lv_obj_t *gui_main_label_time       = NULL;
lv_obj_t *gui_main_status_led       = NULL;
lv_obj_t *gui_main_status_label     = NULL;

/* 底部操作栏控件 - 暂未实现，保持 NULL */
lv_obj_t *gui_main_btn_start_stop   = NULL;
lv_obj_t *gui_main_label_start_stop = NULL;
lv_obj_t *gui_main_label_sp1        = NULL;
lv_obj_t *gui_main_label_sp2        = NULL;
lv_obj_t *gui_main_btn_alarm_reset  = NULL;

/* ==================== 私有变量 ==================== */
static lv_obj_t *s_status_bar        = NULL;   /* 状态栏容器 */
static lv_timer_t *s_time_timer      = NULL;   /* 时间刷新定时器 */
static lv_timer_t *s_blink_timer     = NULL;   /* 报警闪烁定时器 */
static bool      s_blink_visible     = true;   /* 闪烁可见状态 */
static uint8_t   s_alarm_level       = 0;      /* 0=正常, 1=单报警, 2=双报警 */
static uint32_t  s_chart_box_height      = 300;    /* 图表容器高度（像素） */
static uint32_t  s_sp1_count           = 0;        /* 修整循环计数 SP1 */
static uint32_t  s_sp2_count           = 0;        /* 修整循环计数 SP2 */
static uint8_t   s_gen_ch_ids[CHANNEL_DATA_MAX]; /* 已启用通用通道 ID 列表 */
static uint8_t   s_gen_ch_count        = 0;        /* 已启用通用通道数量 */
static bool      s_gen_series_hidden[CHANNEL_DATA_MAX]; /* 图例控制的显示/隐藏 */
static uint8_t   s_legend_series_idx[CHANNEL_DATA_MAX]; /* 图例项 → 序列索引 */

/* 振动图表私有变量 */
static bool      s_vib_series_hidden[3] = {false, false, false}; /* [0]=峰峰值 [1]=上包络 [2]=下包络 */
static uint8_t   s_vib_legend_idx[3];   /* 图例项索引：0/1/2 */

/* 包络线数据（从CSV文件加载，最多 VIB_ENV_MAX_PTS 个点） */
static float     s_env_upper_y[VIB_ENV_MAX_PTS]; /* 上包络线浮点Y值（已含百分比计算） */
static float     s_env_lower_y[VIB_ENV_MAX_PTS]; /* 下包络线浮点Y值（已含百分比计算） */
static uint16_t  s_env_point_count = 0;           /* 有效点数，0 = 未加载或文件无效 */
static float     s_env_norm_scale  = 1.0f;        /* 包络归一化系数（映射到 chart 0~100） */

/* ==================== Forward declarations ==================== */
static void status_bar_time_timer_cb(lv_timer_t *timer);
static void status_bar_blink_timer_cb(lv_timer_t *timer);
static void status_click_cb(lv_event_t *e);
static void alarm_popup_close_cb(lv_event_t *e);
static void chart_draw_event_cb(lv_event_t *e);
static void main_btn_start_stop_cb(lv_event_t *e);
static void main_btn_clear_cb(lv_event_t *e);
static void main_btn_alarm_reset_cb(lv_event_t *e);
static void main_sp_update_labels(void);
static void main_acq_sync_button_ui(void);
static void main_create_general_chart(lv_obj_t *cont);
#if defined(ACQ_USE_DEMO_DATA)
static void main_gen_fill_demo_data(lv_obj_t *chart);
#endif
static void main_gen_init_chart_zero(lv_obj_t *chart);
static uint8_t main_collect_enabled_general(void);
static void main_gen_legend_click_cb(lv_event_t *e);

/* 振动信号图表 */
static bool     vib_load_envelope_file(const char *path,
                                        uint8_t upper_pct, uint8_t lower_pct);
static void     vib_chart_draw_event_cb(lv_event_t *e);
static void     main_vib_legend_click_cb(lv_event_t *e);
#if defined(ACQ_USE_DEMO_DATA)
static void     main_vib_fill_demo_data(lv_obj_t *chart);
#endif
static void     main_vib_init_pp_chart_zero(lv_obj_t *chart);
static void     main_vib_fill_envelope(lv_obj_t *chart, const AcqParams_t *params);
static uint16_t main_chart_window_sec(void);
static void     main_chart_scroll_to_start(lv_obj_t *chart);

static uint16_t main_chart_window_sec(void)
{
    return (uint16_t)(((uint32_t)CHART_POINTS_MAX * (uint32_t)ACQ_UI_UPDATE_MS) / 1000U);
}

static void main_chart_scroll_to_start(lv_obj_t *chart)
{
    if (chart != NULL) {
        lv_obj_scroll_to_x(chart, 0, LV_ANIM_OFF);
    }
}
static void     main_create_vibration_chart(lv_obj_t *cont);

/* ==================== 时间刷新（1秒） ==================== */

#if !defined(PC_SIMULATOR)

static bool sys_time_is_leap(uint16_t year)
{
    return ((year % 4U) == 0U && (year % 100U) != 0U) || ((year % 400U) == 0U);
}

static uint32_t sys_time_datetime_to_epoch(uint16_t year, uint8_t month, uint8_t day,
                                           uint8_t hour, uint8_t minute, uint8_t second)
{
    static const uint16_t month_days[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    uint32_t days = 0U;

    for (uint16_t y = 1970U; y < year; y++) {
        days += sys_time_is_leap(y) ? 366U : 365U;
    }
    if (month >= 1U && month <= 12U) {
        days += month_days[month - 1U];
        if (month > 2U && sys_time_is_leap(year)) {
            days++;
        }
    }
    if (day > 0U) {
        days += (uint32_t)day - 1U;
    }

    return days * 86400UL + (uint32_t)hour * 3600UL +
           (uint32_t)minute * 60UL + (uint32_t)second;
}

static void sys_time_epoch_to_datetime(uint32_t epoch, uint16_t *year, uint8_t *month,
                                       uint8_t *day, uint8_t *hour, uint8_t *minute,
                                       uint8_t *second)
{
    *second = (uint8_t)(epoch % 60U);
    epoch /= 60U;
    *minute = (uint8_t)(epoch % 60U);
    epoch /= 60U;
    *hour = (uint8_t)(epoch % 24U);
    epoch /= 24U;

    uint16_t y = 1970U;
    while (true) {
        uint32_t diy = sys_time_is_leap(y) ? 366U : 365U;
        if (epoch < diy) {
            break;
        }
        epoch -= diy;
        y++;
    }

    static const uint8_t dim[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint8_t m = 1U;
    while (m <= 12U) {
        uint8_t md = dim[m - 1U];
        if (m == 2U && sys_time_is_leap(y)) {
            md = 29U;
        }
        if (epoch < md) {
            break;
        }
        epoch -= md;
        m++;
    }

    *year  = y;
    *month = m;
    *day   = (uint8_t)(epoch + 1U);
}

static uint8_t sys_time_parse_month3(const char *mon)
{
    static const char *names[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    for (uint8_t i = 0U; i < 12U; i++) {
        if (mon[0] == names[i][0] && mon[1] == names[i][1] && mon[2] == names[i][2]) {
            return (uint8_t)(i + 1U);
        }
    }
    return 1U;
}

static uint32_t sys_time_boot_epoch(void)
{
    const char *date = __DATE__;
    const char *time_str = __TIME__;
    uint8_t month = sys_time_parse_month3(date);
    uint8_t day   = (uint8_t)atoi(date + 4);
    uint16_t year = (uint16_t)atoi(date + 7);
    uint8_t hour  = (uint8_t)atoi(time_str);
    uint8_t minute = (uint8_t)atoi(time_str + 3);
    uint8_t second = (uint8_t)atoi(time_str + 6);

    return sys_time_datetime_to_epoch(year, month, day, hour, minute, second);
}

#endif /* !PC_SIMULATOR */

/**
 * @brief  刷新状态栏日期时间
 *
 * PC：读取系统时钟。
 * STM32：编译时刻 + HAL_GetTick()（RTC 未接入前的过渡方案）。
 */
static void main_status_bar_update_time(void)
{
    char buf[24];

    if (gui_main_label_time == NULL) {
        return;
    }

#if defined(PC_SIMULATOR)
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (tm_info != NULL) {
        lv_snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                    tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                    tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    } else {
        lv_snprintf(buf, sizeof(buf), "----/--/-- --:--:--");
    }
#else
    static uint32_t s_boot_epoch = 0U;
    if (s_boot_epoch == 0U) {
        s_boot_epoch = sys_time_boot_epoch();
    }

    uint32_t epoch = s_boot_epoch + (HAL_GetTick() / 1000U);
    uint16_t year;
    uint8_t month, day, hour, minute, second;
    sys_time_epoch_to_datetime(epoch, &year, &month, &day, &hour, &minute, &second);
    lv_snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
                year, month, day, hour, minute, second);
#endif

    lv_label_set_text(gui_main_label_time, buf);
}

static void status_bar_time_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    main_status_bar_update_time();
}

/* ==================== 报警闪烁定时器 ==================== */

/**
 * @brief  报警状态下状态文字和LED的闪烁回调
 *
 * 定时器周期根据 s_alarm_level 动态调整：
 *   - 单报警：1000ms（1次/秒）
 *   - 双报警：500ms（2次/秒）
 */
static void status_bar_blink_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    s_blink_visible = !s_blink_visible;

    if (gui_main_status_label != NULL) {
        lv_obj_set_style_opa(gui_main_status_label,
                             s_blink_visible ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
    if (gui_main_status_led != NULL) {
        lv_obj_set_style_opa(gui_main_status_led,
                             s_blink_visible ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
}

/* ==================== 状态区点击 → 报警弹窗 ==================== */

/**
 * @brief  点击状态文本区域，弹出当前报警简要列表
 */
static void status_click_cb(lv_event_t *e)
{
    (void)e;

    /* 构建报警简要内容 */
    static char msg_buf[256];
    if (s_alarm_level == 0) {
        lv_snprintf(msg_buf, sizeof(msg_buf), "System normal\nNo active alarms");
    } else {
        lv_snprintf(msg_buf, sizeof(msg_buf),
                    "Active alarms: %d\n"
                    "(Details in Alarm Records tab)",
                    s_alarm_level);
    }

    /* 创建模态弹窗 */
    static const char *btns[] = {"OK", ""};
    lv_obj_t *mbox = lv_msgbox_create(NULL, "Alarm Status", msg_buf, btns, false);
    lv_obj_set_width(mbox, 320);
    lv_obj_center(mbox);

    /* 注册关闭回调 */
    lv_obj_add_event_cb(mbox, alarm_popup_close_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

/**
 * @brief  报警弹窗关闭回调
 */
static void alarm_popup_close_cb(lv_event_t *e)
{
    lv_obj_t *mbox = lv_event_get_current_target(e);
    lv_msgbox_close(mbox);
}

/* ==================== 图表绘制事件回调 ==================== */

/**
 * @brief  图表绘制事件回调 - 自定义X轴刻度标签
 *
 * LVGL 默认用数据点索引作为X轴刻度标签（0, 100, 200...），
 * 本回调将刻度索引映射为 0 ~ 显示时间窗（秒），每点 = ACQ_UI_UPDATE_MS。
 */
static void chart_draw_event_cb(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_DRAW_PART_BEGIN) return;

    lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);
    if (!lv_obj_draw_part_check_type(dsc, &lv_chart_class,
                                     LV_CHART_DRAW_PART_TICK_LABEL)) {
        return;
    }
    if (dsc->text == NULL) return;

    if (dsc->id == LV_CHART_AXIS_PRIMARY_X) {
        int32_t major_idx = dsc->value;
        if (major_idx >= 0 && major_idx < CHART_X_TICKS) {
            uint16_t win_sec = main_chart_window_sec();
            int tick_sec = 0;
            if (CHART_X_TICKS > 1) {
                tick_sec = (int)((uint32_t)win_sec * (uint32_t)major_idx
                                 / (uint32_t)(CHART_X_TICKS - 1));
            }
            lv_snprintf(dsc->text, dsc->text_length, "%d", tick_sec);
        }
    }
}

/* ==================== 通用信号图表 ==================== */

/**
 * @brief  从通道设置收集所有已启用的通用通道
 */
static uint8_t main_collect_enabled_general(void)
{
    s_gen_ch_count = 0;
    uint8_t total = ChannelData_GetCount();
    for (uint8_t i = 0; i < total && s_gen_ch_count < CHANNEL_DATA_MAX; i++) {
        ChannelConfig_t *ch = ChannelData_GetByIndex(i);
        if (ch == NULL || !ch->enabled) continue;
        if (ch->ch_type != CH_TYPE_GENERAL) continue;
        s_gen_ch_ids[s_gen_ch_count++] = ch->channel_id;
    }
    gui_main_gen_series_count = s_gen_ch_count;
    return s_gen_ch_count;
}

/**
 * @brief  将通用折线图 Y 数组清零（无演示数据时的初始状态）
 * @param  chart  通用信号 chart 对象
 */
static void main_gen_init_chart_zero(lv_obj_t *chart)
{
    for (uint8_t ch = 0; ch < s_gen_ch_count; ch++) {
        if (gui_main_series_general[ch] == NULL) {
            continue;
        }
        lv_coord_t *y_arr = lv_chart_get_y_array(chart, gui_main_series_general[ch]);
        if (y_arr == NULL) {
            continue;
        }
        for (uint16_t i = 0; i < CHART_POINTS_MAX; i++) {
            y_arr[i] = 0;
        }
    }
    lv_chart_refresh(chart);
}

#if defined(ACQ_USE_DEMO_DATA)
/**
 * @brief  填充演示数据，便于无后端时预览折线效果（仅 ACQ_USE_DEMO_DATA 启用）
 */
static void main_gen_fill_demo_data(lv_obj_t *chart)
{
    for (uint8_t ch = 0; ch < s_gen_ch_count; ch++) {
        if (gui_main_series_general[ch] == NULL) continue;
        lv_coord_t *y_arr = lv_chart_get_y_array(chart, gui_main_series_general[ch]);
        if (y_arr == NULL) continue;

        for (uint16_t i = 0; i < CHART_POINTS_MAX; i++) {
            float t = (float)i / (float)CHART_POINTS_MAX * 6.2831853f;
            float v = 50.0f + 35.0f * sinf(t * 2.0f + (float)ch * 1.2f);
            y_arr[i] = (lv_coord_t)v;
        }
    }
    lv_chart_refresh(chart);
}
#endif /* ACQ_USE_DEMO_DATA */

/**
 * @brief  图例点击：切换对应折线显示/隐藏
 */
static void main_gen_legend_click_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    uint8_t idx = *(uint8_t *)lv_event_get_user_data(e);
    if (gui_main_chart_general == NULL || idx >= s_gen_ch_count) return;
    if (gui_main_series_general[idx] == NULL) return;

    s_gen_series_hidden[idx] = !s_gen_series_hidden[idx];
    lv_chart_hide_series(gui_main_chart_general,
                         gui_main_series_general[idx],
                         s_gen_series_hidden[idx]);

    lv_obj_t *item = lv_event_get_target(e);
    lv_obj_set_style_opa(item,
                         s_gen_series_hidden[idx] ? LV_OPA_40 : LV_OPA_COVER, 0);
}

/**
 * @brief  在 legend 容器内创建通用信号折线图
 *
 * 参考 LVGL 8.4 lv_example_chart_5（1000点 + 缩放滚动）
 * 及 lv_example_chart_3（坐标轴刻度 + 自定义标签）
 */
static void main_create_general_chart(lv_obj_t *cont)
{
    main_collect_enabled_general();

    /* cont 边框/背景/内边距由 GUI_CreateLegendBox 设置，此处仅追加 flex 布局 */
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_gap(cont, 4, 0);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    /* 图表标题 */
    lv_obj_t *lbl_title = lv_label_create(cont);
    lv_label_set_text(lbl_title, "General Signal");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x333333), 0);
    lv_obj_set_width(lbl_title, LV_PCT(100));
    lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_bottom(lbl_title, 2, 0);

    /* 图表行：Y轴名称 + 折线图（底部预留 X 刻度空间） */
    lv_obj_t *chart_row = lv_obj_create(cont);
    lv_obj_set_width(chart_row, LV_PCT(100));
    lv_obj_set_height(chart_row, CHART_HEIGHT + CHART_X_LABEL_H);
    lv_obj_set_flex_flow(chart_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(chart_row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_bg_opa(chart_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chart_row, 0, 0);
    lv_obj_set_style_pad_all(chart_row, 0, 0);
    lv_obj_set_style_pad_gap(chart_row, 8, 0);
    lv_obj_add_flag(chart_row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_clear_flag(chart_row, LV_OBJ_FLAG_SCROLLABLE);

    /* Y轴名称列：折行竖排，不使用 transform_angle
     * "Signal\nValue" 两行居中显示在 CHART_Y_LABEL_W × CHART_HEIGHT 的列内 */
    lv_obj_t *y_col = lv_obj_create(chart_row);
    lv_obj_set_width(y_col, CHART_Y_LABEL_W);
    lv_obj_set_height(y_col, CHART_HEIGHT);
    lv_obj_set_style_bg_opa(y_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(y_col, 0, 0);
    lv_obj_set_style_pad_all(y_col, 0, 0);
    lv_obj_clear_flag(y_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_y = lv_label_create(y_col);
    lv_label_set_text(lbl_y, "Signal\nValue");
    lv_obj_set_width(lbl_y, CHART_Y_LABEL_W);
    lv_obj_set_height(lbl_y, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(lbl_y, 0, 0);
    lv_obj_set_style_border_width(lbl_y, 0, 0);
    lv_obj_set_style_bg_opa(lbl_y, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_font(lbl_y, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_y, lv_color_hex(0x444444), 0);
    lv_obj_set_style_text_align(lbl_y, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl_y, LV_ALIGN_CENTER, 0, 0);  /* 在 y_col 内水平+垂直居中 */

    /* 折线图（1000点，支持横向滚动与缩放） */
    gui_main_chart_general = lv_chart_create(chart_row);
    lv_obj_set_flex_grow(gui_main_chart_general, 1);
    lv_obj_set_height(gui_main_chart_general, CHART_HEIGHT);
    lv_obj_add_flag(gui_main_chart_general, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_chart_set_type(gui_main_chart_general, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(gui_main_chart_general, CHART_POINTS_MAX);
    lv_chart_set_range(gui_main_chart_general, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_div_line_count(gui_main_chart_general, 0, 0);  /* 不显示内部分割线 */
    lv_chart_set_update_mode(gui_main_chart_general, LV_CHART_UPDATE_MODE_SHIFT);
    lv_obj_set_style_size(gui_main_chart_general, 0, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(gui_main_chart_general, 0, 0);
    lv_obj_set_style_bg_opa(gui_main_chart_general, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_font(gui_main_chart_general, CHART_AXIS_FONT, LV_PART_TICKS);
    lv_obj_set_style_pad_left(gui_main_chart_general, 4, 0);
    lv_obj_set_style_pad_right(gui_main_chart_general, 8, 0);
    lv_obj_set_style_pad_top(gui_main_chart_general, 6, 0);
    lv_obj_set_style_pad_bottom(gui_main_chart_general, 6, 0);
    lv_obj_set_style_pad_bottom(gui_main_chart_general, 2, LV_PART_TICKS);  /* 刻度线与数值间距 */
    lv_obj_add_event_cb(gui_main_chart_general, chart_draw_event_cb,
                        LV_EVENT_DRAW_PART_BEGIN, NULL);

    lv_chart_set_axis_tick(gui_main_chart_general, LV_CHART_AXIS_PRIMARY_X,
                           5, 3, CHART_X_TICKS, 4, true, 22);
    lv_chart_set_axis_tick(gui_main_chart_general, LV_CHART_AXIS_PRIMARY_Y,
                           5, 3, CHART_Y_TICKS, 4, true, 36);

    lv_chart_set_zoom_x(gui_main_chart_general, CHART_ZOOM_X);
    lv_obj_set_scroll_dir(gui_main_chart_general, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(gui_main_chart_general, LV_SCROLLBAR_MODE_AUTO);

    /* 按通道设置动态添加折线（仅 enabled + General 类型） */
    for (uint8_t i = 0; i < s_gen_ch_count; i++) {
        gui_main_series_general[i] = lv_chart_add_series(
            gui_main_chart_general, s_gen_colors[i], LV_CHART_AXIS_PRIMARY_Y);
    }
    for (uint8_t i = s_gen_ch_count; i < CHANNEL_DATA_MAX; i++) {
        gui_main_series_general[i] = NULL;
    }

    if (s_gen_ch_count > 0) {
#if defined(ACQ_USE_DEMO_DATA)
        main_gen_fill_demo_data(gui_main_chart_general);
#else
        main_gen_init_chart_zero(gui_main_chart_general);
#endif
    }

    /* X轴标签 */
    lv_obj_t *lbl_x = lv_label_create(cont);
    lv_label_set_text(lbl_x, "Time(s)");
    lv_obj_set_style_text_font(lbl_x, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_x, lv_color_hex(0x333333), 0);
    lv_obj_set_width(lbl_x, LV_PCT(100));
    lv_obj_set_style_text_align(lbl_x, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(lbl_x, CHART_Y_LABEL_W + 14, 0);

    /* 图例：已启用通用通道名称 */
    lv_obj_t *legend = lv_obj_create(cont);
    lv_obj_set_width(legend, LV_PCT(100));
    lv_obj_set_height(legend, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(legend, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(legend, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(legend, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(legend, 0, 0);
    lv_obj_set_style_pad_all(legend, 0, 0);
    lv_obj_set_style_pad_top(legend, 2, 0);
    lv_obj_set_style_pad_gap(legend, 10, 0);
    lv_obj_clear_flag(legend, LV_OBJ_FLAG_SCROLLABLE);

    for (uint8_t i = 0; i < s_gen_ch_count; i++) {
        ChannelConfig_t *ch = ChannelData_GetById(s_gen_ch_ids[i]);
        const char *ch_name = (ch && ch->channel_name[0]) ? ch->channel_name : "Ch?";

        s_legend_series_idx[i] = i;
        s_gen_series_hidden[i] = false;

        lv_obj_t *item = lv_obj_create(legend);
        lv_obj_set_size(item, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(item, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_opa(item, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(item, 0, 0);
        lv_obj_set_style_pad_all(item, 2, 0);
        lv_obj_set_style_pad_gap(item, 4, 0);
        lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(item, main_gen_legend_click_cb,
                            LV_EVENT_CLICKED, &s_legend_series_idx[i]);

        lv_obj_t *swatch = lv_obj_create(item);
        lv_obj_set_size(swatch, 10, 10);
        lv_obj_set_style_radius(swatch, 1, 0);
        lv_obj_set_style_bg_color(swatch, s_gen_colors[i], 0);
        lv_obj_set_style_bg_opa(swatch, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(swatch, 0, 0);
        lv_obj_clear_flag(swatch, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(swatch, LV_OBJ_FLAG_EVENT_BUBBLE);

        lv_obj_t *name = lv_label_create(item);
        lv_label_set_text(name, ch_name);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(name, lv_color_hex(0x333333), 0);
        lv_obj_add_flag(name, LV_OBJ_FLAG_EVENT_BUBBLE);
    }
}

/* ==================== 包络线文件加载与插值 ==================== */
/*
 * 首期策略（替代 PRD 5.3 在线三次样条生成）：
 *   1. 用户从 SD 卡选择 CSV，每行一个基准点 (X_index, Y_baseline)
 *   2. 加载时按采集设置百分比计算上下界：
 *        upper[i] = baseline[i] × (1 + upper_pct/100)
 *        lower[i] = baseline[i] × (1 − lower_pct/100)
 *   3. 绘图/实时推送时，将离散点线性插值到图表 100 个 X 位置
 *   4. acq_pipeline 通过 GUI_GetVibEnvelopeAtSeq(seq) 按 PP 点序号查询
 */

/**
 * @brief  从 CSV 文件加载包络线基准数据，计算上下包络 Y 值
 *
 * CSV 格式（# 开头为注释行，忽略）：
 *   X_index,Y_value
 *   0,0.5
 *   1,0.8
 *   ...
 *
 * 执行步骤：
 *   1. 清空 s_env_point_count
 *   2. 逐行解析 float,float；Y≥0 才计入
 *   3. 调用 SignalAlgo_EnvelopeFromBaseline 生成上下界数组
 *   4. 至少 2 个点才认为包络有效（插值需要区间）
 *
 * @param  path       CSV 文件路径（为 NULL 或空串时直接返回 false）
 * @param  upper_pct  包络上限百分比（整数，如 20 表示 +20%）
 * @param  lower_pct  包络下限百分比（整数，如 10 表示 −10%）
 * @return true = 成功加载 >= 2 个有效数据点
 */
static bool vib_load_envelope_file(const char *path,
                                    uint8_t upper_pct, uint8_t lower_pct)
{
    float baseline[VIB_ENV_MAX_PTS];
    uint16_t n = 0u;

    s_env_point_count = 0u;
    if (path == NULL || path[0] == '\0') return false;

#if defined(PC_SIMULATOR)
    FILE *fp = fopen(path, "r");
    if (!fp) return false;

    char line[64];
    while (fgets(line, sizeof(line), fp) != NULL && n < VIB_ENV_MAX_PTS) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        float x_val = 0.0f, y_val = 0.0f;
        if (sscanf(line, "%f,%f", &x_val, &y_val) == 2 && y_val >= 0.0f) {
            baseline[n++] = y_val;
        }
    }
    fclose(fp);

#else   /* STM32: FatFs */
    FIL   fil;
    FRESULT res = f_open(&fil, path, FA_READ);
    if (res != FR_OK) return false;

    char line[64];
    while (f_gets(line, sizeof(line), &fil) != NULL && n < VIB_ENV_MAX_PTS) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        float x_val = 0.0f, y_val = 0.0f;
        if (sscanf(line, "%f,%f", &x_val, &y_val) == 2 && y_val >= 0.0f) {
            baseline[n++] = y_val;
        }
    }
    f_close(&fil);
#endif

    if (n < 2u) {
        return false;
    }

    SignalAlgo_EnvelopeFromBaseline(baseline, n, upper_pct, lower_pct,
                                    s_env_upper_y, s_env_lower_y);
    s_env_point_count = n;
    return true;
}

/* vib_interp_env_value / vib_compute_peak_to_peak 已迁至 signal_algo.c */

/* ==================== 振动图表绘制事件回调 ==================== */

/**
 * @brief  振动图表 X 轴刻度标签映射：索引 0~10 → 时间 0~100 秒
 */
static void vib_chart_draw_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DRAW_PART_BEGIN) return;

    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (!lv_obj_draw_part_check_type(dsc, &lv_chart_class,
                                     LV_CHART_DRAW_PART_TICK_LABEL)) return;
    if (dsc->text == NULL) return;

    if (dsc->id == LV_CHART_AXIS_PRIMARY_X) {
        int32_t major_idx = dsc->value;
        if (major_idx >= 0 && major_idx < VIB_X_TICKS) {
            uint16_t win_sec = main_chart_window_sec();
            int tick_sec = 0;
            if (VIB_X_TICKS > 1) {
                tick_sec = (int)((uint32_t)win_sec * (uint32_t)major_idx
                                 / (uint32_t)(VIB_X_TICKS - 1));
            }
            lv_snprintf(dsc->text, dsc->text_length, "%d", tick_sec);
        }
    }
}

/* ==================== 振动图例点击回调 ==================== */

/**
 * @brief  点击图例项切换振动图中对应系列的显示/隐藏
 *
 * user_data 指向 s_vib_legend_idx 数组某元素：
 *   0 = 峰峰值曲线  1 = 上包络线  2 = 下包络线
 */
static void main_vib_legend_click_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (gui_main_chart_vibration == NULL) return;

    uint8_t idx = *(uint8_t *)lv_event_get_user_data(e);
    if (idx > 2u) return;

    lv_chart_series_t *ser = NULL;
    if (idx == 0u)      ser = gui_main_series_vib[0];
    else if (idx == 1u) ser = gui_main_series_env_upper;
    else                ser = gui_main_series_env_lower;

    if (ser == NULL) return;

    s_vib_series_hidden[idx] = !s_vib_series_hidden[idx];
    lv_chart_hide_series(gui_main_chart_vibration, ser,
                         s_vib_series_hidden[idx]);

    lv_obj_t *item = lv_event_get_target(e);
    lv_obj_set_style_opa(item,
        s_vib_series_hidden[idx] ? LV_OPA_40 : LV_OPA_COVER, 0);
}

/* ==================== 振动图演示数据填充（仅 ACQ_USE_DEMO_DATA） ==================== */

#if defined(ACQ_USE_DEMO_DATA)
/**
 * @brief  向峰峰值系列填充演示数据（无硬件时预览图表效果）
 *
 * 生成一段复合正弦波形模拟真实振动峰峰值趋势，
 * Y 值范围：约 22~78（在 0~100 坐标内）。
 */
static void main_vib_fill_demo_data(lv_obj_t *chart)
{
    if (gui_main_series_vib[0] == NULL) return;

    lv_coord_t *y = lv_chart_get_y_array(chart, gui_main_series_vib[0]);
    if (y == NULL) return;

    for (uint16_t i = 0u; i < CHART_POINTS_MAX; i++) {
        float t  = (float)i / (float)CHART_POINTS_MAX * 6.2831853f;
        /* 均值50 + 20×sin(t) + 8×sin(3t) 模拟复合振动峰峰值 */
        float v  = 50.0f
                 + 20.0f * sinf(t)
                 +  8.0f * sinf(3.0f * t + 0.5f);
        y[i] = (lv_coord_t)v;
    }
    lv_chart_refresh(chart);
}
#endif /* ACQ_USE_DEMO_DATA */

/**
 * @brief  振动图峰峰值系列初始化为 0（无 ACQ_USE_DEMO_DATA 时）
 */
static void main_vib_init_pp_chart_zero(lv_obj_t *chart)
{
    if (gui_main_series_vib[0] == NULL) {
        return;
    }

    lv_coord_t *y = lv_chart_get_y_array(chart, gui_main_series_vib[0]);
    if (y == NULL) {
        return;
    }

    for (uint16_t i = 0u; i < CHART_POINTS_MAX; i++) {
        y[i] = 0;
    }
    lv_chart_refresh(chart);
}

/**
 * @brief  将已加载的包络线数据插值写入图表上下包络系列
 *
 * 若包络线未加载（s_env_point_count < 2），则隐藏上下包络系列。
 * 包络 Y 值归一化到图表 Y 范围 0~100：
 *   scale = 90 / max(upper_y)   ← 保留 10% 顶部余量
 *
 * @param  chart   振动图表对象
 * @param  params  采集参数（当前未使用，保留供后续扩展）
 */
static void main_vib_fill_envelope(lv_obj_t *chart, const AcqParams_t *params)
{
    (void)params;

    if (s_env_point_count < 2u) {
        if (gui_main_series_env_upper != NULL)
            lv_chart_hide_series(chart, gui_main_series_env_upper, true);
        if (gui_main_series_env_lower != NULL)
            lv_chart_hide_series(chart, gui_main_series_env_lower, true);
        return;
    }

    /* 计算归一化系数：将最大上包络值映射到 90（保留余量） */
    float env_max = 0.0f;
    for (uint16_t i = 0u; i < s_env_point_count; i++) {
        if (s_env_upper_y[i] > env_max) env_max = s_env_upper_y[i];
    }
    float norm_scale = (env_max > 0.0f) ? (90.0f / env_max) : 1.0f;
    s_env_norm_scale = norm_scale;

    /* 插值填充上包络线 */
    lv_coord_t *upper_arr = lv_chart_get_y_array(chart, gui_main_series_env_upper);
    lv_coord_t *lower_arr = lv_chart_get_y_array(chart, gui_main_series_env_lower);

    for (uint16_t i = 0u; i < CHART_POINTS_MAX; i++) {
        float u = 0.0f;
        float l = 0.0f;
        SignalAlgo_EnvelopeAtChartIdx(s_env_upper_y, s_env_lower_y,
                                      s_env_point_count,
                                      i, CHART_POINTS_MAX, norm_scale,
                                      &u, &l);
        if (upper_arr) {
            upper_arr[i] = (lv_coord_t)u;
        }
        if (lower_arr) {
            lower_arr[i] = (lv_coord_t)l;
        }
    }

    lv_chart_hide_series(chart, gui_main_series_env_upper, false);
    lv_chart_hide_series(chart, gui_main_series_env_lower, false);
    lv_chart_refresh(chart);
}

/* ==================== 振动信号图表创建 ==================== */

/**
 * @brief  创建振动信号折线图（峰峰值曲线 + 上下包络线）
 *
 * 图表结构：
 *   标题行："Vibration Signal - Peak-Peak & Envelope"
 *   图表行：Y轴名称列 + 折线图（100点，支持横向滚动）
 *   X轴标签："Time(s)"
 *   图例行：■ Peak-Peak  ■ Upper Env  ■ Lower Env
 *
 * 数据流：
 *   1. 读取采集参数，加载包络线文件（若配置了路径）
 *   2. 计算包络线系列数据（插值到100点）
 *   3. 填充演示峰峰值数据（无采集后端时预览）
 *
 * @param  cont  GUI_CreateLegendBox() 返回的内层容器
 */
static void main_create_vibration_chart(lv_obj_t *cont)
{
    /* ---- 读取采集参数：获取包络设置 ---- */
    AcqParams_t acq = Config_LoadAcqParams();

    /* ---- 初始化包络线数据 ---- */
    s_env_point_count = 0u;
    bool env_loaded = vib_load_envelope_file(acq.envelope_file_path,
                                             acq.envelope_upper_pct,
                                             acq.envelope_lower_pct);

    /* ---- 容器布局：纵向 Flex ---- */
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_gap(cont, 4, 0);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    /* ---- 图表标题 ---- */
    lv_obj_t *lbl_title = lv_label_create(cont);
    lv_label_set_text(lbl_title, "Vibration Signal - Peak-Peak & Envelope");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x333333), 0);
    lv_obj_set_width(lbl_title, LV_PCT(100));
    lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_bottom(lbl_title, 2, 0);

    /* ---- 图表行：Y轴名称列 + 折线图 ---- */
    lv_obj_t *chart_row = lv_obj_create(cont);
    lv_obj_set_width(chart_row, LV_PCT(100));
    lv_obj_set_height(chart_row, CHART_HEIGHT + CHART_X_LABEL_H);
    lv_obj_set_flex_flow(chart_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(chart_row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_bg_opa(chart_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chart_row, 0, 0);
    lv_obj_set_style_pad_all(chart_row, 0, 0);
    lv_obj_set_style_pad_gap(chart_row, 8, 0);
    lv_obj_add_flag(chart_row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_clear_flag(chart_row, LV_OBJ_FLAG_SCROLLABLE);

    /* Y轴名称列：折行竖排，不使用 transform_angle */
    lv_obj_t *y_col = lv_obj_create(chart_row);
    lv_obj_set_width(y_col, CHART_Y_LABEL_W);
    lv_obj_set_height(y_col, CHART_HEIGHT);
    lv_obj_set_style_bg_opa(y_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(y_col, 0, 0);
    lv_obj_set_style_pad_all(y_col, 0, 0);
    lv_obj_clear_flag(y_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_y = lv_label_create(y_col);
    lv_label_set_text(lbl_y, "Peak-\nPeak");
    lv_obj_set_width(lbl_y, CHART_Y_LABEL_W);
    lv_obj_set_height(lbl_y, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(lbl_y, 0, 0);
    lv_obj_set_style_border_width(lbl_y, 0, 0);
    lv_obj_set_style_bg_opa(lbl_y, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_font(lbl_y, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_y, lv_color_hex(0x444444), 0);
    lv_obj_set_style_text_align(lbl_y, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl_y, LV_ALIGN_CENTER, 0, 0);

    /* ---- 折线图控件（100点，支持横向缩放/滚动） ---- */
    gui_main_chart_vibration = lv_chart_create(chart_row);
    lv_obj_set_flex_grow(gui_main_chart_vibration, 1);
    lv_obj_set_height(gui_main_chart_vibration, CHART_HEIGHT);
    lv_obj_add_flag(gui_main_chart_vibration, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_chart_set_type(gui_main_chart_vibration, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(gui_main_chart_vibration, CHART_POINTS_MAX);
    lv_chart_set_range(gui_main_chart_vibration, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_div_line_count(gui_main_chart_vibration, 0, 0);
    lv_chart_set_update_mode(gui_main_chart_vibration,
                             LV_CHART_UPDATE_MODE_SHIFT);
    lv_obj_set_style_size(gui_main_chart_vibration, 0, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(gui_main_chart_vibration, 0, 0);
    lv_obj_set_style_bg_opa(gui_main_chart_vibration, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_font(gui_main_chart_vibration,
                               CHART_AXIS_FONT, LV_PART_TICKS);
    lv_obj_set_style_pad_left(gui_main_chart_vibration, 4, 0);
    lv_obj_set_style_pad_right(gui_main_chart_vibration, 8, 0);
    lv_obj_set_style_pad_top(gui_main_chart_vibration, 6, 0);
    lv_obj_set_style_pad_bottom(gui_main_chart_vibration, 6, 0);
    lv_obj_set_style_pad_bottom(gui_main_chart_vibration, 2, LV_PART_TICKS);
    lv_obj_add_event_cb(gui_main_chart_vibration, vib_chart_draw_event_cb,
                        LV_EVENT_DRAW_PART_BEGIN, NULL);

    /* X轴：11个主刻度（0~100s, 步长10s），4个次刻度 */
    lv_chart_set_axis_tick(gui_main_chart_vibration, LV_CHART_AXIS_PRIMARY_X,
                           5, 3, VIB_X_TICKS, 4, true, 22);
    /* Y轴：6个主刻度（0~100） */
    lv_chart_set_axis_tick(gui_main_chart_vibration, LV_CHART_AXIS_PRIMARY_Y,
                           5, 3, CHART_Y_TICKS, 4, true, 36);

    /* 横向缩放，支持滚动 */
    lv_chart_set_zoom_x(gui_main_chart_vibration, CHART_ZOOM_X);
    lv_obj_set_scroll_dir(gui_main_chart_vibration, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(gui_main_chart_vibration, LV_SCROLLBAR_MODE_AUTO);

    /* ---- 添加三条曲线系列 ---- */
    /* 峰峰值曲线：绿色（s_gen_colors[1] = #66BB6A） */
    gui_main_series_vib[0] = lv_chart_add_series(
        gui_main_chart_vibration,
        s_gen_colors[1],
        LV_CHART_AXIS_PRIMARY_Y);

    /* 上包络线：红色（s_vib_colors[0] = #EF5350，初始隐藏，有包络文件时显示） */
    gui_main_series_env_upper = lv_chart_add_series(
        gui_main_chart_vibration,
        s_vib_colors[0],
        LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_hide_series(gui_main_chart_vibration,
                         gui_main_series_env_upper, true);

    /* 下包络线：蓝色（s_gen_colors[0] = #42A5F5，初始隐藏） */
    gui_main_series_env_lower = lv_chart_add_series(
        gui_main_chart_vibration,
        s_gen_colors[0],
        LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_hide_series(gui_main_chart_vibration,
                         gui_main_series_env_lower, true);

    /* ---- 包络线（CSV）；P-P 演示数据仅 ACQ_USE_DEMO_DATA 时填充 ---- */
    main_vib_fill_envelope(gui_main_chart_vibration, &acq);
#if defined(ACQ_USE_DEMO_DATA)
    main_vib_fill_demo_data(gui_main_chart_vibration);
#else
    main_vib_init_pp_chart_zero(gui_main_chart_vibration);
#endif

    /* ---- X轴标签 ---- */
    lv_obj_t *lbl_x = lv_label_create(cont);
    lv_label_set_text(lbl_x, "Time(s)");
    lv_obj_set_style_text_font(lbl_x, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_x, lv_color_hex(0x333333), 0);
    lv_obj_set_width(lbl_x, LV_PCT(100));
    lv_obj_set_style_text_align(lbl_x, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(lbl_x, CHART_Y_LABEL_W + 14, 0);

    /* ---- 图例行 ---- */
    /* 图例文字名称：用纯字符串数组避免在函数内用 LV_COLOR_MAKE 初始化结构体 */
    static const char * const vib_legend_names[3] = {
        "Peak-Peak", "Upper Env", "Lower Env"
    };
    /* 图例颜色复用文件作用域颜色数组（避免 LV_COLOR_MAKE 展开问题）：
     *   [0] 绿色 s_gen_colors[1]  #66BB6A - 峰峰值
     *   [1] 红色 s_vib_colors[0]  #EF5350 - 上包络
     *   [2] 蓝色 s_gen_colors[0]  #42A5F5 - 下包络
     */

    lv_obj_t *legend = lv_obj_create(cont);
    lv_obj_set_width(legend, LV_PCT(100));
    lv_obj_set_height(legend, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(legend, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(legend, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(legend, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(legend, 0, 0);
    lv_obj_set_style_pad_all(legend, 0, 0);
    lv_obj_set_style_pad_top(legend, 2, 0);
    lv_obj_set_style_pad_gap(legend, 10, 0);
    lv_obj_clear_flag(legend, LV_OBJ_FLAG_SCROLLABLE);

    for (uint8_t i = 0u; i < 3u; i++) {
        s_vib_legend_idx[i]    = i;
        s_vib_series_hidden[i] = false;

        /* 根据索引选对应颜色 */
        lv_color_t leg_color;
        if      (i == 0u) leg_color = s_gen_colors[1]; /* 绿色 - 峰峰值 */
        else if (i == 1u) leg_color = s_vib_colors[0]; /* 红色 - 上包络 */
        else              leg_color = s_gen_colors[0];  /* 蓝色 - 下包络 */

        /* 若包络线未加载，上下包络图例半透明显示（已隐藏系列） */
        lv_opa_t item_opa = ((i > 0u) && !env_loaded)
                            ? LV_OPA_40 : LV_OPA_COVER;

        lv_obj_t *item = lv_obj_create(legend);
        lv_obj_set_size(item, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(item, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_opa(item, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(item, 0, 0);
        lv_obj_set_style_pad_all(item, 2, 0);
        lv_obj_set_style_pad_gap(item, 4, 0);
        lv_obj_set_style_opa(item, item_opa, 0);
        lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(item, main_vib_legend_click_cb,
                            LV_EVENT_CLICKED, &s_vib_legend_idx[i]);

        lv_obj_t *swatch = lv_obj_create(item);
        lv_obj_set_size(swatch, 10, 10);
        lv_obj_set_style_radius(swatch, 1, 0);
        lv_obj_set_style_bg_color(swatch, leg_color, 0);
        lv_obj_set_style_bg_opa(swatch, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(swatch, 0, 0);
        lv_obj_clear_flag(swatch, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(swatch, LV_OBJ_FLAG_EVENT_BUBBLE);

        lv_obj_t *name_lbl = lv_label_create(item);
        lv_label_set_text(name_lbl, vib_legend_names[i]);
        lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(name_lbl, lv_color_hex(0x333333), 0);
        lv_obj_add_flag(name_lbl, LV_OBJ_FLAG_EVENT_BUBBLE);
    }

    /* 同步图例"上下包络"可见状态（无文件时已隐藏系列） */
    if (!env_loaded) {
        s_vib_series_hidden[1] = true;
        s_vib_series_hidden[2] = true;
    }
}

static void set_blink_mode(uint8_t level)
{
    /* 停止已有闪烁 */
    if (s_blink_timer != NULL) {
        lv_timer_del(s_blink_timer);
        s_blink_timer = NULL;
    }

    s_alarm_level = level;

    /* 恢复正常显示 */
    if (gui_main_status_label != NULL)
        lv_obj_set_style_opa(gui_main_status_label, LV_OPA_COVER, 0);
    if (gui_main_status_led != NULL)
        lv_obj_set_style_opa(gui_main_status_led, LV_OPA_COVER, 0);

    if (level > 0) {
        /* 单报警 1000ms, 双报警 500ms */
        uint32_t period = (level >= 2) ? 500 : 1000;
        s_blink_timer = lv_timer_create(status_bar_blink_timer_cb, period, NULL);
        s_blink_visible = true;
    }
}

/* ==================== 底部操作栏回调 ==================== */

static void main_sp_update_labels(void)
{
    char buf[12];
    if (gui_main_label_sp1 != NULL) {
        lv_snprintf(buf, sizeof(buf), "%u", (unsigned)s_sp1_count);
        lv_label_set_text(gui_main_label_sp1, buf);
    }
    if (gui_main_label_sp2 != NULL) {
        lv_snprintf(buf, sizeof(buf), "%u", (unsigned)s_sp2_count);
        lv_label_set_text(gui_main_label_sp2, buf);
    }
}

/**
 * @brief  根据 Acq_IsRunning() 同步 Start/Stop 按钮外观
 */
static void main_acq_sync_button_ui(void)
{
    if (gui_main_btn_start_stop == NULL || gui_main_label_start_stop == NULL) {
        return;
    }

    if (Acq_IsRunning()) {
        lv_obj_set_style_bg_color(gui_main_btn_start_stop,
                                  lv_color_hex(0x757575), 0);
        lv_label_set_text(gui_main_label_start_stop, "Stop Acq");
    } else {
        lv_obj_set_style_bg_color(gui_main_btn_start_stop,
                                  lv_color_hex(0x1E88E5), 0);
        lv_label_set_text(gui_main_label_start_stop, "Start Acq");
    }
}

static void main_btn_start_stop_cb(lv_event_t *e)
{
    (void)e;

    if (Acq_IsRunning()) {
        Acq_Stop();
    } else {
        Acq_Start();
    }

    main_acq_sync_button_ui();
}

static void main_btn_clear_cb(lv_event_t *e)
{
    (void)e;
    s_sp1_count = 0;
    s_sp2_count = 0;
    main_sp_update_labels();
}

static void main_btn_alarm_reset_cb(lv_event_t *e)
{
    (void)e;
    Alarm_Reset();
    GUI_UpdateStatus((uint8_t)Alarm_GetStatus(),
                     g_general_alarm_active, g_vibration_alarm_active);
    GUI_Toast("Alarm reset", 2000);
}

/* ==================== Public create function ==================== */

/**
 * @brief  创建主监控画面（当前仅标题/状态栏）
 * @param  parent  Tabview 的 tab 页容器
 */
void gui_main_display_create(lv_obj_t *parent)
{
    /* ================================================================
     * 第1部分：标题/状态栏
     * ┌────────────────────────────────────────────────────────┐
     * │ Main Display  ● Normal        2026-01-01 00:00:00     │
     * └────────────────────────────────────────────────────────┘
     * 左侧：标题 + 系统状态（绿色LED + 文字，点击弹出报警列表）
     * 右侧：日期 + 时间（精确到秒，每秒刷新）
     * 淡灰色背景，所有元素上下居中
     * ================================================================ */

    /* --- 去掉 tab 内容区的默认 padding，消除与 tab 栏的间隙 --- */
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_pad_row(parent, 0, 0);
    lv_obj_set_style_pad_gap(parent, 0, 0);

    /* --- 状态栏容器：100%宽、固定高度、贴顶、淡灰背景 --- */
    s_status_bar = lv_obj_create(parent);
    lv_obj_set_width(s_status_bar, LV_PCT(100));
    lv_obj_set_height(s_status_bar, STATUS_BAR_H);
    lv_obj_set_pos(s_status_bar, 0, 0);
    lv_obj_set_style_bg_color(s_status_bar, lv_color_hex(0xE8E8E8), 0);
    lv_obj_set_style_bg_opa(s_status_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(s_status_bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(s_status_bar, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_border_width(s_status_bar, 1, 0);
    lv_obj_set_style_pad_all(s_status_bar, 0, 0);
    lv_obj_set_style_radius(s_status_bar, 0, 0);
    lv_obj_set_style_outline_width(s_status_bar, 0, 0);
    lv_obj_set_style_shadow_width(s_status_bar, 0, 0);

    /* ===== 左侧区域：标题 + 状态 ===== */

    /* --- 标题文字 --- */
    lv_obj_t *title_label = lv_label_create(s_status_bar);
    lv_label_set_text(title_label, "Main Display");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x222222), 0);
    lv_obj_set_style_pad_all(title_label, 0, 0);   /* 清除主题默认padding防止高度溢出 */
    lv_obj_set_style_border_width(title_label, 0, 0);
    lv_obj_align(title_label, LV_ALIGN_LEFT_MID, STATUS_BAR_PAD, 0);

    /* --- 状态LED（绿色圆点） --- */
    gui_main_status_led = lv_obj_create(s_status_bar);
    lv_obj_set_size(gui_main_status_led, STATUS_LED_SIZE, STATUS_LED_SIZE);
    lv_obj_set_style_radius(gui_main_status_led, STATUS_LED_SIZE / 2, 0);
    lv_obj_set_style_bg_color(gui_main_status_led,
                              lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_style_pad_all(gui_main_status_led, 0, 0);  /* 清除主题默认padding防止高度溢出 */
    lv_obj_set_style_border_width(gui_main_status_led, 0, 0);
    lv_obj_align_to(gui_main_status_led, title_label,
                    LV_ALIGN_OUT_RIGHT_MID, TITLE_STATUS_GAP, 0);

    /* --- 状态文字 --- */
    gui_main_status_label = lv_label_create(s_status_bar);
    lv_label_set_text(gui_main_status_label, "Normal");
    lv_obj_set_style_text_font(gui_main_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(gui_main_status_label,
                                lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_style_pad_all(gui_main_status_label, 0, 0);  /* 清除主题默认padding防止高度溢出 */
    lv_obj_set_style_border_width(gui_main_status_label, 0, 0);
    lv_obj_align_to(gui_main_status_label, gui_main_status_led,
                    LV_ALIGN_OUT_RIGHT_MID, STATUS_LED_TEXT_GAP, 0);

    /* --- 状态栏本身可点击（点击弹出报警简要列表） --- */
    lv_obj_add_flag(s_status_bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_status_bar, status_click_cb,
                        LV_EVENT_CLICKED, NULL);

    /* ===== 右侧区域：日期 + 时间 ===== */

    /* --- 时间标签（初始文本，由定时器刷新） --- */
    gui_main_label_time = lv_label_create(s_status_bar);
    main_status_bar_update_time();
    lv_obj_set_style_text_font(gui_main_label_time, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(gui_main_label_time, lv_color_hex(0x333333), 0);
    lv_obj_set_style_pad_all(gui_main_label_time, 0, 0);    /* 清除主题默认padding防止高度溢出 */
    lv_obj_set_style_border_width(gui_main_label_time, 0, 0);
    lv_obj_align(gui_main_label_time, LV_ALIGN_RIGHT_MID, -STATUS_BAR_PAD, 0);

    /* --- 创建时间刷新定时器（1秒周期） --- */
    s_time_timer = lv_timer_create(status_bar_time_timer_cb, 1000, NULL);

    /* --- 初始状态：正常，无闪烁 --- */
    s_alarm_level = 0;
    s_blink_visible = true;

    /* ================================================================
     * 第2部分：图表区域（可滚动容器）
     *
     * 当前仅实现通用信号图表，振动信号图表待后续添加
     *
     * 布局：
     *  ┌─ scroll_area (vertical scroll) ────────────────────┐
     *  │  ┌── gen_section (bordered container + margin) ──┐  │
     *  │  │ Repair Monitor - General Signal  (on border)  │  │
     *  │  │         General Signal  (chart title)         │  │
     *  │  │  Value ┌──────────────────────────────────┐   │  │
     *  │  │   ↑    │  (line chart, 4 series)           │   │  │
     *  │  │   │    └──────────────────────────────────┘   │  │
     *  │  │         Time(s)                               │  │
     *  │  │  ■ Ch1  ■ Ch2  ■ Ch3  ■ Ch4                  │  │
     *  │  └─────────────────────────────────────────────┘  │
     *  └──────────────────────────────────────────────────────┘
     * ================================================================ */

    /* --- 可滚动图表容器，固定高度，内容超出时纵向滚动 --- */
    lv_obj_t *scroll_area = lv_obj_create(parent);
    lv_obj_set_width(scroll_area, LV_PCT(100));
    lv_obj_set_height(scroll_area, MAIN_SCROLL_H);
    lv_obj_set_pos(scroll_area, 0, STATUS_BAR_H);
    lv_obj_set_style_bg_color(scroll_area, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(scroll_area, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scroll_area, 0, 0);
    lv_obj_set_style_pad_all(scroll_area, SCROLL_PAD, 0);
    lv_obj_set_style_pad_gap(scroll_area, SECTION_GAP, 0);
    lv_obj_set_style_radius(scroll_area, 0, 0);
    lv_obj_set_scroll_dir(scroll_area, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(scroll_area, LV_SCROLL_SNAP_NONE);
    lv_obj_set_scrollbar_mode(scroll_area, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_layout(scroll_area, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(scroll_area, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scroll_area, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_flag(scroll_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(scroll_area, LV_OBJ_FLAG_SCROLL_MOMENTUM);

    /* --- 通用信号图表区（标题压在边框左上角） --- */
    GUI_LegendBoxCfg_t gen_cfg;
    GUI_LegendBoxCfgInit(&gen_cfg);
    gen_cfg.box_h     = (lv_coord_t)s_chart_box_height;
    gen_cfg.cont_h    = (lv_coord_t)(s_chart_box_height - 24);
    gen_cfg.inner_pad = CHART_INNER_PAD;
    lv_obj_t *gen_chart_cont = GUI_CreateLegendBox(scroll_area,
        "Repair Monitor - General Signal", &gen_cfg);
    main_create_general_chart(gen_chart_cont);

    /* 通用信号和振动信号分割线 */
    lv_obj_t *gen_vib_divider = lv_obj_create(scroll_area);
    lv_obj_set_size(gen_vib_divider, LV_PCT(100), 2);
    lv_obj_set_style_bg_color(gen_vib_divider, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_bg_opa(gen_vib_divider, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(gen_vib_divider, 0, 0);
    lv_obj_clear_flag(gen_vib_divider, LV_OBJ_FLAG_SCROLLABLE);

    /* --- 振动信号图表区（标题压在边框左上角） ---
     *
     * 边框标题格式："Repair Monitor - {振动通道名称}"
     * 通道名称来自采集设置中选定的振动通道（AcqParams.vibration_ch_num）。
     * 若未配置振动通道，则使用默认标题。
     */
    {
        AcqParams_t vib_acq = Config_LoadAcqParams();
        char vib_box_title[64];
        ChannelConfig_t *vib_ch = ChannelData_GetById(vib_acq.vibration_ch_num);
        if (vib_ch != NULL && vib_ch->channel_name[0] != '\0') {
            lv_snprintf(vib_box_title, sizeof(vib_box_title),
                        "Repair Monitor - %s", vib_ch->channel_name);
        } else {
            lv_snprintf(vib_box_title, sizeof(vib_box_title),
                        "Repair Monitor - Vibration Signal");
        }

        GUI_LegendBoxCfg_t vib_cfg;
        GUI_LegendBoxCfgInit(&vib_cfg);
        vib_cfg.box_h     = (lv_coord_t)s_chart_box_height;
        vib_cfg.cont_h    = (lv_coord_t)(s_chart_box_height - 24);
        vib_cfg.inner_pad = CHART_INNER_PAD;
        lv_obj_t *vib_chart_cont = GUI_CreateLegendBox(scroll_area,
            vib_box_title, &vib_cfg);
        main_create_vibration_chart(vib_chart_cont);
    }

    /* ================================================================
     * 第4部分：底部操作栏（固定，不随滚动）
     *
     *  [Start Acq]  Trim Cycle: SP1:[0] SP2:[0] [Clear]     [Alarm Reset]
     * ================================================================ */

    lv_obj_t *footer_bar = lv_obj_create(parent);
    lv_obj_set_width(footer_bar, LV_PCT(100));
    lv_obj_set_height(footer_bar, FOOTER_BAR_H);
    lv_obj_set_pos(footer_bar, 0, STATUS_BAR_H + MAIN_SCROLL_H);
    lv_obj_set_style_bg_color(footer_bar, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(footer_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(footer_bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(footer_bar, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_border_width(footer_bar, 1, 0);
    lv_obj_set_style_radius(footer_bar, 0, 0);
    lv_obj_set_style_pad_hor(footer_bar, 10, 0);
    lv_obj_set_style_pad_ver(footer_bar, 0, 0);
    lv_obj_clear_flag(footer_bar, LV_OBJ_FLAG_SCROLLABLE);

    /* --- 开始/停止采集 --- */
    gui_main_btn_start_stop = lv_btn_create(footer_bar);
    lv_obj_set_size(gui_main_btn_start_stop, 96, 28);
    lv_obj_align(gui_main_btn_start_stop, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(gui_main_btn_start_stop, lv_color_hex(0x1E88E5), 0);
    lv_obj_set_style_bg_opa(gui_main_btn_start_stop, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(gui_main_btn_start_stop, 0, 0);
    lv_obj_set_style_radius(gui_main_btn_start_stop, 3, 0);
    gui_main_label_start_stop = lv_label_create(gui_main_btn_start_stop);
    lv_label_set_text(gui_main_label_start_stop, "Start Acq");
    lv_obj_set_style_text_color(gui_main_label_start_stop, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(gui_main_label_start_stop, &lv_font_montserrat_12, 0);
    lv_obj_center(gui_main_label_start_stop);
    lv_obj_add_event_cb(gui_main_btn_start_stop, main_btn_start_stop_cb,
                        LV_EVENT_CLICKED, NULL);

    /* --- 修整循环计数 SP1 / SP2 --- */
    lv_obj_t *lbl_cycle = lv_label_create(footer_bar);
    lv_label_set_text(lbl_cycle, "Trim Cycle:");
    lv_obj_set_style_text_font(lbl_cycle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_cycle, lv_color_hex(0x333333), 0);
    lv_obj_align_to(lbl_cycle, gui_main_btn_start_stop,
                    LV_ALIGN_OUT_RIGHT_MID, 16, 0);

    lv_obj_t *lbl_sp1 = lv_label_create(footer_bar);
    lv_label_set_text(lbl_sp1, "SP1:");
    lv_obj_set_style_text_font(lbl_sp1, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_sp1, lv_color_hex(0x333333), 0);
    lv_obj_align_to(lbl_sp1, lbl_cycle, LV_ALIGN_OUT_RIGHT_MID, 8, 0);

    lv_obj_t *sp1_box = lv_obj_create(footer_bar);
    lv_obj_set_size(sp1_box, 40, 24);
    lv_obj_set_style_bg_color(sp1_box, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(sp1_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sp1_box, 1, 0);
    lv_obj_set_style_border_color(sp1_box, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_radius(sp1_box, 2, 0);
    lv_obj_set_style_pad_all(sp1_box, 0, 0);
    lv_obj_clear_flag(sp1_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align_to(sp1_box, lbl_sp1, LV_ALIGN_OUT_RIGHT_MID, 4, 0);
    gui_main_label_sp1 = lv_label_create(sp1_box);
    lv_label_set_text(gui_main_label_sp1, "0");
    lv_obj_set_style_text_font(gui_main_label_sp1, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(gui_main_label_sp1, lv_color_hex(0x333333), 0);
    lv_obj_center(gui_main_label_sp1);

    lv_obj_t *lbl_sp2 = lv_label_create(footer_bar);
    lv_label_set_text(lbl_sp2, "SP2:");
    lv_obj_set_style_text_font(lbl_sp2, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_sp2, lv_color_hex(0x333333), 0);
    lv_obj_align_to(lbl_sp2, sp1_box, LV_ALIGN_OUT_RIGHT_MID, 8, 0);

    lv_obj_t *sp2_box = lv_obj_create(footer_bar);
    lv_obj_set_size(sp2_box, 40, 24);
    lv_obj_set_style_bg_color(sp2_box, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(sp2_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sp2_box, 1, 0);
    lv_obj_set_style_border_color(sp2_box, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_radius(sp2_box, 2, 0);
    lv_obj_set_style_pad_all(sp2_box, 0, 0);
    lv_obj_clear_flag(sp2_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align_to(sp2_box, lbl_sp2, LV_ALIGN_OUT_RIGHT_MID, 4, 0);
    gui_main_label_sp2 = lv_label_create(sp2_box);
    lv_label_set_text(gui_main_label_sp2, "0");
    lv_obj_set_style_text_font(gui_main_label_sp2, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(gui_main_label_sp2, lv_color_hex(0x333333), 0);
    lv_obj_center(gui_main_label_sp2);

    /* --- 清零 --- */
    lv_obj_t *btn_clear = lv_btn_create(footer_bar);
    lv_obj_set_size(btn_clear, 56, 28);
    lv_obj_set_style_bg_color(btn_clear, lv_color_hex(0x757575), 0);
    lv_obj_set_style_bg_opa(btn_clear, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_clear, 0, 0);
    lv_obj_set_style_radius(btn_clear, 3, 0);
    lv_obj_align_to(btn_clear, sp2_box, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    lv_obj_t *lbl_clear = lv_label_create(btn_clear);
    lv_label_set_text(lbl_clear, "Clear");
    lv_obj_set_style_text_color(lbl_clear, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_clear, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_clear);
    lv_obj_add_event_cb(btn_clear, main_btn_clear_cb, LV_EVENT_CLICKED, NULL);

    /* --- 报警复位（红色，靠右固定显示） --- */
    gui_main_btn_alarm_reset = lv_btn_create(footer_bar);
    lv_obj_set_size(gui_main_btn_alarm_reset, 100, 28);
    lv_obj_align(gui_main_btn_alarm_reset, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(gui_main_btn_alarm_reset, lv_color_hex(0xE53935), 0);
    lv_obj_set_style_bg_opa(gui_main_btn_alarm_reset, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(gui_main_btn_alarm_reset, 0, 0);
    lv_obj_set_style_radius(gui_main_btn_alarm_reset, 3, 0);
    lv_obj_t *lbl_alarm_rst = lv_label_create(gui_main_btn_alarm_reset);
    lv_label_set_text(lbl_alarm_rst, "Alarm Reset");
    lv_obj_set_style_text_color(lbl_alarm_rst, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_alarm_rst, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_alarm_rst);
    lv_obj_add_event_cb(gui_main_btn_alarm_reset, main_btn_alarm_reset_cb,
                        LV_EVENT_CLICKED, NULL);

    lv_obj_update_layout(scroll_area);
}

/* ==================== 报警等级设置（供 GUI_UpdateStatus 调用） ==================== */

/**
 * @brief  设置主监控画面的报警闪烁等级
 * @param  level  0=正常, 1=单报警, 2=双报警
 *
 * gui_core.c 中 GUI_UpdateStatus() 根据 general_alarm / vibration_alarm
 * 标志调用此函数控制闪烁效果。
 */
void gui_main_set_alarm_level(uint8_t level)
{
    set_blink_mode(level);
}

/**
 * @brief  开始/重启采集时清零实时 PP 与通用曲线（包络参考线不重绘）
 */
void gui_main_reset_realtime_charts(void)
{
    if (gui_main_chart_general != NULL) {
        main_gen_init_chart_zero(gui_main_chart_general);
        main_chart_scroll_to_start(gui_main_chart_general);
    }
    if (gui_main_chart_vibration != NULL) {
        main_vib_init_pp_chart_zero(gui_main_chart_vibration);
        main_chart_scroll_to_start(gui_main_chart_vibration);
    }
}

/**
 * @brief  按振动图 PP 点序号查询包络参考值（0~100 显示坐标）
 *
 * 供 acq_pipeline 在 ProcessFrame 中查询当前 PP 点对应的包络上下界。
 * 与 main_vib_fill_envelope 使用相同归一化系数 s_env_norm_scale。
 *
 * 序号映射：
 *   acq_pipeline 每输出一个 PP 点，s_vib_ui_seq 递增；
 *   chart_idx = seq_index % CHART_POINTS_MAX，实现 100 秒循环显示；
 *   在 chart_idx 处对 CSV 上下界数组做线性插值，再乘以归一化系数对齐图表 Y 轴。
 *
 * @param  seq_index  PP 曲线点序号（每 20ms +1）
 * @param  upper      输出上包络（0~100 图表坐标）；可 NULL
 * @param  lower      输出下包络（0~100 图表坐标）；可 NULL
 * @return true  包络 CSV 已加载且插值成功
 * @return false 无包络数据（未选文件或加载失败）
 */
void gui_main_display_destroy(lv_obj_t *parent)
{
    if (s_time_timer != NULL) {
        lv_timer_del(s_time_timer);
        s_time_timer = NULL;
    }
    if (s_blink_timer != NULL) {
        lv_timer_del(s_blink_timer);
        s_blink_timer = NULL;
    }

    if (parent != NULL) {
        lv_obj_clean(parent);
    }

    gui_main_chart_general   = NULL;
    gui_main_chart_vibration = NULL;
    memset(gui_main_series_general, 0, sizeof(gui_main_series_general));
    memset(gui_main_series_vib, 0, sizeof(gui_main_series_vib));
    gui_main_series_env_upper = NULL;
    gui_main_series_env_lower = NULL;
    gui_main_gen_series_count = 0U;

    gui_main_label_time       = NULL;
    gui_main_status_led       = NULL;
    gui_main_status_label     = NULL;
    gui_main_btn_start_stop   = NULL;
    gui_main_label_start_stop = NULL;
    gui_main_label_sp1        = NULL;
    gui_main_label_sp2        = NULL;
    gui_main_btn_alarm_reset  = NULL;

    s_status_bar    = NULL;
    s_alarm_level   = 0U;
    s_blink_visible = true;
    s_gen_ch_count  = 0U;
}

bool GUI_GetVibEnvelopeAtSeq(uint32_t seq_index, float *upper, float *lower)
{
    if (s_env_point_count < 2u) {
        return false;
    }

    uint16_t chart_idx = (uint16_t)(seq_index % (uint32_t)ACQ_VIB_CHART_POINTS);
    return SignalAlgo_EnvelopeAtChartIdx(s_env_upper_y, s_env_lower_y,
                                         s_env_point_count,
                                         chart_idx, ACQ_VIB_CHART_POINTS,
                                         s_env_norm_scale, upper, lower);
}
