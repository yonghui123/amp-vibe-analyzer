/**
 * LVGL widgets 模块包装文件（基础控件，不含 draw_main 冲突文件）
 */
#define S(x) #x
#define X(y) S(y)
#define P ../../../../Middlewares/Third_Party/LVGL/src/widgets

#include X(P/lv_arc.c)
#include X(P/lv_bar.c)
#include X(P/lv_btn.c)
#include X(P/lv_canvas.c)
#include X(P/lv_checkbox.c)
#include X(P/lv_img.c)
#include X(P/lv_line.c)
#include X(P/lv_objx_templ.c)
#include X(P/lv_slider.c)
#include X(P/lv_textarea.c)
