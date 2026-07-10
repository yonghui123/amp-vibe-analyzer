/**
 * LVGL font 模块包装文件
 * 仅包含 lv_font.c 和 lv_font_fmt_txt.c（共享基础）
 */
#define S(x) #x
#define X(y) S(y)
#define P ../../../../Middlewares/Third_Party/LVGL/src/font

#include X(P/lv_font.c)
#include X(P/lv_font_fmt_txt.c)
