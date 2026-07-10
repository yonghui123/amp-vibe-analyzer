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

#endif /* __GUI_WIDGETS_H */
