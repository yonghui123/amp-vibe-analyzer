/**
 * @file    screen_channel_setup.c
 * @brief   SignalX GUI - Channel Setup Screen (Tab 1)
 *
 *  Features:
 *   - 8-row x 9-column editable configuration table (per PRD)
 *   - Enable checkbox toggle, type dropdown (General/Vibration),
 *     unit dropdown (context-sensitive), frequency spinbox, coefficient spinbox
 *   - Disabled rows grayed out
 *   - Type change updates available units
 *   - Save validates and writes to Config_Store, Reset restores defaults
 */

/* ==================== Includes ==================== */
#include <stdlib.h>
#include <stdio.h>
#include "lvgl.h"
#include "gui_core.h"
#include "gui_widgets.h"
#include "config.h"
#include "config_store.h"
#include "channel_data.h"
#include "acq_pipeline.h"

/* ==================== Constants ==================== */
#define CH_TABLE_ROWS    8
#define CH_TABLE_COLS    9

/* Column indices */
enum {
    COL_CH_NUM   = 0,
    COL_ENABLE   = 1,
    COL_NAME     = 2,
    COL_TYPE     = 3,
    COL_UNIT     = 4,
    COL_FREQ     = 5,
    COL_COEFF    = 6,
    COL_OFFSET   = 7,
    COL_SENSITIVITY = 8
};

/* ==================== Static data ==================== */
/* g_ch_configs 指向共享数据的指针, 不要在这里分配数组 */
static ChannelConfig_t *g_ch_configs = NULL;
static uint8_t g_ch_count = 0;

/* 单位列表已移至 channel_data.c (共享) */

/* Table header */
static const char *g_headers[] = {
    "Ch#", "Enable", "Name", "Type", "Unit", "Freq(Hz)", "Coeff", "Offset", "Sens"
};

/* ==================== Static GUI objects ==================== */
static lv_obj_t *g_ch_table = NULL;
static lv_obj_t *g_ch_btn_save = NULL;
static lv_obj_t *g_ch_btn_reset = NULL;
static int16_t g_selected_row = -1;       /* Currently selected data row (-1 = none) */
static uint16_t g_press_row = LV_TABLE_CELL_NONE;
static uint16_t g_press_col = LV_TABLE_CELL_NONE;
static lv_obj_t *g_popup_kb = NULL;       /* 虚拟键盘 (lv_keyboard) */
static lv_group_t *g_popup_group = NULL;  /* 弹窗专用独立 group */
static lv_group_t *g_main_group  = NULL;  /* 保存的主 group 指针 */

/* 弹窗打开前调用: 保存主group, 创建弹窗专用group */
static void ch_popup_enter(void)
{
    g_main_group = lv_group_get_default();
    if (g_popup_group) { lv_group_del(g_popup_group); g_popup_group = NULL; }
    g_popup_group = lv_group_create();
    lv_group_set_default(g_popup_group);
}

/* 关闭弹窗前调用: 恢复主group + 销毁键盘, 防止焦点跳到 tabview */
static void ch_popup_pre_close(void)
{
    /* 先恢复主group为default, 后续删除弹窗对象时焦点变动只在已废弃的popup_group内 */
    if (g_main_group) lv_group_set_default(g_main_group);
    /* 销毁虚拟键盘 */
    if (g_popup_kb) { lv_obj_del(g_popup_kb); g_popup_kb = NULL; }
}

/* 关闭弹窗后调用: 删除弹窗group并聚焦回表格 */
static void ch_popup_post_close(void)
{
    if (g_popup_group) { lv_group_del(g_popup_group); g_popup_group = NULL; }
    if (g_ch_table) lv_group_focus_obj(g_ch_table);
}

/* Right panel objects */
static lv_obj_t *g_right_panel = NULL;
static lv_obj_t *g_general_panel = NULL;
static lv_obj_t *g_vibration_panel = NULL;
static lv_obj_t *g_hint_label = NULL;
static lv_obj_t *g_lbl_threshold_val = NULL;
static lv_obj_t *g_lbl_delay_val = NULL;
static lv_obj_t *g_lbl_sens_val = NULL;
static lv_obj_t *g_lbl_suggest_val = NULL;
static lv_obj_t *g_lbl_alarm_val = NULL;

/* Right panel edit field IDs */
enum { AUX_THRESHOLD = 0, AUX_DELAY, AUX_SENSITIVITY, AUX_SUGGESTED, AUX_ALARM_VAL };

/* ==================== Forward declarations ==================== */
static void ch_table_draw_event_cb(lv_event_t *e);
static void ch_table_click_event_cb(lv_event_t *e);
static void ch_btn_save_cb(lv_event_t *e);
static void ch_btn_reset_cb(lv_event_t *e);
static void ch_refresh_table_display(void);
static void ch_load_configs(void);
static void ch_popup_edit_name(uint16_t row);
static void ch_popup_edit_type(uint16_t row);
static void ch_popup_edit_unit(uint16_t row);
static void ch_popup_edit_spinbox(uint16_t row, uint16_t col);
static void ch_aux_edit_spinbox(uint8_t field, uint8_t decimals);
static void ch_aux_spinbox_ok_cb(lv_event_t *e);
static void ch_aux_row_click_cb(lv_event_t *e);
static void ch_create_aux_panel(lv_obj_t *parent);
static void ch_update_right_panel(void);
static void ch_refresh_aux_panel(void);

/* ==================== Popup editor helpers ==================== */

typedef struct {
    uint16_t row;
    uint16_t col;
} ch_edit_info_t;

static ch_edit_info_t g_edit_info;

/* ---- Name editor (custom panel + keyboard) ---- */
static void ch_name_kb_ready_cb(lv_event_t *e)
{
    lv_obj_t *kb = lv_event_get_target(e);
    lv_obj_t *ta = lv_keyboard_get_textarea(kb);
    if (ta == NULL) return;

    const char *txt = lv_textarea_get_text(ta);
    uint16_t row = g_edit_info.row;

    if (txt != NULL && txt[0] != '\0') {
        snprintf(g_ch_configs[row].channel_name,
                 sizeof(g_ch_configs[row].channel_name),
                 "%s", txt);
        ch_refresh_table_display();
    }

    /* 先置空键盘指针, 再删除遮罩层 (含子对象), 避免 double-free */
    g_popup_kb = NULL;
    lv_obj_t *overlay = lv_obj_get_parent(kb);

    /* Group隔离: 恢复主group后再销毁弹窗对象 */
    ch_popup_pre_close();
    if (overlay) lv_obj_del(overlay);
    ch_popup_post_close();
}

static void ch_popup_edit_name(uint16_t row)
{
    g_edit_info.row = row;
    g_edit_info.col = COL_NAME;
    ch_popup_enter();

    /* 全屏遮罩层 (阻止背景交互) */
    lv_obj_t *overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(overlay, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_30, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);

    /* 自定义弹窗面板 (遮罩层的子对象) */
    lv_obj_t *panel = lv_obj_create(overlay);
    lv_obj_set_size(panel, 280, 100);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, -60);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_radius(panel, 6, 0);
    lv_obj_set_style_shadow_width(panel, 8, 0);
    lv_obj_set_style_shadow_opa(panel, LV_OPA_30, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(panel, 10, 0);
    lv_obj_set_style_pad_gap(panel, 6, 0);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, "Edit Name");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    lv_obj_t *ta = lv_textarea_create(panel);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, 20);
    lv_textarea_set_text(ta, g_ch_configs[row].channel_name);
    lv_obj_set_width(ta, LV_PCT(100));
    lv_obj_set_height(ta, 40);
    lv_obj_clear_flag(ta, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_top(ta, 8, 0);
    lv_obj_set_style_pad_bottom(ta, 8, 0);

    /* 加入弹窗group并获得焦点 */
    if (g_popup_group) { lv_group_add_obj(g_popup_group, ta); lv_group_focus_obj(ta); }

    /* 虚拟键盘 (遮罩层的子对象, 屏幕底部 35%) */
    g_popup_kb = lv_keyboard_create(overlay);
    lv_keyboard_set_textarea(g_popup_kb, ta);
    lv_obj_set_size(g_popup_kb, LV_PCT(100), LV_PCT(35));
    lv_obj_align(g_popup_kb, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* 键盘 OK 键触发保存并关闭 */
    lv_obj_add_event_cb(g_popup_kb, ch_name_kb_ready_cb, LV_EVENT_READY, NULL);
}

/* ---- Type editor (dropdown list popup) ---- */
static void ch_type_close_cb(lv_event_t *e)
{
    ch_popup_pre_close();
    lv_obj_t *btnmatrix = lv_event_get_target(e);
    lv_obj_t *mbox = lv_obj_get_parent(btnmatrix);
    if (mbox != NULL) lv_msgbox_close(mbox);
    ch_popup_post_close();
}

static void ch_type_popup_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *dd = lv_event_get_target(e);
        uint16_t selected = lv_dropdown_get_selected(dd);
        uint16_t row = g_edit_info.row;

        g_ch_configs[row].ch_type = (uint8_t)selected;

        /* 切换类型时重置单位索引 (通用和振动单位表不同) */
        g_ch_configs[row].unit_index = 0;

        /* Update unit to first of new type and set default aux params */
        if (selected == CH_TYPE_GENERAL) {
            g_ch_configs[row].params.general.alarm_threshold = 50.0f;
            g_ch_configs[row].params.general.alarm_delay_ms = 1000;
        } else {
            g_ch_configs[row].params.vibration.sensitivity = 20.0f;
            g_ch_configs[row].params.vibration.suggested_value = 0.50f;
            g_ch_configs[row].params.vibration.alarm_value = 1.00f;
        }

        ch_refresh_table_display();
        ch_update_right_panel();
        /* Don't auto-close; user clicks Close button to dismiss */
    }
}

static void ch_popup_edit_type(uint16_t row)
{
    g_edit_info.row = row;
    g_edit_info.col = COL_TYPE;
    ch_popup_enter();

    static const char *btns[] = {"Close", ""};
    lv_obj_t *mbox = lv_msgbox_create(NULL, "Select Type", "", btns, false);
    lv_obj_set_width(mbox, 220);

    lv_obj_t *content = lv_msgbox_get_content(mbox);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(content, 12, 0);

    lv_obj_t *dd = lv_dropdown_create(content);
    lv_dropdown_set_options(dd, "General\nVibration");
    lv_dropdown_set_selected(dd, g_ch_configs[row].ch_type);
    lv_obj_set_width(dd, LV_PCT(100));
    lv_obj_center(mbox);

    lv_obj_add_event_cb(dd, ch_type_popup_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Close button on msgbox */
    lv_obj_t *btn_close = lv_msgbox_get_btns(mbox);
    lv_obj_add_event_cb(btn_close, ch_type_close_cb, LV_EVENT_CLICKED, NULL);
}

/* ---- Unit editor (dropdown list popup, context-sensitive) ---- */
static void ch_unit_close_cb(lv_event_t *e)
{
    ch_popup_pre_close();
    lv_obj_t *btnmatrix = lv_event_get_target(e);
    lv_obj_t *mbox = lv_obj_get_parent(btnmatrix);
    if (mbox != NULL) lv_msgbox_close(mbox);
    ch_popup_post_close();
}

static void ch_unit_popup_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *dd = lv_event_get_target(e);
        uint16_t row = g_edit_info.row;
        /* 保存单位索引到共享数据 */
        g_ch_configs[row].unit_index = (uint8_t)lv_dropdown_get_selected(dd);
        ch_refresh_table_display();
        /* Don't auto-close; user clicks Close button */
    }
}

static void ch_popup_edit_unit(uint16_t row)
{
    g_edit_info.row = row;
    g_edit_info.col = COL_UNIT;
    ch_popup_enter();

    const char *options;
    if (g_ch_configs[row].ch_type == CH_TYPE_GENERAL) {
        options = "A\nkW";
    } else {
        options = "um\nmm/s\nm/s\nm/s2";
    }

    static const char *btns[] = {"Close", ""};
    lv_obj_t *mbox = lv_msgbox_create(NULL, "Select Unit", "", btns, false);
    lv_obj_set_width(mbox, 220);

    lv_obj_t *content = lv_msgbox_get_content(mbox);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(content, 12, 0);

    lv_obj_t *dd = lv_dropdown_create(content);
    lv_dropdown_set_options(dd, options);
    /* 使用当前已保存的单位索引 */
    lv_dropdown_set_selected(dd, g_ch_configs[row].unit_index);
    lv_obj_set_width(dd, LV_PCT(100));
    lv_obj_center(mbox);

    lv_obj_add_event_cb(dd, ch_unit_popup_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Close button on msgbox */
    lv_obj_t *btn_close = lv_msgbox_get_btns(mbox);
    lv_obj_add_event_cb(btn_close, ch_unit_close_cb, LV_EVENT_CLICKED, NULL);
}

/* ---- Spinbox editor (number popup, overlay structure) ---- */

/* 关闭 spinbox overlay 的公共辅助函数 */
static void ch_spinbox_overlay_close(lv_obj_t *spinbox)
{
    g_popup_kb = NULL;
    lv_obj_t *overlay = lv_obj_get_parent(spinbox);  /* spinbox -> panel -> overlay */
    if (overlay) overlay = lv_obj_get_parent(overlay);
    ch_popup_pre_close();
    if (overlay) lv_obj_del(overlay);
    ch_popup_post_close();
}

/* OK 按钮回调 (表格列数字输入) */
static void ch_spinbox_ok_btn_cb(lv_event_t *e)
{
    lv_obj_t *ta = (lv_obj_t *)lv_event_get_user_data(e);
    const char *text = lv_textarea_get_text(ta);
    int32_t val = (text && text[0]) ? (int32_t)atoi(text) : 0;
    uint16_t row = g_edit_info.row;
    uint16_t col = g_edit_info.col;

    if (col == COL_FREQ) {
        if (val < 1) val = 1; if (val > 100000) val = 100000;
        g_ch_configs[row].sample_freq_hz = (uint16_t)val;
    } else if (col == COL_COEFF) {
        if (val < 1) val = 1; if (val > 9999) val = 9999;
        g_ch_configs[row].coefficient = (float)val;
    } else if (col == COL_OFFSET) {
        if (val < -10000) val = -10000; if (val > 10000) val = 10000;
        g_ch_configs[row].offset = (float)val;
    } else if (col == COL_SENSITIVITY) {
        if (val < 1) val = 1; if (val > 10000) val = 10000;
        g_ch_configs[row].params.vibration.sensitivity = (float)val;
    }

    ch_refresh_table_display();
    ch_spinbox_overlay_close(ta);
}

static void ch_popup_edit_spinbox(uint16_t row, uint16_t col)
{
    g_edit_info.row = row;
    g_edit_info.col = col;
    ch_popup_enter();

    int32_t cur_val = 0;
    int32_t min_val = 1;
    int32_t max_val = 100000;
    int32_t step   = 1;
    uint8_t digits = 4;
    const char *title = "";

    if (col == COL_FREQ) {
        cur_val = (int32_t)g_ch_configs[row].sample_freq_hz;
        min_val = 1;  max_val = 100000;  step = 10;
        title   = "Frequency (Hz)";
    } else if (col == COL_COEFF) {
        cur_val = (int32_t)g_ch_configs[row].coefficient;
        min_val = 1;  max_val = 9999;  step = 1;
        title   = "Coefficient";
    } else if (col == COL_OFFSET) {
        cur_val = (int32_t)g_ch_configs[row].offset;
        min_val = -10000;  max_val = 10000;  step = 1;  digits = 5;
        title   = "Offset";
    } else if (col == COL_SENSITIVITY) {
        cur_val = (int32_t)g_ch_configs[row].params.vibration.sensitivity;
        min_val = 1;  max_val = 10000;  step = 1;
        title   = "Sensitivity";
    }

    /* 全屏遮罩层 (阻止背景交互) */
    lv_obj_t *overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(overlay, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_30, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);

    /* 自定义弹窗面板 (遮罩层的子对象) */
    lv_obj_t *panel = lv_obj_create(overlay);
    lv_obj_set_size(panel, 280, LV_SIZE_CONTENT);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, -60);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_radius(panel, 6, 0);
    lv_obj_set_style_shadow_width(panel, 8, 0);
    lv_obj_set_style_shadow_opa(panel, LV_OPA_30, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(panel, 10, 0);
    lv_obj_set_style_pad_gap(panel, 6, 0);

    lv_obj_t *lbl_title = lv_label_create(panel);
    lv_label_set_text(lbl_title, title);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, 0);

    /* 使用 textarea 代替 spinbox，避免前导零 */
    lv_obj_t *ta = lv_textarea_create(panel);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, 8);
    /* 将当前值转为字符串显示（无前导零） */
    char val_buf[16];
    snprintf(val_buf, sizeof(val_buf), "%d", (int)cur_val);
    lv_textarea_set_text(ta, val_buf);
    lv_obj_set_width(ta, LV_PCT(100));
    lv_obj_set_height(ta, 40);
    lv_obj_clear_flag(ta, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_14, 0);
    lv_obj_set_style_pad_top(ta, 8, 0);
    lv_obj_set_style_pad_bottom(ta, 8, 0);

    /* 加入弹窗 group 并获得焦点 */
    if (g_popup_group) { lv_group_add_obj(g_popup_group, ta); lv_group_focus_obj(ta); }

    /* OK 按钮 */
    lv_obj_t *btn_ok = lv_btn_create(panel);
    lv_obj_set_size(btn_ok, LV_PCT(100), 32);
    lv_obj_set_style_bg_color(btn_ok, lv_color_hex(0x007BFF), 0);
    lv_obj_set_style_bg_opa(btn_ok, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_ok, 0, 0);
    lv_obj_set_style_radius(btn_ok, 3, 0);
    lv_obj_t *lbl_ok = lv_label_create(btn_ok);
    lv_label_set_text(lbl_ok, "OK");
    lv_obj_set_style_text_color(lbl_ok, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl_ok);
    lv_obj_add_event_cb(btn_ok, ch_spinbox_ok_btn_cb, LV_EVENT_CLICKED, ta);

    /* 虚拟键盘 (遮罩层的子对象, 屏幕底部 35%, 数字模式) */
    g_popup_kb = lv_keyboard_create(overlay);
    lv_keyboard_set_mode(g_popup_kb, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(g_popup_kb, ta);
    lv_obj_set_size(g_popup_kb, LV_PCT(100), LV_PCT(35));
    lv_obj_align(g_popup_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
}

/* ==================== Table draw event (per-cell styling) ==================== */

static void ch_table_draw_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_DRAW_PART_BEGIN) return;

    lv_obj_draw_part_dsc_t *part_dsc = lv_event_get_draw_part_dsc(e);
    if (part_dsc == NULL || part_dsc->type != LV_TABLE_DRAW_PART_CELL) return;

    uint32_t id = part_dsc->id;
    lv_obj_t *table = lv_event_get_target(e);
    uint16_t col_cnt = lv_table_get_col_cnt(table);
    uint16_t row = (uint16_t)(id / col_cnt);

    if (row == 0) {
        /* Header row: gray background, bold text */
        if (part_dsc->rect_dsc) {
            part_dsc->rect_dsc->bg_color = lv_color_hex(0xD0D0D0);
            part_dsc->rect_dsc->bg_opa = LV_OPA_COVER;
            part_dsc->rect_dsc->border_width = 0;
        }
        if (part_dsc->label_dsc) {
            part_dsc->label_dsc->color = lv_color_hex(0x222222);
            part_dsc->label_dsc->opa = LV_OPA_COVER;
        }
    } else if (g_selected_row >= 0 && (uint16_t)(row - 1) == (uint16_t)g_selected_row) {
        /* Selected row: blue background, white text */
        if (part_dsc->rect_dsc) {
            part_dsc->rect_dsc->bg_color = lv_color_hex(0x2979FF);
            part_dsc->rect_dsc->bg_opa = LV_OPA_COVER;
            part_dsc->rect_dsc->border_width = 0;
        }
        if (part_dsc->label_dsc) {
            part_dsc->label_dsc->color = lv_color_hex(0xFFFFFF);
        }
    } else if (row > 0 && !g_ch_configs[row - 1].enabled) {
        /* Disabled row: gray text */
        if (part_dsc->label_dsc) {
            part_dsc->label_dsc->color = lv_color_hex(0xAAAAAA);
        }
    }
}

/* ==================== Right auxiliary panel ==================== */

static uint8_t g_aux_edit_field = 0;
static uint8_t g_aux_edit_decimals = 0;

/* Helper: create one parameter row (label + value) */
static lv_obj_t *create_param_row(lv_obj_t *parent, const char *label_text,
                                   lv_obj_t **val_label_out)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_top(row, 4, 0);
    lv_obj_set_style_pad_bottom(row, 4, 0);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, label_text);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, LV_PCT(100));
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x333333), 0);

    lv_obj_t *val = lv_label_create(row);
    lv_label_set_text(val, "--");
    lv_obj_set_style_text_font(val, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(val, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_color(val, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(val, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(val, 1, 0);
    lv_obj_set_style_border_color(val, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_pad_all(val, 4, 0);
    lv_obj_set_style_radius(val, 3, 0);
    lv_obj_set_width(val, LV_PCT(100));
    lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_LEFT, 0);

    *val_label_out = val;
    return row;
}

/* Helper: add note text below a param row */
static void add_param_note(lv_obj_t *parent, const char *text)
{
    lv_obj_t *note = lv_label_create(parent);
    lv_label_set_text(note, text);
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(note, LV_PCT(100));
    lv_obj_set_style_text_font(note, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(note, lv_color_hex(0x999999), 0);
    lv_obj_set_style_pad_bottom(note, 6, 0);
}

static void ch_create_aux_panel(lv_obj_t *parent)
{
    g_right_panel = parent;
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xF5F5F5), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(parent, 1, 0);
    lv_obj_set_style_border_side(parent, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_color(parent, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_radius(parent, 0, 0);
    lv_obj_set_style_pad_all(parent, 12, 0);

    /* Title */
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Aux Params");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x333333), 0);
    lv_obj_set_style_pad_bottom(title, 10, 0);

    /* ---- General panel ---- */
    g_general_panel = lv_obj_create(parent);
    lv_obj_set_size(g_general_panel, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(g_general_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(g_general_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_general_panel, 0, 0);
    lv_obj_set_style_pad_all(g_general_panel, 0, 0);

    lv_obj_t *sec1 = lv_label_create(g_general_panel);
    lv_label_set_text(sec1, "| Alarm Params");
    lv_obj_set_style_text_font(sec1, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sec1, lv_color_hex(0x2979FF), 0);
    lv_obj_set_style_pad_bottom(sec1, 8, 0);

    lv_obj_t *row_threshold = create_param_row(g_general_panel, "Threshold",
                                                &g_lbl_threshold_val);
    lv_obj_add_flag(row_threshold, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row_threshold, ch_aux_row_click_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)AUX_THRESHOLD);

    lv_obj_t *row_delay = create_param_row(g_general_panel, "Delay(ms)",
                                            &g_lbl_delay_val);
    lv_obj_add_flag(row_delay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row_delay, ch_aux_row_click_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)AUX_DELAY);

    add_param_note(g_general_panel, "(Fixed 1S, alarm after 1S)");

    /* ---- Vibration panel ---- */
    g_vibration_panel = lv_obj_create(parent);
    lv_obj_set_size(g_vibration_panel, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(g_vibration_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(g_vibration_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_vibration_panel, 0, 0);
    lv_obj_set_style_pad_all(g_vibration_panel, 0, 0);

    lv_obj_t *sec2 = lv_label_create(g_vibration_panel);
    lv_label_set_text(sec2, "| Balance Params");
    lv_obj_set_style_text_font(sec2, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sec2, lv_color_hex(0x2979FF), 0);
    lv_obj_set_style_pad_bottom(sec2, 8, 0);

    lv_obj_t *row_sens = create_param_row(g_vibration_panel, "Sens(mV/um)",
                                           &g_lbl_sens_val);
    lv_obj_add_flag(row_sens, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row_sens, ch_aux_row_click_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)AUX_SENSITIVITY);

    lv_obj_t *row_suggest = create_param_row(g_vibration_panel, "Suggest Val",
                                              &g_lbl_suggest_val);
    lv_obj_add_flag(row_suggest, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row_suggest, ch_aux_row_click_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)AUX_SUGGESTED);

    lv_obj_t *row_alarm = create_param_row(g_vibration_panel, "Alarm Val",
                                            &g_lbl_alarm_val);
    lv_obj_add_flag(row_alarm, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row_alarm, ch_aux_row_click_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)AUX_ALARM_VAL);

    add_param_note(g_vibration_panel, "(Must > suggest value)");

    /* Hint label */
    g_hint_label = lv_label_create(parent);
    lv_label_set_text(g_hint_label, "Select a channel to view aux params");
    lv_label_set_long_mode(g_hint_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g_hint_label, LV_PCT(100));
    lv_obj_set_style_text_font(g_hint_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(g_hint_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_pad_top(g_hint_label, 16, 0);

    /* Initially hide both panels */
    lv_obj_add_flag(g_general_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_vibration_panel, LV_OBJ_FLAG_HIDDEN);
}

/* Click handler for aux param rows */
static void ch_aux_row_click_cb(lv_event_t *e)
{
    uint8_t field = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (g_selected_row < 0) return;

    uint16_t idx = (uint16_t)g_selected_row;
    if (!g_ch_configs[idx].enabled) return;

    if (g_ch_configs[idx].ch_type == CH_TYPE_GENERAL) {
        switch (field) {
            case AUX_THRESHOLD: ch_aux_edit_spinbox(AUX_THRESHOLD, 1); break;
            case AUX_DELAY:     ch_aux_edit_spinbox(AUX_DELAY, 0); break;
        }
    } else {
        switch (field) {
            case AUX_SENSITIVITY: ch_aux_edit_spinbox(AUX_SENSITIVITY, 2); break;
            case AUX_SUGGESTED:   ch_aux_edit_spinbox(AUX_SUGGESTED, 2); break;
            case AUX_ALARM_VAL:   ch_aux_edit_spinbox(AUX_ALARM_VAL, 2); break;
        }
    }
}

/* Open spinbox popup for auxiliary parameter (overlay structure) */
static void ch_aux_edit_spinbox(uint8_t field, uint8_t decimals)
{
    if (g_selected_row < 0) return;
    uint16_t idx = (uint16_t)g_selected_row;
    ChannelConfig_t *cfg = &g_ch_configs[idx];
    ch_popup_enter();

    int32_t cur_val = 0;
    int32_t min_val = 0;
    int32_t max_val = 100000;
    int32_t step = 1;
    uint8_t digits = 5;
    const char *title = "";

    g_aux_edit_field = field;
    g_aux_edit_decimals = decimals;

    int32_t mul = 1;
    for (uint8_t d = 0; d < decimals; d++) mul *= 10;

    if (field == AUX_THRESHOLD) {
        cur_val = (int32_t)(cfg->params.general.alarm_threshold * mul);
        min_val = 0; max_val = 99999; step = 1; digits = 5;
        title = "Alarm Threshold";
    } else if (field == AUX_DELAY) {
        cur_val = (int32_t)(cfg->params.general.alarm_delay_ms);
        min_val = 100; max_val = 30000; step = 100; digits = 5;
        title = "Alarm Delay(ms)";
    } else if (field == AUX_SENSITIVITY) {
        cur_val = (int32_t)(cfg->params.vibration.sensitivity * mul);
        min_val = 1; max_val = 10000; step = 1; digits = 4;
        title = "Sensitivity(mV/um)";
    } else if (field == AUX_SUGGESTED) {
        cur_val = (int32_t)(cfg->params.vibration.suggested_value * mul);
        min_val = 1; max_val = 10000; step = 1; digits = 4;
        title = "Balance Suggest";
    } else if (field == AUX_ALARM_VAL) {
        cur_val = (int32_t)(cfg->params.vibration.alarm_value * mul);
        min_val = 1; max_val = 10000; step = 1; digits = 4;
        title = "Balance Alarm";
    }

    /* 全屏遮罩层 (阻止背景交互) */
    lv_obj_t *overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(overlay, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_30, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);

    /* 自定义弹窗面板 (遮罩层的子对象) */
    lv_obj_t *panel = lv_obj_create(overlay);
    lv_obj_set_size(panel, 280, LV_SIZE_CONTENT);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, -60);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_radius(panel, 6, 0);
    lv_obj_set_style_shadow_width(panel, 8, 0);
    lv_obj_set_style_shadow_opa(panel, LV_OPA_30, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(panel, 10, 0);
    lv_obj_set_style_pad_gap(panel, 6, 0);

    lv_obj_t *lbl_title = lv_label_create(panel);
    lv_label_set_text(lbl_title, title);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, 0);

    lv_obj_t *spinbox = lv_spinbox_create(panel);
    lv_spinbox_set_range(spinbox, min_val, max_val);
    lv_spinbox_set_digit_format(spinbox, digits, decimals);
    lv_spinbox_set_step(spinbox, (uint32_t)step);
    lv_spinbox_set_value(spinbox, cur_val);
    lv_obj_set_width(spinbox, LV_PCT(100));
    lv_obj_set_height(spinbox, 40);
    lv_obj_clear_flag(spinbox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_top(spinbox, 8, 0);
    lv_obj_set_style_pad_bottom(spinbox, 8, 0);

    /* 全零时光标移到最左边: 从最高位开始输入; 非零值保持光标在最右(追加模式) */
    if (cur_val == 0) { for (int i = 0; i < (int)(digits - decimals - 1); i++) lv_spinbox_step_prev(spinbox); }

    /* 加入弹窗 group 并获得焦点 */
    if (g_popup_group) { lv_group_add_obj(g_popup_group, spinbox); lv_group_focus_obj(spinbox); }

    /* OK 按钮 */
    lv_obj_t *btn_ok = lv_btn_create(panel);
    lv_obj_set_size(btn_ok, LV_PCT(100), 32);
    lv_obj_set_style_bg_color(btn_ok, lv_color_hex(0x007BFF), 0);
    lv_obj_set_style_bg_opa(btn_ok, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_ok, 0, 0);
    lv_obj_set_style_radius(btn_ok, 3, 0);
    lv_obj_t *lbl_ok = lv_label_create(btn_ok);
    lv_label_set_text(lbl_ok, "OK");
    lv_obj_set_style_text_color(lbl_ok, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl_ok);
    lv_obj_add_event_cb(btn_ok, ch_aux_spinbox_ok_cb, LV_EVENT_CLICKED, spinbox);

    /* 虚拟键盘 (遮罩层的子对象, 屏幕底部 35%, 数字模式) */
    g_popup_kb = lv_keyboard_create(overlay);
    lv_keyboard_set_mode(g_popup_kb, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(g_popup_kb, spinbox);
    lv_obj_set_size(g_popup_kb, LV_PCT(100), LV_PCT(35));
    lv_obj_align(g_popup_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
}

/* OK callback for aux spinbox popup (overlay structure) */
static void ch_aux_spinbox_ok_cb(lv_event_t *e)
{
    lv_obj_t *spinbox = (lv_obj_t *)lv_event_get_user_data(e);
    int32_t val = lv_spinbox_get_value(spinbox);
    uint16_t idx = (uint16_t)g_selected_row;
    ChannelConfig_t *cfg = &g_ch_configs[idx];

    float fval;
    int32_t mul = 1;
    for (uint8_t d = 0; d < g_aux_edit_decimals; d++) mul *= 10;

    switch (g_aux_edit_field) {
        case AUX_THRESHOLD:
            fval = (float)val / (float)mul;
            cfg->params.general.alarm_threshold = fval;
            break;
        case AUX_DELAY:
            cfg->params.general.alarm_delay_ms = (uint16_t)val;
            break;
        case AUX_SENSITIVITY:
            fval = (float)val / (float)mul;
            cfg->params.vibration.sensitivity = fval;
            break;
        case AUX_SUGGESTED:
            fval = (float)val / (float)mul;
            cfg->params.vibration.suggested_value = fval;
            break;
        case AUX_ALARM_VAL:
            fval = (float)val / (float)mul;
            cfg->params.vibration.alarm_value = fval;
            break;
    }

    ch_refresh_aux_panel();
    ch_spinbox_overlay_close(spinbox);
}

/* Update right panel visibility based on selected row */
static void ch_update_right_panel(void)
{
    if (g_selected_row < 0) {
        lv_obj_add_flag(g_general_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_vibration_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_hint_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    uint16_t idx = (uint16_t)g_selected_row;
    if (!g_ch_configs[idx].enabled) {
        lv_obj_add_flag(g_general_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_vibration_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_hint_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (g_ch_configs[idx].ch_type == CH_TYPE_GENERAL) {
        lv_obj_clear_flag(g_general_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_vibration_panel, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(g_general_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_vibration_panel, LV_OBJ_FLAG_HIDDEN);
    }
    /* hint_label always visible */
    ch_refresh_aux_panel();
}

/* Refresh aux panel values from data */
static void ch_refresh_aux_panel(void)
{
    if (g_selected_row < 0) return;
    uint16_t idx = (uint16_t)g_selected_row;
    ChannelConfig_t *cfg = &g_ch_configs[idx];
    char buf[32];

    if (cfg->ch_type == CH_TYPE_GENERAL) {
        GUI_FormatFloat(buf, sizeof(buf), cfg->params.general.alarm_threshold, 1);
        lv_label_set_text(g_lbl_threshold_val, buf);
        snprintf(buf, sizeof(buf), "%u", cfg->params.general.alarm_delay_ms);
        lv_label_set_text(g_lbl_delay_val, buf);
    } else {
        GUI_FormatFloat(buf, sizeof(buf), cfg->params.vibration.sensitivity, 2);
        lv_label_set_text(g_lbl_sens_val, buf);
        GUI_FormatFloat(buf, sizeof(buf), cfg->params.vibration.suggested_value, 2);
        lv_label_set_text(g_lbl_suggest_val, buf);
        GUI_FormatFloat(buf, sizeof(buf), cfg->params.vibration.alarm_value, 2);
        lv_label_set_text(g_lbl_alarm_val, buf);
    }
}

/* ==================== Table click event ==================== */

static void ch_table_click_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *table = lv_event_get_target(e);

    if (code == LV_EVENT_PRESSED) {
        /* Capture row/col during PRESSED (before RELEASED resets them) */
        lv_table_get_selected_cell(table, &g_press_row, &g_press_col);
        return;
    }

    /* LV_EVENT_CLICKED — use values captured during PRESSED */
    uint16_t row = g_press_row;
    uint16_t col = g_press_col;
    if (row == 0) return;  /* Skip header row */

    uint16_t idx = row - 1;
    if (idx >= CH_TABLE_ROWS) return;

    /* Select row and refresh display */
    g_selected_row = (int16_t)idx;
    lv_obj_invalidate(table);
    ch_update_right_panel();

    /* Enable column: toggle on/off */
    if (col == COL_ENABLE) {
        g_ch_configs[idx].enabled = !g_ch_configs[idx].enabled;
        ch_refresh_table_display();
        return;
    }

    /* If disabled row, only allow Enable column */
    if (!g_ch_configs[idx].enabled) return;

    /* Open popup editor based on column */
    switch (col) {
        case COL_NAME:    ch_popup_edit_name(idx);    break;
        case COL_TYPE:    ch_popup_edit_type(idx);    break;
        case COL_UNIT:    ch_popup_edit_unit(idx);    break;
        case COL_FREQ:    ch_popup_edit_spinbox(idx, COL_FREQ);    break;
        case COL_COEFF:   ch_popup_edit_spinbox(idx, COL_COEFF);   break;
        case COL_OFFSET:  ch_popup_edit_spinbox(idx, COL_OFFSET);  break;
        case COL_SENSITIVITY: ch_popup_edit_spinbox(idx, COL_SENSITIVITY); break;
        default: break;
    }
}

/* ==================== Button callbacks ==================== */

static void ch_btn_save_cb(lv_event_t *e)
{
    (void)e;
    char msg[80];

    /* Validate: at least one channel enabled */
    bool any_enabled = false;
    for (uint8_t i = 0; i < g_ch_count; i++) {
        if (g_ch_configs[i].enabled) {
            any_enabled = true;
            break;
        }
    }

    if (!any_enabled) {
        GUI_Toast("Error: Enable at least one channel!", 2000);
        return;
    }

    /* Validate each enabled channel per PRD 3.1.2 */
    for (uint8_t i = 0; i < g_ch_count; i++) {
        ChannelConfig_t *c = &g_ch_configs[i];
        if (!c->enabled) continue;

        /* Name must not be empty */
        if (c->channel_name[0] == '\0') {
            snprintf(msg, sizeof(msg), "Ch%u: Name cannot be empty!", c->channel_id);
            GUI_Toast(msg, 2500);
            return;
        }

        /* Frequency range: General 1~1000Hz, Vibration 10~5000Hz */
        if (c->ch_type == CH_TYPE_GENERAL) {
            if (c->sample_freq_hz < 1 || c->sample_freq_hz > 1000) {
                snprintf(msg, sizeof(msg), "Ch%u: Freq must be 1~1000Hz!", c->channel_id);
                GUI_Toast(msg, 2500);
                return;
            }
        } else {
            if (c->sample_freq_hz < 10 || c->sample_freq_hz > 5000) {
                snprintf(msg, sizeof(msg), "Ch%u: Freq must be 10~5000Hz!", c->channel_id);
                GUI_Toast(msg, 2500);
                return;
            }
        }

        /* Coefficient: 0.001~1000.0 */
        if (c->coefficient < 0.001f || c->coefficient > 1000.0f) {
            snprintf(msg, sizeof(msg), "Ch%u: Coeff must be 0.001~1000!", c->channel_id);
            GUI_Toast(msg, 2500);
            return;
        }

        /* Offset: -1000.0~1000.0 (same range as coefficient, allow negative) */
        if (c->offset < -1000.0f || c->offset > 1000.0f) {
            snprintf(msg, sizeof(msg), "Ch%u: Offset must be -1000~1000!", c->channel_id);
            GUI_Toast(msg, 2500);
            return;
        }

        /* Vibration-specific checks */
        if (c->ch_type == CH_TYPE_VIBRATION) {
            /* Sensitivity: 0.1~100 */
            if (c->params.vibration.sensitivity < 0.1f ||
                c->params.vibration.sensitivity > 100.0f) {
                snprintf(msg, sizeof(msg), "Ch%u: Sensitivity must be 0.1~100!", c->channel_id);
                GUI_Toast(msg, 2500);
                return;
            }
            /* Suggested value: 0.1~100 */
            if (c->params.vibration.suggested_value < 0.1f ||
                c->params.vibration.suggested_value > 100.0f) {
                snprintf(msg, sizeof(msg), "Ch%u: Suggest val must be 0.1~100!", c->channel_id);
                GUI_Toast(msg, 2500);
                return;
            }
            /* Alarm value must > suggested value */
            if (c->params.vibration.alarm_value <= c->params.vibration.suggested_value) {
                snprintf(msg, sizeof(msg), "Ch%u: Alarm val must > suggest val!", c->channel_id);
                GUI_Toast(msg, 2500);
                return;
            }
        }

        /* General-specific checks */
        if (c->ch_type == CH_TYPE_GENERAL) {
            if (c->params.general.alarm_threshold < 0.0f) {
                snprintf(msg, sizeof(msg), "Ch%u: Threshold must be >= 0!", c->channel_id);
                GUI_Toast(msg, 2500);
                return;
            }
        }
    }

    /* Save to JSON file via shared data module */
    if (ChannelData_Save()) {
        AcqPipeline_RebuildRoute();
        GUI_Toast("Configuration saved successfully!", 2000);
    } else {
        GUI_Toast("Error saving configuration!", 2000);
    }
}

static void ch_btn_reset_cb(lv_event_t *e)
{
    (void)e;

    /* 只重置当前选中通道为当前类型的默认值 */
    if (g_selected_row < 0 || g_selected_row >= g_ch_count) {
        GUI_Toast("Select a channel first!", 1500);
        return;
    }

    uint16_t idx = (uint16_t)g_selected_row;
    ChannelConfig_t *c = &g_ch_configs[idx];

    /* 保留 channel_id, channel_num, enabled 不变 */
    c->unit_index = 0;
    c->coefficient = 1.0f;
    c->offset = 0.0f;

    if (c->ch_type == CH_TYPE_GENERAL) {
        c->sample_freq_hz = 30;
        c->params.general.alarm_threshold = 50.0f;
        c->params.general.alarm_delay_ms  = 1000;
    } else {
        c->sample_freq_hz = 1000;
        c->params.vibration.sensitivity     = 20.0f;
        c->params.vibration.suggested_value = 0.50f;
        c->params.vibration.alarm_value     = 1.00f;
    }

    ch_refresh_table_display();
    ch_update_right_panel();
    GUI_Toast("Channel reset to defaults", 1500);
}

/* ==================== Table refresh (update display from data) ==================== */

static void ch_refresh_table_display(void)
{
    if (g_ch_table == NULL) return;

    for (uint16_t i = 0; i < CH_TABLE_ROWS; i++) {
        uint16_t row = i + 1;  /* +1 to skip header row */
        ChannelConfig_t *cfg = &g_ch_configs[i];
        char buf[32];

        /* Col 0: Channel number */
        lv_snprintf(buf, sizeof(buf), "%u", cfg->channel_num);
        lv_table_set_cell_value(g_ch_table, row, COL_CH_NUM, buf);

        /* Col 1: Enable (checkbox style) */
        lv_table_set_cell_value(g_ch_table, row, COL_ENABLE,
                                cfg->enabled ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE);

        /* Col 2: Name */
        lv_table_set_cell_value(g_ch_table, row, COL_NAME, cfg->channel_name);

        /* Col 3: Type */
        lv_table_set_cell_value(g_ch_table, row, COL_TYPE,
                                cfg->ch_type == CH_TYPE_GENERAL ? "General" : "Vibration");

        /* Col 4: Unit (from unit_index in shared data) */
        lv_table_set_cell_value(g_ch_table, row, COL_UNIT,
                                ChannelData_GetUnit(cfg));

        /* Col 5: Frequency */
        lv_snprintf(buf, sizeof(buf), "%u", cfg->sample_freq_hz);
        lv_table_set_cell_value(g_ch_table, row, COL_FREQ, buf);

        /* Col 6: Coefficient */
        GUI_FormatFloat(buf, sizeof(buf), cfg->coefficient, 3);
        lv_table_set_cell_value(g_ch_table, row, COL_COEFF, buf);

        /* Col 7: Offset */
        GUI_FormatFloat(buf, sizeof(buf), cfg->offset, 3);
        lv_table_set_cell_value(g_ch_table, row, COL_OFFSET, buf);

        /* Col 8: Sensitivity (only for vibration) */
        if (cfg->ch_type == CH_TYPE_VIBRATION) {
            GUI_FormatFloat(buf, sizeof(buf), cfg->params.vibration.sensitivity, 1);
            lv_table_set_cell_value(g_ch_table, row, COL_SENSITIVITY, buf);
        } else {
            lv_table_set_cell_value(g_ch_table, row, COL_SENSITIVITY, "-");
        }

    }
}

/* ==================== Load configs from flash ==================== */

static void ch_load_configs(void)
{
    /* 从共享数据模块获取通道配置 */
    g_ch_configs = ChannelData_GetAll();
    g_ch_count   = ChannelData_GetCount();
    if (g_ch_count == 0 || g_ch_count > CH_TABLE_ROWS) {
        ChannelData_ResetDefaults();
        g_ch_configs = ChannelData_GetAll();
        g_ch_count   = ChannelData_GetCount();
    }
}

/* ==================== Public create function ==================== */

void gui_channel_setup_create(lv_obj_t *parent)
{
    /* Remove tab page default padding so header is flush with edges */
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_pad_gap(parent, 0, 0);

    /* ===== Header bar (full-width gray background, flush top-left) ===== */
    lv_obj_t *header_bar = lv_obj_create(parent);
    lv_obj_set_size(header_bar, LCD_WIDTH, 46);
    lv_obj_set_pos(header_bar, 0, 0);
    lv_obj_set_style_bg_color(header_bar, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_bg_opa(header_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header_bar, 0, 0);
    lv_obj_set_style_radius(header_bar, 0, 0);
    lv_obj_set_style_pad_all(header_bar, 0, 0);
    lv_obj_clear_flag(header_bar, LV_OBJ_FLAG_SCROLLABLE);

    /* ----- Title label (left side) ----- */
    lv_obj_t *title = lv_label_create(header_bar);
    lv_label_set_text(title, "Channel Configuration");
    lv_obj_set_style_text_color(title, lv_color_hex(0x333333), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);

    /* ----- Save button (blue bg, white text) ----- */
    g_ch_btn_save = lv_btn_create(header_bar);
    lv_obj_set_size(g_ch_btn_save, 70, 28);
    lv_obj_align_to(g_ch_btn_save, title, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    lv_obj_set_style_bg_color(g_ch_btn_save, lv_color_hex(0x007BFF), 0);
    lv_obj_set_style_bg_opa(g_ch_btn_save, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_ch_btn_save, 0, 0);
    lv_obj_set_style_radius(g_ch_btn_save, 3, 0);
    lv_obj_t *lbl_save = lv_label_create(g_ch_btn_save);
    lv_label_set_text(lbl_save, "Save");
    lv_obj_set_style_text_color(lbl_save, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl_save);
    lv_obj_add_event_cb(g_ch_btn_save, ch_btn_save_cb, LV_EVENT_CLICKED, NULL);

    /* ----- Reset button (dark gray bg, white text) ----- */
    g_ch_btn_reset = lv_btn_create(header_bar);
    lv_obj_set_size(g_ch_btn_reset, 70, 28);
    lv_obj_align_to(g_ch_btn_reset, g_ch_btn_save, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    lv_obj_set_style_bg_color(g_ch_btn_reset, lv_color_hex(0x6C757D), 0);
    lv_obj_set_style_bg_opa(g_ch_btn_reset, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_ch_btn_reset, 0, 0);
    lv_obj_set_style_radius(g_ch_btn_reset, 3, 0);
    lv_obj_t *lbl_reset = lv_label_create(g_ch_btn_reset);
    lv_label_set_text(lbl_reset, "Reset");
    lv_obj_set_style_text_color(lbl_reset, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl_reset);
    lv_obj_add_event_cb(g_ch_btn_reset, ch_btn_reset_cb, LV_EVENT_CLICKED, NULL);

    /* ===== Content area below header (left table + right panel) ===== */
    lv_obj_t *content = lv_obj_create(parent);
    lv_obj_set_size(content, LCD_WIDTH, LCD_HEIGHT - 46 - 36);
    lv_obj_set_pos(content, 0, 46);
    lv_obj_set_style_bg_color(content, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 8, 0);
    lv_obj_set_style_pad_gap(content, 0, 0);
    lv_obj_set_style_radius(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);

    /* ----- Left: Channel table (9 columns per PRD) ----- */
    g_ch_table = lv_table_create(content);
    /* Explicit white background for table body, remove border */
    lv_obj_set_style_bg_color(g_ch_table, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_ch_table, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_ch_table, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(g_ch_table, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_ch_table, 0, LV_PART_MAIN);

    /* 9 rows total: row 0 = header, rows 1..8 = data */
    lv_table_set_row_cnt(g_ch_table, CH_TABLE_ROWS + 1);
    lv_table_set_col_cnt(g_ch_table, CH_TABLE_COLS);

    /* Column widths: total must = 584px (800 - 200right - 8*2padding = 584) */
    lv_table_set_col_width(g_ch_table, COL_CH_NUM,      36);   /* was 34 */
    lv_table_set_col_width(g_ch_table, COL_ENABLE,      60);
    lv_table_set_col_width(g_ch_table, COL_NAME,       104);   /* was 100 */
    lv_table_set_col_width(g_ch_table, COL_TYPE,        76);   /* was 72 */
    lv_table_set_col_width(g_ch_table, COL_UNIT,        68);   /* was 65 */
    lv_table_set_col_width(g_ch_table, COL_FREQ,        70);   /* was 65 */
    lv_table_set_col_width(g_ch_table, COL_COEFF,       60);   /* was 58 */
    lv_table_set_col_width(g_ch_table, COL_OFFSET,      60);   /* was 58 */
    lv_table_set_col_width(g_ch_table, COL_SENSITIVITY, 50);   /* was 60 */
    /* Sum: 36+60+104+76+68+70+60+60+50 = 584 */

    lv_obj_set_flex_grow(g_ch_table, 1);   /* Fill all remaining width in left area */
    lv_obj_set_height(g_ch_table, LV_PCT(100));

    /* Cell style: smaller font, white background, no text wrap */
    lv_obj_set_style_text_font(g_ch_table, &lv_font_montserrat_12, LV_PART_ITEMS);
    lv_obj_set_style_text_decor(g_ch_table, LV_TEXT_DECOR_NONE, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(g_ch_table, lv_color_hex(0xFFFFFF), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(g_ch_table, LV_OPA_COVER, LV_PART_ITEMS);

    /* Cell padding: compact horizontal, taller rows */
    lv_obj_set_style_pad_top(g_ch_table, 6, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(g_ch_table, 6, LV_PART_ITEMS);
    lv_obj_set_style_pad_left(g_ch_table, 4, LV_PART_ITEMS);
    lv_obj_set_style_pad_right(g_ch_table, 4, LV_PART_ITEMS);

    /* Header row (row 0): light gray background via draw event */
    for (uint16_t c = 0; c < CH_TABLE_COLS; c++) {
        lv_table_set_cell_value(g_ch_table, 0, c, g_headers[c]);
    }

    /* Load data and populate */
    ch_load_configs();
    ch_refresh_table_display();

    /* Register events */
    lv_obj_add_event_cb(g_ch_table, ch_table_click_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(g_ch_table, ch_table_click_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(g_ch_table, ch_table_draw_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);

    /* ----- Right: Auxiliary parameters panel ----- */
    lv_obj_t *right_panel = lv_obj_create(content);
    lv_obj_set_size(right_panel, 200, LV_PCT(100));
    ch_create_aux_panel(right_panel);
}

void gui_channel_setup_destroy(lv_obj_t *parent)
{
    if (g_popup_kb != NULL) {
        lv_obj_t *kb      = g_popup_kb;
        lv_obj_t *overlay = lv_obj_get_parent(kb);

        g_popup_kb = NULL;
        ch_popup_pre_close();
        if (overlay != NULL) {
            lv_obj_del(overlay);
        }
        ch_popup_post_close();
    } else if (g_popup_group != NULL) {
        ch_popup_pre_close();
        ch_popup_post_close();
    }

    if (parent != NULL) {
        lv_obj_clean(parent);
    }

    g_ch_table           = NULL;
    g_ch_btn_save        = NULL;
    g_ch_btn_reset       = NULL;
    g_right_panel        = NULL;
    g_general_panel      = NULL;
    g_vibration_panel    = NULL;
    g_hint_label         = NULL;
    g_lbl_threshold_val  = NULL;
    g_lbl_delay_val      = NULL;
    g_lbl_sens_val       = NULL;
    g_lbl_suggest_val    = NULL;
    g_lbl_alarm_val      = NULL;
    g_selected_row       = -1;
    g_press_row          = LV_TABLE_CELL_NONE;
    g_press_col          = LV_TABLE_CELL_NONE;
}
