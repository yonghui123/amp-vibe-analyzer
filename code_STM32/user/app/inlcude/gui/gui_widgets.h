/**
 * @file    gui_widgets.h
 * @brief   SignalX GUI 通用控件辅助函数
 */

#ifndef __GUI_WIDGETS_H
#define __GUI_WIDGETS_H

#include "lvgl.h"

/**
 * @brief  "标题压在边框左上角" 容器配置
 *
 * 未显式设置的字段由 GUI_LegendBoxCfgInit() 填充为默认值
 * （对齐 screen_main_display 标准实现）。
 */
typedef struct {
    lv_coord_t box_h;         /**< 外层高度，0 = LV_SIZE_CONTENT */
    lv_coord_t cont_h;        /**< 内层高度，0 = LV_SIZE_CONTENT */
    lv_coord_t cont_y_ofs;    /**< 内层向下偏移，默认 12 */
    lv_coord_t inner_pad;     /**< 内层 padding，默认 0 */
    uint32_t   title_color;   /**< 标题颜色，0 = 0x333333 */
    bool       box_transparent; /**< true: 外层透明背景 */
    bool       flex_grow;     /**< 外层 flex_grow */
    bool       flex_column;   /**< 内层纵向 flex 布局 */
    lv_coord_t flex_gap;      /**< flex 行间距，默认 2 */
} GUI_LegendBoxCfg_t;

/**
 * @brief  填充 LegendBox 默认配置
 */
void GUI_LegendBoxCfgInit(GUI_LegendBoxCfg_t *cfg);

/**
 * @brief  创建"标题压在边框左上角"的 fieldset 风格容器
 *
 * 结构：
 *   box  (外层, 无边框)
 *   ├── cont  (白底 + 灰色圆角边框)
 *   └── lbl   (标题, 白底遮住顶边, z 序在 cont 之上)
 *
 * @param  parent  父对象
 * @param  title   标题文字
 * @param  cfg     配置，传 NULL 使用全部默认值
 * @return 内层 cont，调用方直接向其中添加子控件
 */
lv_obj_t *GUI_CreateLegendBox(lv_obj_t *parent, const char *title,
                              const GUI_LegendBoxCfg_t *cfg);

/**
 * @brief  定点格式化浮点数（兼容 newlib-nano，无 %f 支持）
 * @param  decimals  小数位数 0~3
 */
void GUI_FormatFloat(char *buf, size_t len, float val, uint8_t decimals);

/* ==================== 简易模态弹窗（替代 lv_msgbox） ==================== */

/**
 * @brief  弹窗按钮回调
 * @param  dialog   GUI_DialogCreate 返回的面板对象
 * @param  btn_idx  按钮索引（从 0 开始）
 * @param  user_data  创建时传入的用户数据
 *
 * 回调内可调用 GUI_DialogClose(dialog) 关闭；
 * 若创建时 btn_cb 为 NULL，则点击任意按钮自动关闭。
 */
typedef void (*GUI_DialogBtnCb_t)(lv_obj_t *dialog, uint16_t btn_idx, void *user_data);

/**
 * @brief  创建模态弹窗（遮罩挂在 lv_scr_act，不用 lv_layer_top / lv_msgbox）
 *
 * 结构：
 *   overlay (全屏半透明，拦截背景触摸)
 *   └── panel (白底圆角面板)  ← 返回值
 *       ├── title
 *       ├── content（可放自定义控件；有 message 时内含文本）
 *       └── 按钮行
 *
 * @param  title       标题，可为 NULL
 * @param  message     正文，可为 NULL/空串（仅要自定义 content 时传 NULL）
 * @param  btn_labels  按钮文案数组，以 "" 结尾（同 lv_msgbox 约定）
 * @param  btn_cb      按钮回调，NULL = 点击后自动关闭
 * @param  user_data   传给回调的用户数据
 * @return panel；失败返回 NULL。关闭请用 GUI_DialogClose()
 */
lv_obj_t *GUI_DialogCreate(const char *title, const char *message,
                           const char * const *btn_labels,
                           GUI_DialogBtnCb_t btn_cb, void *user_data);

/** 获取 content 容器，用于向弹窗内添加下拉框等自定义控件 */
lv_obj_t *GUI_DialogGetContent(lv_obj_t *dialog);

/** 获取标题 label，便于改颜色等样式 */
lv_obj_t *GUI_DialogGetTitle(lv_obj_t *dialog);

/** 设置弹窗面板宽度（默认 280） */
void GUI_DialogSetWidth(lv_obj_t *dialog, lv_coord_t width);

/** 关闭并销毁弹窗（删除全屏 overlay） */
void GUI_DialogClose(lv_obj_t *dialog);

#endif /* __GUI_WIDGETS_H */
