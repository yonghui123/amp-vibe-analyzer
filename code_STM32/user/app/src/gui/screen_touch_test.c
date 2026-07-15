/**
 * @file    screen_touch_test.c
 * @brief   触摸点位对齐测试界面（LVGL 版，GUI_TOUCH_TEST_LVGL_ENABLE=1 时启用）
 */

#include "config.h"

#if GUI_TOUCH_TEST_LVGL_ENABLE

#include "lvgl.h"
#if !defined(PC_SIMULATOR)
#include "disp_ra8875_lvgl.h"
#include "touch_calib.h"
#endif
#include <stdio.h>
#include <stdbool.h>

#define TOUCH_TEST_MARKER_MAX   50
#define TOUCH_TEST_CROSS_HALF   14
#define TOUCH_TEST_INFO_H       88

static lv_obj_t *g_touch_area = NULL;
static lv_obj_t *g_label_coord = NULL;
static lv_obj_t *g_markers[TOUCH_TEST_MARKER_MAX];
static uint16_t g_marker_count = 0;
static uint16_t g_touch_seq = 0;

static lv_obj_t *g_label_status = NULL;
static lv_obj_t *g_label_diag = NULL;
static lv_timer_t *g_live_timer = NULL;
#if !defined(PC_SIMULATOR)
static lv_indev_state_t g_last_tp_state = LV_INDEV_STATE_RELEASED;
static int32_t g_last_scr_x = 0;
static int32_t g_last_scr_y = 0;
static uint16_t g_last_raw_x = 0;
static uint16_t g_last_raw_y = 0;

static void touch_test_add_marker_scr(int32_t scr_x, int32_t scr_y,
                                      uint16_t raw_x, uint16_t raw_y);
#endif

static void touch_test_refresh_coord_label(bool with_seq, lv_coord_t area_x, lv_coord_t area_y)
{
    if (g_label_coord == NULL) {
        return;
    }

    static char buf[96];
#if !defined(PC_SIMULATOR)
    uint16_t raw_x = 0;
    uint16_t raw_y = 0;
    int32_t scr_x = 0;
    int32_t scr_y = 0;
    lv_indev_state_t tp_state = LV_INDEV_STATE_RELEASED;

    DISP_GetLastTouchRaw(&raw_x, &raw_y);
    (void)DISP_GetTouchPoint(&scr_x, &scr_y, &tp_state);

    if (with_seq) {
        lv_snprintf(buf, sizeof(buf),
                    "(%d,%d) scr(%d,%d) raw(%u,%u) #%u",
                    (int)area_x, (int)area_y, (int)scr_x, (int)scr_y,
                    (unsigned)raw_x, (unsigned)raw_y, g_touch_seq);
    } else if (tp_state == LV_INDEV_STATE_PRESSED) {
        lv_snprintf(buf, sizeof(buf),
                    "LIVE scr(%d,%d) raw(%u,%u)",
                    (int)scr_x, (int)scr_y, (unsigned)raw_x, (unsigned)raw_y);
    } else {
        lv_snprintf(buf, sizeof(buf), "Last: --  (touch to show LIVE scr/raw)");
    }
#else
    if (with_seq) {
        lv_snprintf(buf, sizeof(buf), "(%d, %d)  #%u", (int)area_x, (int)area_y, g_touch_seq);
    } else {
        lv_snprintf(buf, sizeof(buf), "Last: --");
    }
#endif
    lv_label_set_text(g_label_coord, buf);
}

static void touch_test_live_timer_cb(lv_timer_t *timer)
{
    (void)timer;
#if !defined(PC_SIMULATOR)
    int32_t scr_x = 0;
    int32_t scr_y = 0;
    uint16_t raw_x = 0;
    uint16_t raw_y = 0;
    lv_indev_state_t tp_state = LV_INDEV_STATE_RELEASED;

    DISP_GetLastTouchRaw(&raw_x, &raw_y);
    (void)DISP_GetTouchPoint(&scr_x, &scr_y, &tp_state);

    if (tp_state == LV_INDEV_STATE_PRESSED) {
        g_last_scr_x = scr_x;
        g_last_scr_y = scr_y;
        g_last_raw_x = raw_x;
        g_last_raw_y = raw_y;
    } else if (g_last_tp_state == LV_INDEV_STATE_PRESSED
               && tp_state == LV_INDEV_STATE_RELEASED) {
        touch_test_add_marker_scr(g_last_scr_x, g_last_scr_y,
                                  g_last_raw_x, g_last_raw_y);
    }

    g_last_tp_state = tp_state;
#endif
    touch_test_refresh_coord_label(false, 0, 0);
}

static void touch_test_update_calib_status(void)
{
    char diag[72];

#if !defined(PC_SIMULATOR)
    TouchCalib_FormatLoadDiag(diag, sizeof(diag));
#else
    diag[0] = '\0';
#endif

    if (g_label_diag != NULL) {
        lv_label_set_text(g_label_diag, diag);
    }

    if (g_label_status == NULL) {
        return;
    }

#if !defined(PC_SIMULATOR)
    switch (TouchCalib_GetSource()) {
    case TOUCH_CALIB_SRC_SD:
        lv_label_set_text(g_label_status, "Calib: SD");
        break;
    case TOUCH_CALIB_SRC_FLASH:
        lv_label_set_text(g_label_status, "Calib: Flash (legacy)");
        break;
    case TOUCH_CALIB_SRC_V5_PARAM:
        lv_label_set_text(g_label_status, "Calib: v5 factory");
        break;
    case TOUCH_CALIB_SRC_RAM:
        lv_label_set_text(g_label_status, "Calib: RAM OK");
        break;
    default:
        lv_label_set_text(g_label_status, "Calib: NONE");
        break;
    }
#else
    lv_label_set_text(g_label_status, "Calib: PC sim");
#endif
}

static void touch_test_clear_markers(void)
{
    for (uint16_t i = 0; i < g_marker_count; i++) {
        if (g_markers[i] != NULL) {
            lv_obj_del(g_markers[i]);
            g_markers[i] = NULL;
        }
    }
    g_marker_count = 0;
    g_touch_seq = 0;
    touch_test_refresh_coord_label(false, 0, 0);
}

static void touch_test_clear_btn_cb(lv_event_t *e)
{
    (void)e;
    touch_test_clear_markers();
}

static lv_obj_t *touch_test_create_marker(lv_obj_t *screen, lv_coord_t scr_x, lv_coord_t scr_y)
{
    lv_obj_t *m = lv_obj_create(screen);
    lv_coord_t size = TOUCH_TEST_CROSS_HALF * 2 + 1;

    lv_obj_add_flag(m, LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(m, size, size);
    lv_obj_set_pos(m, scr_x - TOUCH_TEST_CROSS_HALF, scr_y - TOUCH_TEST_CROSS_HALF);
    lv_obj_clear_flag(m, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(m, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_align(m, LV_ALIGN_TOP_LEFT, 0);
    lv_obj_set_style_bg_opa(m, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m, 0, 0);
    lv_obj_set_style_pad_all(m, 0, 0);
    lv_obj_move_foreground(m);

    lv_obj_t *hline = lv_obj_create(m);
    lv_obj_set_size(hline, size, 2);
    lv_obj_set_pos(hline, 0, TOUCH_TEST_CROSS_HALF - 1);
    lv_obj_clear_flag(hline, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(hline, lv_color_hex(0xE53935), 0);
    lv_obj_set_style_bg_opa(hline, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hline, 0, 0);
    lv_obj_set_style_radius(hline, 0, 0);

    lv_obj_t *vline = lv_obj_create(m);
    lv_obj_set_size(vline, 2, size);
    lv_obj_set_pos(vline, TOUCH_TEST_CROSS_HALF - 1, 0);
    lv_obj_clear_flag(vline, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(vline, lv_color_hex(0xE53935), 0);
    lv_obj_set_style_bg_opa(vline, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(vline, 0, 0);
    lv_obj_set_style_radius(vline, 0, 0);

    lv_obj_t *dot = lv_obj_create(m);
    lv_obj_set_size(dot, 6, 6);
    lv_obj_set_pos(dot, TOUCH_TEST_CROSS_HALF - 3, TOUCH_TEST_CROSS_HALF - 3);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(0xE53935), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 0, 0);

    return m;
}

#if !defined(PC_SIMULATOR)
static void touch_test_add_marker_scr(int32_t scr_x, int32_t scr_y,
                                      uint16_t raw_x, uint16_t raw_y)
{
    lv_obj_t *screen = lv_scr_act();
    lv_coord_t x = (lv_coord_t)scr_x;
    lv_coord_t y = (lv_coord_t)scr_y;

    if (x < 0 || y < 0 || x >= LCD_WIDTH || y >= LCD_HEIGHT) {
        return;
    }

    if (g_marker_count >= TOUCH_TEST_MARKER_MAX) {
        lv_obj_del(g_markers[0]);
        for (uint16_t i = 1; i < g_marker_count; i++) {
            g_markers[i - 1] = g_markers[i];
        }
        g_marker_count--;
    }

    g_markers[g_marker_count] = touch_test_create_marker(screen, x, y);
    g_marker_count++;
    g_touch_seq++;

    if (g_label_coord != NULL) {
        static char buf[112];
        lv_snprintf(buf, sizeof(buf),
                    "scr(%d,%d) raw(%u,%u) #%u",
                    (int)scr_x, (int)scr_y,
                    (unsigned)raw_x, (unsigned)raw_y, g_touch_seq);
        lv_label_set_text(g_label_coord, buf);
    }
}
#endif

static void touch_test_add_reference_dot(lv_obj_t *parent, lv_align_t align,
                                         lv_coord_t x_ofs, lv_coord_t y_ofs,
                                         lv_color_t color)
{
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_set_size(dot, 10, 10);
    lv_obj_align(dot, align, x_ofs, y_ofs);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, color, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
}

static void touch_test_area_event_cb(lv_event_t *e)
{
    (void)e;
    /* 红叉改由 live timer 在驱动层 PRESSED→RELEASED 时按 scr 绘制 */
}

void gui_touch_test_create(lv_obj_t *parent)
{
    g_touch_area = NULL;
    g_label_coord = NULL;
    g_marker_count = 0;
    g_touch_seq = 0;

    lv_obj_set_style_bg_color(parent, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_border_width(parent, 0, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *info_bar = lv_obj_create(parent);
    lv_obj_set_width(info_bar, LV_PCT(100));
    lv_obj_set_height(info_bar, TOUCH_TEST_INFO_H);
    lv_obj_clear_flag(info_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(info_bar, lv_color_hex(0xF5F5F5), 0);
    lv_obj_set_style_bg_opa(info_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(info_bar, 0, 0);
    lv_obj_set_style_border_side(info_bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(info_bar, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_border_width(info_bar, 1, 0);
    lv_obj_set_style_pad_hor(info_bar, 8, 0);
    lv_obj_set_style_pad_ver(info_bar, 4, 0);
    lv_obj_set_flex_flow(info_bar, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info_bar, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *row1 = lv_obj_create(info_bar);
    lv_obj_set_width(row1, LV_PCT(100));
    lv_obj_set_height(row1, LV_SIZE_CONTENT);
    lv_obj_clear_flag(row1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(row1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row1, 0, 0);
    lv_obj_set_style_pad_all(row1, 0, 0);
    lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row1, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title = lv_label_create(row1);
    lv_label_set_text(title, "LVGL Touch Test v4");
    lv_obj_set_style_text_color(title, lv_color_hex(0x333333), 0);

    g_label_status = lv_label_create(row1);
    lv_obj_set_style_text_color(g_label_status, lv_color_hex(0x2E7D32), 0);

    lv_obj_t *btn_clear = lv_btn_create(row1);
    lv_obj_set_size(btn_clear, 72, 28);
    lv_obj_set_style_bg_color(btn_clear, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_bg_color(btn_clear, lv_color_hex(0xBDBDBD), LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_clear, touch_test_clear_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_clear = lv_label_create(btn_clear);
    lv_label_set_text(lbl_clear, "Clear");
    lv_obj_center(lbl_clear);

    g_label_diag = lv_label_create(info_bar);
    lv_obj_set_width(g_label_diag, LV_PCT(100));
    lv_obj_set_style_text_color(g_label_diag, lv_color_hex(0x1565C0), 0);
    touch_test_update_calib_status();

    g_label_coord = lv_label_create(info_bar);
    touch_test_refresh_coord_label(false, 0, 0);
    lv_obj_set_width(g_label_coord, LV_PCT(100));
    lv_obj_set_style_text_color(g_label_coord, lv_color_hex(0x666666), 0);

    g_live_timer = lv_timer_create(touch_test_live_timer_cb, 100, NULL);

    g_touch_area = lv_obj_create(parent);
    lv_obj_set_width(g_touch_area, LV_PCT(100));
    lv_obj_set_flex_grow(g_touch_area, 1);
    lv_obj_set_height(g_touch_area, 1);
    lv_obj_clear_flag(g_touch_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_touch_area, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(g_touch_area, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(g_touch_area, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_touch_area, 0, 0);
    lv_obj_set_style_pad_all(g_touch_area, 0, 0);
    lv_obj_add_event_cb(g_touch_area, touch_test_area_event_cb, LV_EVENT_RELEASED, NULL);

    {
        lv_color_t ref = lv_color_hex(0x1E88E5);

        touch_test_add_reference_dot(g_touch_area, LV_ALIGN_TOP_LEFT, 0, 0, ref);
        touch_test_add_reference_dot(g_touch_area, LV_ALIGN_TOP_RIGHT, 0, 0, ref);
        touch_test_add_reference_dot(g_touch_area, LV_ALIGN_BOTTOM_LEFT, 0, 0, ref);
        touch_test_add_reference_dot(g_touch_area, LV_ALIGN_BOTTOM_RIGHT, 0, 0, ref);
        touch_test_add_reference_dot(g_touch_area, LV_ALIGN_CENTER, 0, 0, ref);

        lv_obj_t *cx = lv_obj_create(g_touch_area);
        lv_obj_set_size(cx, 40, 1);
        lv_obj_align(cx, LV_ALIGN_CENTER, 0, 0);
        lv_obj_clear_flag(cx, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(cx, lv_color_hex(0xE0E0E0), 0);
        lv_obj_set_style_bg_opa(cx, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(cx, 0, 0);

        lv_obj_t *cy = lv_obj_create(g_touch_area);
        lv_obj_set_size(cy, 1, 40);
        lv_obj_align(cy, LV_ALIGN_CENTER, 0, 0);
        lv_obj_clear_flag(cy, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(cy, lv_color_hex(0xE0E0E0), 0);
        lv_obj_set_style_bg_opa(cy, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(cy, 0, 0);
    }
}

#else /* !GUI_TOUCH_TEST_LVGL_ENABLE */

#include "lvgl.h"

void gui_touch_test_create(lv_obj_t *parent)
{
    (void)parent;
}

#endif /* GUI_TOUCH_TEST_LVGL_ENABLE */
