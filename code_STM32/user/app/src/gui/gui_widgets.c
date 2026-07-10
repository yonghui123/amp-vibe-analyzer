/**
 * @file    gui_widgets.c
 * @brief   SignalX GUI 通用控件辅助函数
 */

#include "gui_widgets.h"

#define LEGEND_BORDER_COLOR   0xCCCCCC
#define LEGEND_TITLE_COLOR    0x333333
#define LEGEND_BG_COLOR       0xFFFFFF
#define LEGEND_BORDER_RADIUS  4
#define LEGEND_TITLE_X        8
#define LEGEND_TITLE_Y        0
#define LEGEND_TITLE_PAD      6
#define LEGEND_CONT_Y_OFS     12
#define LEGEND_FLEX_GAP       2

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
