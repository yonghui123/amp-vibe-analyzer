/**
 * @file    gui_core.c
 * @brief   SignalX LVGL GUI Core - Initialization, Main Loop, Screen Management
 *
 *  Manages:
 *   - LVGL initialization (display driver ILI9341 320x240, touch XPT2046)
 *   - Tabview with 4 screens (Channel Setup, Acq Settings, Main Display, Alarm Records)
 *   - Screen switching, toast notifications, alarm popups, spinner overlay, status updates
 */

/* ==================== Includes ==================== */
#include "config.h"
#if GUI_TOUCH_TEST_LVGL_ENABLE
#include "touch_calib.h"
#endif
#include "lvgl.h"
#include "misc/lv_async.h"
#include "gui_core.h"
#include "channel_data.h"
#include "acq_task.h"
#include "data_logger.h"
#include <string.h>
#if !defined(PC_SIMULATOR)
#include "touch_bsp_touch.h"
#include "disp_ra8875_lvgl.h"
#endif

/* ==================== Forward declarations from screen modules ==================== */
extern void gui_channel_setup_create(lv_obj_t *parent);
extern void gui_acq_settings_create(lv_obj_t *parent);
extern void gui_main_display_create(lv_obj_t *parent);
extern void gui_alarm_records_create(lv_obj_t *parent);
extern void gui_channel_setup_destroy(lv_obj_t *parent);
extern void gui_acq_settings_destroy(lv_obj_t *parent);
extern void gui_main_display_destroy(lv_obj_t *parent);
extern void gui_alarm_records_destroy(lv_obj_t *parent);

/* Extern objects from screen_main_display.c for real-time updates */
extern lv_obj_t *gui_main_chart_general;        /* General signal chart */
extern lv_obj_t *gui_main_chart_vibration;      /* Vibration chart */
extern lv_chart_series_t *gui_main_series_general[4];
extern lv_chart_series_t *gui_main_series_vib[4];
extern lv_chart_series_t *gui_main_series_env_upper;
extern lv_chart_series_t *gui_main_series_env_lower;
extern uint8_t gui_main_gen_series_count;       /* 主监控通用折线有效条数 */
extern lv_obj_t *gui_main_label_time;
extern lv_obj_t *gui_main_status_led;
extern lv_obj_t *gui_main_status_label;
extern lv_obj_t *gui_main_btn_start_stop;
extern lv_obj_t *gui_main_label_start_stop;
extern lv_obj_t *gui_main_label_sp1;
extern lv_obj_t *gui_main_label_sp2;
extern lv_obj_t *gui_main_btn_alarm_reset;

/* Extern from screen_alarm_records.c for list refresh */
extern void gui_alarm_records_refresh_list(void);
extern void gui_main_set_alarm_level(uint8_t level);
extern void gui_main_reset_realtime_charts(void);

/* ==================== Static globals ==================== */
static lv_obj_t *g_tab_btns = NULL;
static lv_obj_t *g_tab_pages[SCREEN_COUNT];
static bool g_tab_loaded[SCREEN_COUNT];
static ScreenID_t g_current_screen = SCREEN_CHANNEL_SETUP;

/**
 * @brief  异步 GUI 刷新用的实时数据快照（堆分配，回调内释放）
 */
typedef struct {
    float    general_vals[MAX_GENERAL_CHANNELS];
    uint8_t  general_count;
    float    vib_pp;
    float    env_upper;
    float    env_lower;
    uint16_t rpm;
    uint8_t  vib_valid;
    uint8_t  env_valid;
} GuiRealtimePayload_t;

static void gui_apply_realtime_payload(const GuiRealtimePayload_t *payload);
static void gui_async_realtime_cb(void *user_data);

typedef struct {
    uint8_t status;
    bool    general_alarm;
    bool    vibration_alarm;
} GuiAlarmStatusPayload_t;

typedef struct {
    uint8_t alarm_type;
    char    channel_name[32];
    float   current_val;
    float   threshold;
} GuiAlarmPopupPayload_t;

static void gui_async_alarm_status_cb(void *user_data);
static void gui_async_alarm_popup_cb(void *user_data);
static lv_obj_t *g_spinner_overlay = NULL;
static lv_obj_t *g_toast_label = NULL;
static lv_timer_t *g_toast_timer = NULL;
#if !defined(PC_SIMULATOR)
/** 启动后忽略 Tab 切换的时长（避免校准松手残留坐标误切页） */
#define GUI_TAB_INPUT_DELAY_MS  1000U
static bool g_tab_input_armed = false;
static uint32_t g_tab_arm_ms = 0;
#endif

/* Tab names (btnmatrix map, last entry must be "") */
static const char *g_tab_map[] = {
    "Channel Setup",
    "Acq Settings",
    "Main Display",
    "Alarm Records",
    ""
};

static void gui_style_tab_btns(lv_obj_t *tab_btns)
{
    lv_obj_set_style_bg_color(tab_btns, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(tab_btns, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tab_btns, 0, 0);
    lv_obj_set_style_pad_all(tab_btns, 0, 0);
    lv_obj_set_style_pad_gap(tab_btns, 0, 0);

    lv_obj_set_style_bg_color(tab_btns, lv_color_hex(0xE8E8E8), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(tab_btns, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(tab_btns, lv_color_hex(0x333333), LV_PART_ITEMS);
    lv_obj_set_style_border_width(tab_btns, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_side(tab_btns, LV_BORDER_SIDE_FULL, LV_PART_ITEMS);
    lv_obj_set_style_border_color(tab_btns, lv_color_hex(0x999999), LV_PART_ITEMS);
    lv_obj_set_style_pad_left(tab_btns, 8, LV_PART_ITEMS);
    lv_obj_set_style_pad_right(tab_btns, 8, LV_PART_ITEMS);
    lv_obj_set_style_radius(tab_btns, 0, LV_PART_ITEMS);
    lv_obj_set_style_outline_width(tab_btns, 0, LV_PART_ITEMS);

    lv_obj_set_style_bg_color(tab_btns, lv_color_hex(0xFFFFFF),
        LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(tab_btns, LV_OPA_COVER,
        LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(tab_btns, lv_color_hex(0x000000),
        LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(tab_btns, 1,
        LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_side(tab_btns, LV_BORDER_SIDE_FULL,
        LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(tab_btns, lv_color_hex(0x999999),
        LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_outline_width(tab_btns, 0,
        LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_outline_width(tab_btns, 0,
        LV_PART_ITEMS | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_opa(tab_btns, LV_OPA_TRANSP,
        LV_PART_ITEMS | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(tab_btns, 0,
        LV_PART_ITEMS | LV_STATE_FOCUSED);
    lv_obj_set_style_shadow_width(tab_btns, 0,
        LV_PART_ITEMS | LV_STATE_FOCUSED);

    lv_obj_set_style_bg_color(tab_btns, lv_color_hex(0xFFFFFF),
        LV_PART_ITEMS | LV_STATE_CHECKED | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(tab_btns, LV_OPA_COVER,
        LV_PART_ITEMS | LV_STATE_CHECKED | LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(tab_btns, 1,
        LV_PART_ITEMS | LV_STATE_CHECKED | LV_STATE_FOCUSED);
    lv_obj_set_style_border_side(tab_btns, LV_BORDER_SIDE_FULL,
        LV_PART_ITEMS | LV_STATE_CHECKED | LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(tab_btns, lv_color_hex(0x999999),
        LV_PART_ITEMS | LV_STATE_CHECKED | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(tab_btns, 0,
        LV_PART_ITEMS | LV_STATE_CHECKED | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_opa(tab_btns, LV_OPA_TRANSP,
        LV_PART_ITEMS | LV_STATE_CHECKED | LV_STATE_FOCUSED);
    lv_obj_set_style_shadow_width(tab_btns, 0,
        LV_PART_ITEMS | LV_STATE_CHECKED | LV_STATE_FOCUSED);

    lv_obj_set_style_bg_color(tab_btns, lv_color_hex(0xE8E8E8),
        LV_PART_ITEMS | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(tab_btns, LV_OPA_COVER,
        LV_PART_ITEMS | LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(tab_btns, 1,
        LV_PART_ITEMS | LV_STATE_FOCUSED);
    lv_obj_set_style_border_side(tab_btns, LV_BORDER_SIDE_FULL,
        LV_PART_ITEMS | LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(tab_btns, lv_color_hex(0x999999),
        LV_PART_ITEMS | LV_STATE_FOCUSED);
}

/** 按需创建 Tab 页内容，避免启动时 4 屏同时占用 LVGL 堆导致 STM32 OOM 卡死 */
static void gui_load_tab(ScreenID_t tab_id)
{
    if (tab_id >= SCREEN_COUNT || g_tab_loaded[tab_id]) {
        return;
    }
    if (g_tab_pages[tab_id] == NULL) {
        return;
    }

    switch (tab_id) {
    case SCREEN_CHANNEL_SETUP:
        gui_channel_setup_create(g_tab_pages[tab_id]);
        break;
    case SCREEN_ACQ_SETTINGS:
        gui_acq_settings_create(g_tab_pages[tab_id]);
        break;
    case SCREEN_MAIN_DISPLAY:
        gui_main_display_create(g_tab_pages[tab_id]);
        break;
    case SCREEN_ALARM_RECORDS:
        gui_alarm_records_create(g_tab_pages[tab_id]);
        break;
    default:
        break;
    }

    g_tab_loaded[tab_id] = true;
}

/** 销毁指定 Tab 内容并释放 LVGL 堆（切换页时仅保留当前 Tab） */
static void gui_unload_tab(ScreenID_t tab_id)
{
    lv_obj_t *page;

    if (tab_id >= SCREEN_COUNT || !g_tab_loaded[tab_id]) {
        return;
    }

    page = g_tab_pages[tab_id];
    if (page == NULL) {
        g_tab_loaded[tab_id] = false;
        return;
    }

    switch (tab_id) {
    case SCREEN_CHANNEL_SETUP:
        gui_channel_setup_destroy(page);
        break;
    case SCREEN_ACQ_SETTINGS:
        gui_acq_settings_destroy(page);
        break;
    case SCREEN_MAIN_DISPLAY:
        gui_main_display_destroy(page);
        break;
    case SCREEN_ALARM_RECORDS:
        gui_alarm_records_destroy(page);
        break;
    default:
        lv_obj_clean(page);
        break;
    }

    g_tab_loaded[tab_id] = false;
}

/** 卸载除 keep_id 外所有已加载 Tab，避免四页同驻导致 64KB 堆耗尽 */
static void gui_unload_other_tabs(ScreenID_t keep_id)
{
    for (uint32_t i = 0; i < SCREEN_COUNT; i++) {
        if (i != (uint32_t)keep_id) {
            gui_unload_tab((ScreenID_t)i);
        }
    }
}

static void gui_show_tab(uint32_t tab_id)
{
    if (tab_id >= SCREEN_COUNT) {
        return;
    }

#if !defined(PC_SIMULATOR)
    ScreenID_t prev = g_current_screen;
#endif

    gui_unload_other_tabs((ScreenID_t)tab_id);
    gui_load_tab((ScreenID_t)tab_id);

    if (g_tab_pages[tab_id] != NULL) {
        /* 懒加载/销毁后需刷新布局，否则 scroll_bottom=0 导致无法滑动 */
        lv_obj_update_layout(g_tab_pages[tab_id]);
    }

    for (uint32_t i = 0; i < SCREEN_COUNT; i++) {
        if (g_tab_pages[i] == NULL) {
            continue;
        }
        if (i == tab_id) {
            lv_obj_clear_flag(g_tab_pages[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(g_tab_pages[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (g_tab_btns != NULL) {
        lv_btnmatrix_clear_btn_ctrl_all(g_tab_btns, LV_BTNMATRIX_CTRL_CHECKED);
        lv_btnmatrix_set_btn_ctrl(g_tab_btns, tab_id, LV_BTNMATRIX_CTRL_CHECKED);
    }

    g_current_screen = (ScreenID_t)tab_id;

#if !defined(PC_SIMULATOR)
    /* 每次切页后清触摸/LVGL 残留坐标，避免 phantom PRESSED 误触 Tab */
    if (prev != g_current_screen) {
        DISP_ResetTouchState();
    }
#endif
}

#if !defined(PC_SIMULATOR)
static void gui_sync_tab_btns(void)
{
    if (g_tab_btns == NULL) {
        return;
    }
    lv_btnmatrix_clear_btn_ctrl_all(g_tab_btns, LV_BTNMATRIX_CTRL_CHECKED);
    lv_btnmatrix_set_btn_ctrl(g_tab_btns, (uint16_t)g_current_screen,
                              LV_BTNMATRIX_CTRL_CHECKED);
}

static void gui_tab_reject_switch(void)
{
    gui_sync_tab_btns();
    DISP_ResetTouchState();
}

/** 触摸点是否落在 Tab id 对应列（800px 四等分） */
static bool gui_tab_id_matches_point(uint32_t tab_id, lv_coord_t x)
{
    uint32_t col_w = LCD_WIDTH / SCREEN_COUNT;
    uint32_t col = (col_w > 0U) ? ((uint32_t)x / col_w) : 0U;

    if (col >= SCREEN_COUNT) {
        col = SCREEN_COUNT - 1U;
    }
    return col == tab_id;
}

static bool gui_tab_touch_valid(uint32_t tab_id)
{
    if (!TouchBsp_PenDown()) {
        return false;
    }

    lv_indev_t *indev = lv_indev_get_act();
    if (indev == NULL || lv_indev_get_type(indev) != LV_INDEV_TYPE_POINTER) {
        return false;
    }

    lv_point_t p;
    lv_indev_get_point(indev, &p);
    if (p.y < 0 || p.y >= TAB_BAR_H) {
        return false;
    }
    return gui_tab_id_matches_point(tab_id, p.x);
}
#endif

static void tab_btns_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }

#if !defined(PC_SIMULATOR)
    if (!g_tab_input_armed) {
        gui_tab_reject_switch();
        return;
    }
#endif

    lv_obj_t *btns = lv_event_get_target(e);
    uint32_t id = LV_BTNMATRIX_BTN_NONE;

    if (lv_event_get_param(e) != NULL) {
        id = *(const uint32_t *)lv_event_get_param(e);
    }
    if (id >= SCREEN_COUNT) {
        id = lv_btnmatrix_get_selected_btn(btns);
    }
    if (id >= SCREEN_COUNT) {
        return;
    }

    if (id == (uint32_t)g_current_screen) {
#if !defined(PC_SIMULATOR)
        gui_sync_tab_btns();
#endif
        return;
    }

#if !defined(PC_SIMULATOR)
    if (!gui_tab_touch_valid(id)) {
        gui_tab_reject_switch();
        return;
    }
#endif

    gui_show_tab(id);
}

/* ==================== Toast timer callback ==================== */

static void toast_timer_cb(lv_timer_t *timer)
{
    if (g_toast_label != NULL) {
        lv_obj_del(g_toast_label);
        g_toast_label = NULL;
    }
    if (g_toast_timer != NULL) {
        lv_timer_del(g_toast_timer);
        g_toast_timer = NULL;
    }
    (void)timer;
}

/* ==================== Alarm popup close callback ==================== */

static void alarm_popup_close_cb(lv_event_t *e)
{
    lv_obj_t *mbox = lv_event_get_current_target(e);
    lv_msgbox_close(mbox);
}

/* ==================== Public API ==================== */

void GUI_Init(void)
{
#if GUI_TOUCH_TEST_LVGL_ENABLE
    extern void gui_touch_test_create(lv_obj_t *parent);

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
    gui_touch_test_create(lv_scr_act());
    return;
#endif

    /* 初始化共享通道数据 (加载 JSON 或使用默认值) */
    ChannelData_Init();
    DataLogger_Init();
    (void)DataLogger_Cleanup(30U);

    /* 初始化采集任务与管线（校准路由、报警占位） */
    AcqTask_Init();

    /* Note: lv_init() and display/touch driver registration
     * are handled by the platform layer:
     *   - PC simulator: pc_simulator/main.c (SDL2)
     *   - STM32:        disp_ra8875_lvgl.c (RA8875 FSMC)
     *
     * GUI_Init() only creates the UI widgets here.
     */

    /* 1. Set light/white theme (matches design) */
    lv_theme_t *th = lv_theme_default_init(NULL,
        lv_palette_main(LV_PALETTE_BLUE),
        lv_palette_main(LV_PALETTE_BLUE),
        false,
        &lv_font_montserrat_14);
    lv_disp_set_theme(lv_disp_get_default(), th);

    /* Set screen background to white */
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);

    /* 2. Tab 栏 + 单页内容区（show/hide，避免 tabview 3200px 横向滚动多次重绘） */
#if !defined(PC_SIMULATOR)
    g_tab_input_armed = false;
    g_tab_arm_ms = lv_tick_get();
#endif

    g_tab_btns = lv_btnmatrix_create(lv_scr_act());
    lv_btnmatrix_set_map(g_tab_btns, g_tab_map);
    lv_btnmatrix_set_one_checked(g_tab_btns, true);
    lv_obj_set_size(g_tab_btns, LCD_WIDTH, TAB_BAR_H);
    lv_obj_align(g_tab_btns, LV_ALIGN_TOP_LEFT, 0, 0);
    gui_style_tab_btns(g_tab_btns);

    lv_btnmatrix_set_btn_width(g_tab_btns, 0, 4);
    lv_btnmatrix_set_btn_width(g_tab_btns, 1, 4);
    lv_btnmatrix_set_btn_width(g_tab_btns, 2, 4);
    lv_btnmatrix_set_btn_width(g_tab_btns, 3, 4);
    lv_btnmatrix_set_btn_ctrl_all(g_tab_btns,
        LV_BTNMATRIX_CTRL_CHECKABLE | LV_BTNMATRIX_CTRL_NO_REPEAT);
    lv_obj_add_event_cb(g_tab_btns, tab_btns_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *content = lv_obj_create(lv_scr_act());
    lv_obj_set_size(content, LCD_WIDTH, LCD_HEIGHT - TAB_BAR_H);
    lv_obj_set_pos(content, 0, TAB_BAR_H);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    /* 保持 CLICKABLE（默认），否则部分子树 hit-test/滚动链可能异常 */
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_bg_color(content, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);

    /* content 后创建会压在 Tab 上；置顶 Tab 并立即布局，避免首帧遮挡触摸 */
    lv_obj_move_foreground(g_tab_btns);
    lv_obj_update_layout(lv_scr_act());

    g_tab_pages[SCREEN_CHANNEL_SETUP] = lv_obj_create(content);
    g_tab_pages[SCREEN_ACQ_SETTINGS] = lv_obj_create(content);
    g_tab_pages[SCREEN_MAIN_DISPLAY] = lv_obj_create(content);
    g_tab_pages[SCREEN_ALARM_RECORDS] = lv_obj_create(content);

    for (uint32_t i = 0; i < SCREEN_COUNT; i++) {
        lv_obj_set_size(g_tab_pages[i], LV_PCT(100), LV_PCT(100));
        lv_obj_set_pos(g_tab_pages[i], 0, 0);
        lv_obj_clear_flag(g_tab_pages[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(g_tab_pages[i], 0, 0);
        lv_obj_set_style_border_width(g_tab_pages[i], 0, 0);
        lv_obj_set_style_bg_color(g_tab_pages[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(g_tab_pages[i], LV_OPA_COVER, 0);
        lv_obj_add_flag(g_tab_pages[i], LV_OBJ_FLAG_HIDDEN);
    }

    gui_show_tab(SCREEN_CHANNEL_SETUP);
    g_current_screen = SCREEN_CHANNEL_SETUP;
#if !defined(PC_SIMULATOR)
    DISP_ResetTouchState();
#endif
}


void GUI_TaskHandler(void)
{
#if !defined(PC_SIMULATOR)
    if (!g_tab_input_armed && lv_tick_elaps(g_tab_arm_ms) >= GUI_TAB_INPUT_DELAY_MS) {
        g_tab_input_armed = true;
        DISP_ResetTouchState();
        lv_indev_t *indev = lv_indev_get_next(NULL);
        while (indev != NULL) {
            if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
                lv_indev_reset(indev, g_tab_btns);
                /* 忽略残留 PRESSED，直到用户真正松手后再点 Tab */
                lv_indev_wait_release(indev);
                break;
            }
            indev = lv_indev_get_next(indev);
        }
    }
#endif
    lv_timer_handler();
}


void GUI_SwitchScreen(ScreenID_t screen_id)
{
    if (screen_id >= SCREEN_COUNT) return;

    gui_show_tab((uint32_t)screen_id);
}


ScreenID_t GUI_GetCurrentScreen(void)
{
    return g_current_screen;
}


/**
 * @brief  在 LVGL 上下文内将 payload 写入主监控 chart
 */
static void gui_apply_realtime_payload(const GuiRealtimePayload_t *payload)
{
    if (payload == NULL) {
        return;
    }

    if (g_current_screen != SCREEN_MAIN_DISPLAY) {
        return;
    }

    if (payload->general_count == 0U && payload->vib_valid == 0U) {
        return;
    }

    uint32_t count = payload->general_count;
    if (count == 0U) {
        count = 1U;
    }
    if (payload->general_count > 0U && gui_main_gen_series_count > 0U &&
        count > (uint32_t)gui_main_gen_series_count) {
        count = (uint32_t)gui_main_gen_series_count;
    }
    if (count > (uint32_t)MAX_GENERAL_CHANNELS) {
        count = (uint32_t)MAX_GENERAL_CHANNELS;
    }

    if (payload->general_count > 0U && gui_main_chart_general != NULL) {
        for (uint32_t i = 0; i < count; i++) {
            if (gui_main_series_general[i] != NULL) {
                lv_chart_set_next_value(gui_main_chart_general,
                                        gui_main_series_general[i],
                                        (lv_coord_t)payload->general_vals[i]);
            }
        }
    }

    if (payload->vib_valid != 0U && gui_main_chart_vibration != NULL) {
        if (gui_main_series_vib[0] != NULL) {
            lv_chart_set_next_value(gui_main_chart_vibration,
                                    gui_main_series_vib[0],
                                    (lv_coord_t)payload->vib_pp);
        }
        /* 包络线为静态参考曲线，创建时一次性绘制，采集过程不 SHIFT */
    }
}

/**
 * @brief  lv_async_call 回调：在 GUI 任务中执行 chart 刷新
 */
static void gui_async_realtime_cb(void *user_data)
{
    GuiRealtimePayload_t *payload = (GuiRealtimePayload_t *)user_data;
    if (payload == NULL) {
        return;
    }

    gui_apply_realtime_payload(payload);
    lv_mem_free(payload);
}

void GUI_PostRealtimeData(const float *general_vals, uint8_t general_count,
                          float vib_pp, uint8_t vib_valid,
                          float env_upper, float env_lower, uint8_t env_valid,
                          uint16_t rpm)
{
    GuiRealtimePayload_t local;

    memset(&local, 0, sizeof(local));
    local.general_count = general_count;
    local.vib_pp        = vib_pp;
    local.env_upper     = env_upper;
    local.env_lower     = env_lower;
    local.rpm           = rpm;
    local.vib_valid     = vib_valid;
    local.env_valid     = env_valid;

    if (general_count > 0U && general_vals != NULL) {
        uint8_t n = general_count;
        if (n > MAX_GENERAL_CHANNELS) {
            n = MAX_GENERAL_CHANNELS;
        }
        memcpy(local.general_vals, general_vals, (size_t)n * sizeof(float));
    }

#if defined(PC_SIMULATOR)
    /* PC：AcqTask_Poll 与 lv_timer_handler 同线程，直接刷新 */
    gui_apply_realtime_payload(&local);
#else
    GuiRealtimePayload_t *copy =
        (GuiRealtimePayload_t *)lv_mem_alloc(sizeof(GuiRealtimePayload_t));
    if (copy == NULL) {
        return;
    }
    memcpy(copy, &local, sizeof(GuiRealtimePayload_t));
    if (lv_async_call(gui_async_realtime_cb, copy) != LV_RES_OK) {
        lv_mem_free(copy);
    }
#endif
}

static void gui_post_alarm_status_impl(uint8_t status, bool general_alarm,
                                       bool vibration_alarm)
{
    GUI_UpdateStatus(status, general_alarm, vibration_alarm);
}

static void gui_async_alarm_status_cb(void *user_data)
{
    GuiAlarmStatusPayload_t *payload = (GuiAlarmStatusPayload_t *)user_data;
    if (payload == NULL) {
        return;
    }

    gui_post_alarm_status_impl(payload->status, payload->general_alarm,
                               payload->vibration_alarm);
    lv_mem_free(payload);
}

void GUI_PostAlarmStatus(uint8_t status, bool general_alarm,
                         bool vibration_alarm)
{
#if defined(PC_SIMULATOR)
    gui_post_alarm_status_impl(status, general_alarm, vibration_alarm);
#else
    GuiAlarmStatusPayload_t *copy =
        (GuiAlarmStatusPayload_t *)lv_mem_alloc(sizeof(GuiAlarmStatusPayload_t));
    if (copy == NULL) {
        return;
    }
    copy->status           = status;
    copy->general_alarm    = general_alarm;
    copy->vibration_alarm  = vibration_alarm;
    if (lv_async_call(gui_async_alarm_status_cb, copy) != LV_RES_OK) {
        lv_mem_free(copy);
    }
#endif
}

static void gui_async_alarm_popup_cb(void *user_data)
{
    GuiAlarmPopupPayload_t *payload = (GuiAlarmPopupPayload_t *)user_data;
    if (payload == NULL) {
        return;
    }

    GUI_ShowAlarmPopup(payload->alarm_type, payload->channel_name,
                       payload->current_val, payload->threshold);
    lv_mem_free(payload);
}

void GUI_PostAlarmPopup(uint8_t alarm_type, const char *channel_name,
                        float current_val, float threshold)
{
#if defined(PC_SIMULATOR)
    GUI_ShowAlarmPopup(alarm_type, channel_name, current_val, threshold);
#else
    GuiAlarmPopupPayload_t *copy =
        (GuiAlarmPopupPayload_t *)lv_mem_alloc(sizeof(GuiAlarmPopupPayload_t));
    if (copy == NULL) {
        return;
    }
    copy->alarm_type  = alarm_type;
    copy->current_val = current_val;
    copy->threshold   = threshold;
    if (channel_name != NULL) {
        strncpy(copy->channel_name, channel_name, sizeof(copy->channel_name) - 1U);
    } else {
        copy->channel_name[0] = '\0';
    }
    copy->channel_name[sizeof(copy->channel_name) - 1U] = '\0';
    if (lv_async_call(gui_async_alarm_popup_cb, copy) != LV_RES_OK) {
        lv_mem_free(copy);
    }
#endif
}

/**
 * @brief  更新主监控实时曲线数据（同步 API，供同线程或调试使用）
 */
void GUI_UpdateRealtimeData(const float *general_vals,
                            const float *vibration_pp,
                            const float *upper_env,
                            const float *lower_env,
                            uint32_t count, uint16_t rpm)
{
    GuiRealtimePayload_t local;

    memset(&local, 0, sizeof(local));
    local.rpm = rpm;

    if (count > (uint32_t)MAX_GENERAL_CHANNELS) {
        count = (uint32_t)MAX_GENERAL_CHANNELS;
    }

    if (general_vals != NULL && count > 0U) {
        local.general_count = (uint8_t)count;
        memcpy(local.general_vals, general_vals, count * sizeof(float));
    }

    if (vibration_pp != NULL) {
        local.vib_pp    = vibration_pp[0];
        local.vib_valid = 1U;
    }
    if (upper_env != NULL && lower_env != NULL) {
        local.env_upper = upper_env[0];
        local.env_lower = lower_env[0];
        local.env_valid = 1U;
    }

    gui_apply_realtime_payload(&local);
}


void GUI_UpdateStatus(uint8_t status, bool general_alarm,
                      bool vibration_alarm)
{
    lv_color_t color;
    const char *status_text;

    switch (status) {
        case 0: /* STATUS_NORMAL */
            color = lv_palette_main(LV_PALETTE_GREEN);
            status_text = "Normal";
            break;
        case 1: /* STATUS_TRIGGERING */
            color = lv_palette_main(LV_PALETTE_YELLOW);
            status_text = "Triggering";
            break;
        case 2: /* STATUS_ALARMING */
            color = lv_palette_main(LV_PALETTE_RED);
            status_text = "ALARM";
            break;
        default:
            color = lv_palette_main(LV_PALETTE_GREY);
            status_text = "Unknown";
            break;
    }

    /* Update main display status */
    if (gui_main_status_led != NULL) {
        lv_obj_set_style_bg_color(gui_main_status_led, color, 0);
    }
    if (gui_main_status_label != NULL) {
        lv_label_set_text(gui_main_status_label, status_text);
    }

    /* Update alarm reset button visibility */
    if (gui_main_btn_alarm_reset != NULL) {
        if (general_alarm || vibration_alarm) {
            lv_obj_clear_flag(gui_main_btn_alarm_reset, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(gui_main_btn_alarm_reset, LV_OBJ_FLAG_HIDDEN);
        }
    }

    uint8_t blink_level = 0U;
    if (general_alarm && vibration_alarm) {
        blink_level = 2U;
    } else if (general_alarm || vibration_alarm) {
        blink_level = 1U;
    }
    gui_main_set_alarm_level(blink_level);
}

void GUI_ResetMainCharts(void)
{
    gui_main_reset_realtime_charts();
}

/**
 * @brief  格式化浮点数（不依赖 LV_SPRINTF_USE_FLOAT）
 */
static void gui_format_float(char *buf, size_t buf_size, float value, uint8_t decimals)
{
    if (buf == NULL || buf_size == 0U) {
        return;
    }
    buf[0] = '\0';

    if (decimals > 6U) {
        decimals = 6U;
    }

    bool neg = (value < 0.0f);
    if (neg) {
        value = -value;
    }

    uint32_t scale = 1U;
    for (uint8_t i = 0U; i < decimals; i++) {
        scale *= 10U;
    }

    uint32_t scaled = (uint32_t)(value * (float)scale + 0.5f);
    uint32_t ipart  = scaled / scale;
    uint32_t fpart  = scaled % scale;

    if (decimals == 0U) {
        lv_snprintf(buf, buf_size, "%s%lu", neg ? "-" : "",
                    (unsigned long)ipart);
        return;
    }

    char frac[8];
    uint32_t tmp = fpart;
    frac[decimals] = '\0';
    for (uint8_t d = decimals; d > 0U; d--) {
        frac[d - 1U] = (char)('0' + (tmp % 10U));
        tmp /= 10U;
    }

    lv_snprintf(buf, buf_size, "%s%lu.%s", neg ? "-" : "",
                (unsigned long)ipart, frac);
}

static void gui_format_percent(char *buf, size_t buf_size, float pct)
{
    if (buf == NULL || buf_size == 0U) {
        return;
    }
    if (pct < 0.0f) {
        pct = 0.0f;
    }
    lv_snprintf(buf, buf_size, "%lu%%", (unsigned long)(uint32_t)(pct + 0.5f));
}


void GUI_ShowAlarmPopup(uint8_t alarm_type, const char *channel_name,
                        float current_val, float threshold)
{
    const char *type_str;
    const char *val_label;
    const char *thresh_label;

    switch (alarm_type) {
        case 1:
            type_str      = "General Signal";
            val_label     = "Measured";
            thresh_label  = "Limit";
            break;
        case 2:
            type_str      = "Vibration Envelope";
            val_label     = "PP (display)";
            thresh_label  = "Over-limit ratio";
            break;
        case 3:
            type_str      = "Dual (General + Vibration)";
            val_label     = "General measured";
            thresh_label  = "General limit";
            break;
        default:
            type_str      = "Unknown";
            val_label     = "Value";
            thresh_label  = "Threshold";
            break;
    }

    if (channel_name == NULL || channel_name[0] == '\0') {
        channel_name = "-";
    }

    char val_str[24];
    char thresh_str[24];
    gui_format_float(val_str, sizeof(val_str), current_val, 3);
    if (alarm_type == 2U) {
        gui_format_percent(thresh_str, sizeof(thresh_str), threshold);
    } else {
        gui_format_float(thresh_str, sizeof(thresh_str), threshold, 3);
    }

    static char msg_buf[320];
    lv_snprintf(msg_buf, sizeof(msg_buf),
                "%s\n"
                "Channel: %s\n"
                "%s: %s\n"
                "%s: %s",
                type_str, channel_name,
                val_label, val_str,
                thresh_label, thresh_str);

    if (alarm_type == 1U || alarm_type == 3U) {
        float delta = current_val - threshold;
        if (delta > 0.0f) {
            char over_str[24];
            gui_format_float(over_str, sizeof(over_str), delta, 3);
            lv_snprintf(msg_buf, sizeof(msg_buf),
                        "%s\n"
                        "Exceeded by: %s",
                        msg_buf, over_str);
        }
    } else if (alarm_type == 2U) {
        lv_snprintf(msg_buf, sizeof(msg_buf),
                    "%s\n"
                    "(PP exceeded envelope over-limit ratio)",
                    msg_buf);
    }

    /* Create modal message box */
    static const char *btns[] = {"Confirm", ""};
    lv_obj_t *mbox = lv_msgbox_create(NULL, "ALARM!", msg_buf, btns, false);
    lv_obj_set_width(mbox, 280);
    lv_obj_center(mbox);

    /* Set alarm colors */
    lv_obj_t *title = lv_msgbox_get_title(mbox);
    lv_obj_set_style_text_color(title, lv_palette_main(LV_PALETTE_RED), 0);

    /* Register close callback */
    lv_obj_add_event_cb(mbox, alarm_popup_close_cb, LV_EVENT_VALUE_CHANGED, NULL);
}


void GUI_RefreshChannelList(void)
{
    /* Re-create channel setup content by calling create again */
    /* Since the tabs already exist, just refresh the channel table data */

    /* We access the channel setup screen's internals via extern reference */
    /* For now, call the alarm list refresh since table structure is similar */
    gui_alarm_records_refresh_list();
}


void GUI_ShowLoading(bool show)
{
    if (show) {
        if (g_spinner_overlay != NULL) return; /* Already showing */

        /* Create full-screen semi-transparent overlay */
        g_spinner_overlay = lv_obj_create(lv_layer_top());
        lv_obj_set_size(g_spinner_overlay, LCD_WIDTH, LCD_HEIGHT);
        lv_obj_set_style_bg_color(g_spinner_overlay,
                                  lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(g_spinner_overlay, LV_OPA_50, 0);
        lv_obj_set_style_border_width(g_spinner_overlay, 0, 0);
        lv_obj_center(g_spinner_overlay);

        /* Spinner */
        lv_obj_t *spinner = lv_spinner_create(g_spinner_overlay, 1000, 60);
        lv_obj_set_size(spinner, 64, 64);
        lv_obj_center(spinner);
    } else {
        if (g_spinner_overlay == NULL) return;
        lv_obj_del(g_spinner_overlay);
        g_spinner_overlay = NULL;
    }
}


void GUI_Toast(const char *text, uint32_t duration_ms)
{
    /* Remove existing toast if any */
    if (g_toast_label != NULL) {
        lv_obj_del(g_toast_label);
        g_toast_label = NULL;
    }
    if (g_toast_timer != NULL) {
        lv_timer_del(g_toast_timer);
        g_toast_timer = NULL;
    }

    /* Create toast label on top layer */
    g_toast_label = lv_label_create(lv_layer_top());
    lv_label_set_text(g_toast_label, text);
    lv_obj_set_style_bg_color(g_toast_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(g_toast_label, LV_OPA_80, 0);
    lv_obj_set_style_text_color(g_toast_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_all(g_toast_label, 8, 0);
    lv_obj_set_style_radius(g_toast_label, 6, 0);

    /* Position at bottom center */
    lv_obj_align(g_toast_label, LV_ALIGN_BOTTOM_MID, 0, -20);

    /* Create auto-hide timer */
    g_toast_timer = lv_timer_create(toast_timer_cb, duration_ms, NULL);
    lv_timer_set_repeat_count(g_toast_timer, 1);
}
