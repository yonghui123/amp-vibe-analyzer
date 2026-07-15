/**
 * @file    gui_calib.c
 * @brief   RA8875 五点仿射校准向导 (20px margin, 800x480)
 */

#include "lvgl.h"
#include "gui_calib.h"
#include "touch_calib.h"
#include "config.h"
#if !defined(PC_SIMULATOR)
#include "disp_ra8875_lvgl.h"
#endif
#include <limits.h>
#include <stdio.h>

#define CALIB_CROSS_HALF        18
#define CALIB_ADC_MIN_DIST2     400U

typedef enum {
    CALIB_UI_WIZARD = 0,
    CALIB_UI_RESULT,
    CALIB_UI_DONE
} calib_ui_phase_t;

static lv_obj_t *g_root = NULL;
static lv_obj_t *g_panel = NULL;
static lv_obj_t *g_cross = NULL;
static lv_obj_t *g_lbl_title = NULL;
static lv_obj_t *g_lbl_target = NULL;
static lv_obj_t *g_lbl_live_raw = NULL;
static lv_obj_t *g_lbl_log = NULL;
static lv_obj_t *g_lbl_status = NULL;
static lv_timer_t *g_poll_timer = NULL;
static lv_timer_t *g_result_timer = NULL;

static calib_ui_phase_t g_phase = CALIB_UI_WIZARD;
static uint8_t g_step = 0;
static touch_calib_adc_t g_adc[TOUCH_CALIB_POINT_COUNT];
static touch_calib_lcd_t g_target[TOUCH_CALIB_POINT_COUNT];

static const char *g_step_titles[TOUCH_CALIB_POINT_COUNT] = {
    "Step 1/4: top-left",
    "Step 2/4: bottom-right",
    "Step 3/4: bottom-left",
    "Step 4/4: top-right",
};

static const char *g_step_short[TOUCH_CALIB_POINT_COUNT] = {
    "P1 TL", "P2 BR", "P3 BL", "P4 TR"
};

#if !defined(PC_SIMULATOR)
static void calib_poll_timer_cb(lv_timer_t *timer);
static void calib_result_timer_cb(lv_timer_t *timer);
#endif
#if defined(PC_SIMULATOR)
static void calib_pc_release_cb(lv_event_t *e);
#endif

static void calib_update_step_ui(void);
static void calib_refresh_log(void);
static void calib_update_live_raw(void);
static void calib_advance_with_sample(uint16_t adc_x, uint16_t adc_y);
static void calib_show_result_ui(void);
static void calib_show_done_ui(bool saved);
static void calib_restart_wizard(void);
static bool calib_adc_is_distinct(uint8_t step, uint16_t adc_x, uint16_t adc_y);
static lv_obj_t *calib_create_crosshair(lv_obj_t *parent, lv_color_t color);
static void calib_create_reference_dot(lv_obj_t *parent, int32_t x, int32_t y, lv_color_t color);

static void calib_cross_set_pos(int32_t x, int32_t y)
{
    if (g_cross == NULL) {
        return;
    }
    lv_obj_set_pos(g_cross,
                   (lv_coord_t)(x - CALIB_CROSS_HALF),
                   (lv_coord_t)(y - CALIB_CROSS_HALF));
}

static bool calib_adc_is_distinct(uint8_t step, uint16_t adc_x, uint16_t adc_y)
{
    int32_t dx;
    int32_t dy;
    uint32_t dist2;

    if (step == 0U) {
        return true;
    }

    for (uint8_t i = 0; i < step; i++) {
        dx = (int32_t)adc_x - (int32_t)g_adc[i].adc_x;
        dy = (int32_t)adc_y - (int32_t)g_adc[i].adc_y;
        dist2 = (uint32_t)(dx * dx + dy * dy);
        if (dist2 < CALIB_ADC_MIN_DIST2) {
            return false;
        }
    }
    return true;
}

static void calib_refresh_log(void)
{
    static char buf[320];
    size_t used = 0U;
    int n;

    if (g_lbl_log == NULL) {
        return;
    }

    n = lv_snprintf(buf, sizeof(buf), "Captured ADC:\n");
    if (n < 0) {
        return;
    }
    used = (size_t)n;

    for (uint8_t i = 0; i < TOUCH_CALIB_POINT_COUNT; i++) {
        if (used >= sizeof(buf)) {
            break;
        }
        if (i < g_step) {
            n = lv_snprintf(buf + used, sizeof(buf) - used,
                            "%s raw(%u,%u) tgt(%d,%d)\n",
                            g_step_short[i],
                            (unsigned)g_adc[i].adc_x, (unsigned)g_adc[i].adc_y,
                            (int)g_target[i].x, (int)g_target[i].y);
        } else {
            n = lv_snprintf(buf + used, sizeof(buf) - used,
                            "%s --\n", g_step_short[i]);
        }
        if (n < 0) {
            break;
        }
        used += (size_t)n;
    }
    lv_label_set_text(g_lbl_log, buf);
}

#if !defined(PC_SIMULATOR)
static void calib_update_live_raw(void)
{
    uint16_t adc_x = 0;
    uint16_t adc_y = 0;
    lv_indev_state_t state = LV_INDEV_STATE_RELEASED;
    static char buf[48];

    if (g_lbl_live_raw == NULL || g_phase != CALIB_UI_WIZARD) {
        return;
    }

    DISP_GetLastTouchRaw(&adc_x, &adc_y);
    (void)DISP_GetTouchPoint(NULL, NULL, &state);

    if (state == LV_INDEV_STATE_PRESSED && (adc_x != 0U || adc_y != 0U)) {
        lv_snprintf(buf, sizeof(buf), "Live raw: (%u, %u)", (unsigned)adc_x, (unsigned)adc_y);
    } else {
        lv_label_set_text(g_lbl_live_raw, "Live raw: --- (press cross)");
        return;
    }
    lv_label_set_text(g_lbl_live_raw, buf);
}
#else
static void calib_update_live_raw(void)
{
    if (g_lbl_live_raw != NULL) {
        lv_label_set_text(g_lbl_live_raw, "Live raw: PC mouse sim");
    }
}
#endif

static void calib_update_step_ui(void)
{
    static char buf[64];

    if (g_step >= TOUCH_CALIB_POINT_COUNT) {
        return;
    }

    lv_label_set_text(g_lbl_title, g_step_titles[g_step]);
    calib_cross_set_pos(g_target[g_step].x, g_target[g_step].y);
    lv_snprintf(buf, sizeof(buf), "Target: (%d, %d)",
                (int)g_target[g_step].x, (int)g_target[g_step].y);
    lv_label_set_text(g_lbl_target, buf);
    calib_refresh_log();
}

static void calib_show_done_ui(bool saved)
{
    lv_obj_clean(g_root);
    g_panel = lv_obj_create(g_root);
    lv_obj_set_size(g_panel, LV_PCT(100), LV_PCT(100));
    lv_obj_center(g_panel);
    lv_obj_clear_flag(g_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(g_panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_flex_flow(g_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_panel, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title = lv_label_create(g_panel);
    lv_label_set_text(title, saved ? "Calibration Complete" : "Save Failed");
    lv_obj_set_style_text_color(title, lv_color_hex(0x2E7D32), 0);

    lv_obj_t *hint = lv_label_create(g_panel);
    lv_label_set_text(hint,
                       saved ? "Calibration saved to SD (Config/touch_calib.bin)."
                             : "SD save failed. Check SD card then retry.");
    lv_obj_set_width(hint, LV_PCT(80));
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *note = lv_label_create(g_panel);
    lv_label_set_text(note, "Set GUI_TOUCH_CALIB_MODE=0 and rebuild.");
    lv_obj_set_width(note, LV_PCT(80));
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(note, LV_TEXT_ALIGN_CENTER, 0);

    g_phase = CALIB_UI_DONE;
}

static void calib_restart_wizard(void)
{
    GUI_CalibInit();
}

static void calib_verify_save_btn_cb(lv_event_t *e)
{
    bool saved = false;
    (void)e;
    if (g_result_timer != NULL) {
        lv_timer_del(g_result_timer);
        g_result_timer = NULL;
    }
#if !defined(PC_SIMULATOR)
    saved = TouchCalib_Save();
#else
    saved = TouchCalib_Save(); /* PC：写入 sd_root/Config/touch_calib.bin */
#endif
    calib_show_done_ui(saved);
}

static void calib_verify_retry_btn_cb(lv_event_t *e)
{
    (void)e;
    calib_restart_wizard();
}

static lv_obj_t *calib_result_create_marker(lv_obj_t *parent, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *m = lv_obj_create(parent);
    lv_coord_t half = 10;

    lv_obj_set_size(m, half * 2 + 1, half * 2 + 1);
    lv_obj_set_pos(m, x - half, y - half);
    lv_obj_clear_flag(m, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(m, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(m, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m, 0, 0);

    lv_obj_t *hline = lv_obj_create(m);
    lv_obj_set_size(hline, half * 2 + 1, 2);
    lv_obj_set_pos(hline, 0, half - 1);
    lv_obj_set_style_bg_color(hline, lv_color_hex(0xE53935), 0);
    lv_obj_set_style_border_width(hline, 0, 0);

    lv_obj_t *vline = lv_obj_create(m);
    lv_obj_set_size(vline, 2, half * 2 + 1);
    lv_obj_set_pos(vline, half - 1, 0);
    lv_obj_set_style_bg_color(vline, lv_color_hex(0xE53935), 0);
    lv_obj_set_style_border_width(vline, 0, 0);

    return m;
}

#if !defined(PC_SIMULATOR)
static void calib_result_timer_cb(lv_timer_t *timer)
{
    static lv_indev_state_t last_state = LV_INDEV_STATE_RELEASED;
    lv_indev_state_t state = LV_INDEV_STATE_RELEASED;
    int32_t x = 0;
    int32_t y = 0;
    static char buf[128];
    int32_t best_dx = 0;
    int32_t best_dy = 0;
    int32_t best_dist2 = INT32_MAX;
    int32_t ref_x = 0;
    int32_t ref_y = 0;

    (void)timer;

    if (g_phase != CALIB_UI_RESULT || g_lbl_status == NULL) {
        return;
    }

    if (!DISP_GetTouchPoint(&x, &y, &state)) {
        return;
    }

    if (state == LV_INDEV_STATE_RELEASED && last_state == LV_INDEV_STATE_PRESSED) {
        for (uint8_t i = 0; i < TOUCH_CALIB_POINT_COUNT; i++) {
            int32_t dx = x - g_target[i].x;
            int32_t dy = y - g_target[i].y;
            int32_t dist2 = dx * dx + dy * dy;
            if (dist2 < best_dist2) {
                best_dist2 = dist2;
                best_dx = dx;
                best_dy = dy;
                ref_x = g_target[i].x;
                ref_y = g_target[i].y;
            }
        }

        if (g_panel != NULL) {
            calib_result_create_marker(g_panel, (lv_coord_t)x, (lv_coord_t)y);
        }

        lv_snprintf(buf, sizeof(buf),
                    "Touch (%d,%d) ref(%d,%d) err(%+d,%+d)",
                    (int)x, (int)y, (int)ref_x, (int)ref_y,
                    (int)best_dx, (int)best_dy);
        lv_label_set_text(g_lbl_status, buf);
    }

    last_state = state;
}
#endif

static void calib_show_result_ui(void)
{
    lv_obj_t *btn_save;
    lv_obj_t *btn_retry;
    lv_obj_t *lbl;

    if (g_poll_timer != NULL) {
        lv_timer_del(g_poll_timer);
        g_poll_timer = NULL;
    }
    if (g_result_timer != NULL) {
        lv_timer_del(g_result_timer);
        g_result_timer = NULL;
    }

    lv_obj_clean(g_root);
    g_phase = CALIB_UI_RESULT;

#if !defined(PC_SIMULATOR)
    DISP_SetTouchCalibCollectMode(false);
#endif

    g_panel = lv_obj_create(g_root);
    lv_obj_set_size(g_panel, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(g_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(g_panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(g_panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(g_panel, 0, 0);
    lv_obj_set_style_pad_all(g_panel, 0, 0);

    for (uint8_t i = 0; i < TOUCH_CALIB_POINT_COUNT; i++) {
        calib_create_reference_dot(g_panel,
                                   g_target[i].x, g_target[i].y,
                                   lv_color_hex(0x1E88E5));
    }

    g_lbl_title = lv_label_create(g_panel);
    lv_obj_align(g_lbl_title, LV_ALIGN_TOP_MID, 0, 12);
    lv_label_set_text(g_lbl_title, "Calibration OK - tap Save below");
    lv_obj_set_style_text_color(g_lbl_title, lv_color_hex(0x212121), 0);

    lbl = lv_label_create(g_panel);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 36);
    lv_obj_set_width(lbl, LV_PCT(90));
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lbl, "Optional: tap blue dots to check err.");

    g_lbl_status = lv_label_create(g_panel);
    lv_obj_align(g_lbl_status, LV_ALIGN_TOP_MID, 0, 72);
    lv_obj_set_width(g_lbl_status, LV_PCT(92));
    lv_label_set_long_mode(g_lbl_status, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(g_lbl_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(g_lbl_status, "Press green Save & Finish button.");

    btn_save = lv_btn_create(g_panel);
    lv_obj_set_size(btn_save, 300, 52);
    lv_obj_align(btn_save, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_style_bg_color(btn_save, lv_color_hex(0x2E7D32), 0);
    lv_obj_add_event_cb(btn_save, calib_verify_save_btn_cb, LV_EVENT_CLICKED, NULL);
    lbl = lv_label_create(btn_save);
    lv_label_set_text(lbl, "Save & Finish");
    lv_obj_center(lbl);

    btn_retry = lv_btn_create(g_panel);
    lv_obj_set_size(btn_retry, 300, 44);
    lv_obj_align(btn_retry, LV_ALIGN_CENTER, 0, 110);
    lv_obj_add_event_cb(btn_retry, calib_verify_retry_btn_cb, LV_EVENT_CLICKED, NULL);
    lbl = lv_label_create(btn_retry);
    lv_label_set_text(lbl, "Recalibrate");
    lv_obj_center(lbl);

#if !defined(PC_SIMULATOR)
    g_result_timer = lv_timer_create(calib_result_timer_cb, 50, NULL);
#endif
}

static void calib_advance_with_sample(uint16_t adc_x, uint16_t adc_y)
{
    static char buf[96];

    if (g_phase != CALIB_UI_WIZARD) {
        return;
    }

    if (g_step >= TOUCH_CALIB_POINT_COUNT) {
        return;
    }

    if (!calib_adc_is_distinct(g_step, adc_x, adc_y)) {
        lv_label_set_text(g_lbl_status, "Too close to prior point. Retry.");
        return;
    }

    g_adc[g_step].adc_x = adc_x;
    g_adc[g_step].adc_y = adc_y;
    g_step++;

    lv_snprintf(buf, sizeof(buf), "OK %u/%u raw(%u,%u)",
                (unsigned)g_step, TOUCH_CALIB_POINT_COUNT,
                (unsigned)adc_x, (unsigned)adc_y);
    lv_label_set_text(g_lbl_status, buf);
    calib_refresh_log();

    if (g_step < TOUCH_CALIB_POINT_COUNT) {
        calib_update_step_ui();
        return;
    }

    if (!TouchCalib_ComputeFrom4Points(g_adc, g_target)) {
        g_step = 0;
        calib_update_step_ui();
        lv_label_set_text(g_lbl_status, "Compute failed. Retry 5 points.");
        return;
    }

#if !defined(PC_SIMULATOR)
    {
        bool saved;

        DISP_SetTouchCalibCollectMode(false);
        DISP_ResetTouchState();
        saved = TouchCalib_Save();
        calib_show_done_ui(saved);
    }
#else
    calib_show_result_ui();
#endif
}

#if defined(PC_SIMULATOR)
static void calib_pc_release_cb(lv_event_t *e)
{
    lv_indev_t *indev;
    lv_point_t pt;

    if (lv_event_get_code(e) != LV_EVENT_RELEASED || g_phase != CALIB_UI_WIZARD) {
        return;
    }

    indev = lv_indev_get_act();
    if (indev == NULL) {
        return;
    }

    lv_indev_get_point(indev, &pt);
    calib_advance_with_sample(
        (uint16_t)(pt.x * 1023 / (LCD_WIDTH - 1)),
        (uint16_t)(pt.y * 1023 / (LCD_HEIGHT - 1)));
}
#endif

#if !defined(PC_SIMULATOR)
static void calib_poll_timer_cb(lv_timer_t *timer)
{
    uint16_t adc_x = 0;
    uint16_t adc_y = 0;

    (void)timer;

    if (g_phase != CALIB_UI_WIZARD) {
        return;
    }

    calib_update_live_raw();

    if (!DISP_TakeCalibSample(&adc_x, &adc_y)) {
        return;
    }

    calib_advance_with_sample(adc_x, adc_y);
}
#endif

static lv_obj_t *calib_create_crosshair(lv_obj_t *parent, lv_color_t color)
{
    lv_obj_t *m = lv_obj_create(parent);
    lv_coord_t size = CALIB_CROSS_HALF * 2 + 1;

    lv_obj_set_size(m, size, size);
    lv_obj_clear_flag(m, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(m, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(m, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m, 0, 0);

    lv_obj_t *hline = lv_obj_create(m);
    lv_obj_set_size(hline, size, 3);
    lv_obj_set_pos(hline, 0, CALIB_CROSS_HALF - 1);
    lv_obj_set_style_bg_color(hline, color, 0);
    lv_obj_set_style_border_width(hline, 0, 0);

    lv_obj_t *vline = lv_obj_create(m);
    lv_obj_set_size(vline, 3, size);
    lv_obj_set_pos(vline, CALIB_CROSS_HALF - 1, 0);
    lv_obj_set_style_bg_color(vline, color, 0);
    lv_obj_set_style_border_width(vline, 0, 0);

    lv_obj_t *dot = lv_obj_create(m);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_pos(dot, CALIB_CROSS_HALF - 4, CALIB_CROSS_HALF - 4);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, color, 0);
    lv_obj_set_style_border_width(dot, 0, 0);

    return m;
}

static void calib_create_reference_dot(lv_obj_t *parent, int32_t x, int32_t y, lv_color_t color)
{
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_set_size(dot, 12, 12);
    lv_obj_set_pos(dot, (lv_coord_t)(x - 6), (lv_coord_t)(y - 6));
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, color, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
}

static void calib_create_wizard_ui(void)
{
    g_panel = lv_obj_create(g_root);
    lv_obj_set_size(g_panel, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(g_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(g_panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(g_panel, 0, 0);
#if defined(PC_SIMULATOR)
    lv_obj_add_flag(g_panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_panel, calib_pc_release_cb, LV_EVENT_RELEASED, NULL);
#endif

    for (uint8_t i = 0; i < TOUCH_CALIB_POINT_COUNT; i++) {
        calib_create_reference_dot(g_panel,
                                   g_target[i].x, g_target[i].y,
                                   lv_color_hex(0x90CAF9));
    }

    g_lbl_title = lv_label_create(g_panel);
    lv_obj_align(g_lbl_title, LV_ALIGN_TOP_MID, 0, 8);

    g_lbl_target = lv_label_create(g_panel);
    lv_obj_align(g_lbl_target, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_text_color(g_lbl_target, lv_color_hex(0x1565C0), 0);

    g_lbl_live_raw = lv_label_create(g_panel);
    lv_obj_align(g_lbl_live_raw, LV_ALIGN_TOP_LEFT, 8, 54);
    lv_obj_set_style_text_color(g_lbl_live_raw, lv_color_hex(0xE65100), 0);

    g_lbl_log = lv_label_create(g_panel);
    lv_obj_align(g_lbl_log, LV_ALIGN_TOP_LEFT, 8, 76);
    lv_obj_set_width(g_lbl_log, 260);
    lv_label_set_long_mode(g_lbl_log, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(g_lbl_log, 2, 0);

    g_lbl_status = lv_label_create(g_panel);
    lv_obj_align(g_lbl_status, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_width(g_lbl_status, LV_PCT(92));
    lv_label_set_long_mode(g_lbl_status, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(g_lbl_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(g_lbl_status, "Tap cross center and release.");

    g_cross = calib_create_crosshair(g_panel, lv_color_hex(0xE53935));
    calib_update_step_ui();
    calib_update_live_raw();
}

void GUI_CalibInit(void)
{
    if (g_poll_timer != NULL) {
        lv_timer_del(g_poll_timer);
        g_poll_timer = NULL;
    }
    if (g_result_timer != NULL) {
        lv_timer_del(g_result_timer);
        g_result_timer = NULL;
    }

    g_root = lv_scr_act();
    g_step = 0;
    g_phase = CALIB_UI_WIZARD;

    lv_obj_clean(g_root);
    lv_obj_set_style_bg_color(g_root, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_all(g_root, 0, 0);
    lv_obj_set_style_border_width(g_root, 0, 0);

    TouchCalib_GetDefaultTargets(g_target);
    calib_create_wizard_ui();

#if !defined(PC_SIMULATOR)
    DISP_SetTouchCalibCollectMode(true);
    g_poll_timer = lv_timer_create(calib_poll_timer_cb, 50, NULL);
#endif
}

bool GUI_CalibNeedsRun(void)
{
#if defined(PC_SIMULATOR)
    return false;
#else
    return !TouchCalib_IsValid();
#endif
}
