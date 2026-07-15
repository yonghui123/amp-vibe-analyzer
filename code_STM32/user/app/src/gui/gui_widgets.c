/**
 * @file    gui_widgets.c
 * @brief   SignalX GUI 通用控件辅助函数
 */

#include "gui_widgets.h"
#include "config.h"
#include <stdint.h>
#include <string.h>

#define LEGEND_BORDER_COLOR   0xCCCCCC
#define LEGEND_TITLE_COLOR    0x333333
#define LEGEND_BG_COLOR       0xFFFFFF
#define LEGEND_BORDER_RADIUS  4
#define LEGEND_TITLE_X        8
#define LEGEND_TITLE_Y        0
#define LEGEND_TITLE_PAD      6
#define LEGEND_CONT_Y_OFS     12
#define LEGEND_FLEX_GAP       2

#define DIALOG_DEFAULT_W      280
#define DIALOG_BTN_H          40
#define DIALOG_PAD            12
#define DIALOG_GAP            8

typedef struct {
    GUI_DialogBtnCb_t btn_cb;
    void             *user_data;
    lv_obj_t         *content;
    lv_obj_t         *title;
} gui_dialog_priv_t;

void GUI_LegendBoxCfgInit(GUI_LegendBoxCfg_t *cfg)
{
    cfg->box_h            = LV_SIZE_CONTENT;
    cfg->cont_h           = LV_SIZE_CONTENT;
    cfg->cont_y_ofs       = LEGEND_CONT_Y_OFS;
    cfg->inner_pad        = 0;
    cfg->title_color      = LEGEND_TITLE_COLOR;
    cfg->box_transparent  = false;
    cfg->flex_grow        = false;
    cfg->flex_column      = false;
    cfg->flex_gap         = LEGEND_FLEX_GAP;
}

lv_obj_t *GUI_CreateLegendBox(lv_obj_t *parent, const char *title,
                              const GUI_LegendBoxCfg_t *cfg_in)
{
    GUI_LegendBoxCfg_t defaults;
    GUI_LegendBoxCfgInit(&defaults);
    const GUI_LegendBoxCfg_t *cfg = cfg_in ? cfg_in : &defaults;

    lv_coord_t box_h  = cfg->box_h  ? cfg->box_h  : LV_SIZE_CONTENT;
    lv_coord_t cont_h = cfg->cont_h ? cfg->cont_h : LV_SIZE_CONTENT;
    uint32_t title_color = cfg->title_color ? cfg->title_color : LEGEND_TITLE_COLOR;

    /* 外层 box */
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_width(box, LV_PCT(100));
    lv_obj_set_height(box, box_h);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_radius(box, 0, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    if (cfg->box_transparent) {
        lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    } else {
        lv_obj_set_style_bg_color(box, lv_color_hex(LEGEND_BG_COLOR), 0);
        lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    }

    if (cfg->flex_grow) {
        lv_obj_set_flex_grow(box, 1);
    }

    /* 内层 cont */
    lv_obj_t *cont = lv_obj_create(box);
    lv_obj_set_width(cont, LV_PCT(100));
    lv_obj_set_height(cont, cont_h);
    lv_obj_set_pos(cont, 0, cfg->cont_y_ofs);
    lv_obj_set_style_bg_color(cont, lv_color_hex(LEGEND_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cont, 1, 0);
    lv_obj_set_style_border_color(cont, lv_color_hex(LEGEND_BORDER_COLOR), 0);
    lv_obj_set_style_radius(cont, LEGEND_BORDER_RADIUS, 0);
    lv_obj_set_style_pad_all(cont, cfg->inner_pad, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    if (cfg->flex_column) {
        lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_gap(cont, cfg->flex_gap, 0);
    }

    /* 标题 label（在 cont 之后创建，z 序在上） */
    lv_obj_t *lbl = lv_label_create(box);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(title_color), 0);
    lv_obj_set_style_bg_color(lbl, lv_color_hex(LEGEND_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(lbl, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(lbl, 0, 0);
    lv_obj_set_style_pad_all(lbl, LEGEND_TITLE_PAD, 0);
    lv_obj_set_pos(lbl, LEGEND_TITLE_X, LEGEND_TITLE_Y);

    return cont;
}

void GUI_FormatFloat(char *buf, size_t len, float val, uint8_t decimals)
{
    static const int32_t scale[] = {1, 10, 100, 1000};
    int32_t mul;
    int32_t scaled;
    int32_t abs_s;
    int32_t whole;
    int32_t frac;

    if (buf == NULL || len == 0) {
        return;
    }
    if (decimals > 3U) {
        decimals = 3U;
    }

    mul = scale[decimals];
    scaled = (int32_t)(val * (float)mul + (val >= 0.0f ? 0.5f : -0.5f));
    abs_s = scaled < 0 ? -scaled : scaled;
    whole = abs_s / mul;
    frac = abs_s % mul;

    if (decimals == 0U) {
        lv_snprintf(buf, len, "%s%d", scaled < 0 ? "-" : "", (int)whole);
    } else if (decimals == 1U) {
        lv_snprintf(buf, len, "%s%d.%01d", scaled < 0 ? "-" : "", (int)whole, (int)frac);
    } else if (decimals == 2U) {
        lv_snprintf(buf, len, "%s%d.%02d", scaled < 0 ? "-" : "", (int)whole, (int)frac);
    } else {
        lv_snprintf(buf, len, "%s%d.%03d", scaled < 0 ? "-" : "", (int)whole, (int)frac);
    }
}

/* ==================== 简易模态弹窗 ==================== */

static void gui_dialog_btn_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    uint16_t btn_idx = (uint16_t)(uintptr_t)lv_event_get_user_data(e);

    /* btn → btn_row → panel */
    lv_obj_t *btn_row = lv_obj_get_parent(btn);
    lv_obj_t *panel = btn_row ? lv_obj_get_parent(btn_row) : NULL;
    if (panel == NULL) {
        return;
    }

    gui_dialog_priv_t *priv = (gui_dialog_priv_t *)lv_obj_get_user_data(panel);
    if (priv != NULL && priv->btn_cb != NULL) {
        priv->btn_cb(panel, btn_idx, priv->user_data);
    } else {
        GUI_DialogClose(panel);
    }
}

lv_obj_t *GUI_DialogCreate(const char *title, const char *message,
                           const char * const *btn_labels,
                           GUI_DialogBtnCb_t btn_cb, void *user_data)
{
    gui_dialog_priv_t *priv = (gui_dialog_priv_t *)lv_mem_alloc(sizeof(gui_dialog_priv_t));
    if (priv == NULL) {
        return NULL;
    }
    memset(priv, 0, sizeof(*priv));
    priv->btn_cb = btn_cb;
    priv->user_data = user_data;

    /* 全屏遮罩：挂在当前 screen，不走 lv_layer_top（板端 msgbox 兼容问题） */
    lv_obj_t *overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(overlay, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_30, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *panel = lv_obj_create(overlay);
    lv_obj_set_size(panel, DIALOG_DEFAULT_W, LV_SIZE_CONTENT);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_radius(panel, 6, 0);
    lv_obj_set_style_shadow_width(panel, 8, 0);
    lv_obj_set_style_shadow_opa(panel, LV_OPA_30, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(panel, DIALOG_PAD, 0);
    lv_obj_set_style_pad_gap(panel, DIALOG_GAP, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(panel, priv);

    if (title != NULL && title[0] != '\0') {
        priv->title = lv_label_create(panel);
        lv_label_set_text(priv->title, title);
        lv_obj_set_style_text_font(priv->title, &lv_font_montserrat_14, 0);
        lv_obj_set_width(priv->title, LV_PCT(100));
    }

    priv->content = lv_obj_create(panel);
    lv_obj_set_width(priv->content, LV_PCT(100));
    lv_obj_set_height(priv->content, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(priv->content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(priv->content, 0, 0);
    lv_obj_set_style_pad_all(priv->content, 0, 0);
    lv_obj_set_flex_flow(priv->content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(priv->content, 6, 0);
    lv_obj_clear_flag(priv->content, LV_OBJ_FLAG_SCROLLABLE);

    if (message != NULL && message[0] != '\0') {
        lv_obj_t *msg = lv_label_create(priv->content);
        lv_label_set_text(msg, message);
        lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(msg, LV_PCT(100));
        lv_obj_set_style_text_font(msg, &lv_font_montserrat_14, 0);
    }

    /* 按钮行 */
    uint16_t btn_cnt = 0;
    if (btn_labels != NULL) {
        while (btn_labels[btn_cnt] != NULL && btn_labels[btn_cnt][0] != '\0') {
            btn_cnt++;
        }
    }

    if (btn_cnt > 0) {
        lv_obj_t *btn_row = lv_obj_create(panel);
        lv_obj_set_width(btn_row, LV_PCT(100));
        lv_obj_set_height(btn_row, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(btn_row, 0, 0);
        lv_obj_set_style_pad_all(btn_row, 0, 0);
        lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_gap(btn_row, 8, 0);
        lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

        for (uint16_t i = 0; i < btn_cnt; i++) {
            lv_obj_t *btn = lv_btn_create(btn_row);
            lv_obj_set_height(btn, DIALOG_BTN_H);
            if (btn_cnt == 1) {
                lv_obj_set_width(btn, LV_PCT(100));
            } else {
                lv_obj_set_flex_grow(btn, 1);
            }
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x007BFF), 0);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(btn, 4, 0);
            lv_obj_set_ext_click_area(btn, 12);

            lv_obj_t *lbl = lv_label_create(btn);
            lv_label_set_text(lbl, btn_labels[i]);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
            lv_obj_center(lbl);

            lv_obj_add_event_cb(btn, gui_dialog_btn_event_cb, LV_EVENT_CLICKED,
                                (void *)(uintptr_t)i);
        }
    }

    return panel;
}

lv_obj_t *GUI_DialogGetContent(lv_obj_t *dialog)
{
    if (dialog == NULL) {
        return NULL;
    }
    gui_dialog_priv_t *priv = (gui_dialog_priv_t *)lv_obj_get_user_data(dialog);
    return priv ? priv->content : NULL;
}

lv_obj_t *GUI_DialogGetTitle(lv_obj_t *dialog)
{
    if (dialog == NULL) {
        return NULL;
    }
    gui_dialog_priv_t *priv = (gui_dialog_priv_t *)lv_obj_get_user_data(dialog);
    return priv ? priv->title : NULL;
}

void GUI_DialogSetWidth(lv_obj_t *dialog, lv_coord_t width)
{
    if (dialog != NULL) {
        lv_obj_set_width(dialog, width);
    }
}

void GUI_DialogClose(lv_obj_t *dialog)
{
    if (dialog == NULL) {
        return;
    }

    gui_dialog_priv_t *priv = (gui_dialog_priv_t *)lv_obj_get_user_data(dialog);
    lv_obj_t *overlay = lv_obj_get_parent(dialog);

    if (priv != NULL) {
        lv_obj_set_user_data(dialog, NULL);
        lv_mem_free(priv);
    }
    if (overlay != NULL) {
        lv_obj_del(overlay);
    }
}
