/**
 * LVGL draw/sw 子模块包装文件
 * 单独编译避免与 draw/ 的 static 函数冲突
 */
#define S(x) #x
#define X(y) S(y)

#define PS ../../../../Middlewares/Third_Party/LVGL/src/draw/sw

#include X(PS/lv_draw_sw.c)
#include X(PS/lv_draw_sw_arc.c)
#include X(PS/lv_draw_sw_blend.c)
#include X(PS/lv_draw_sw_dither.c)
#include X(PS/lv_draw_sw_gradient.c)
#include X(PS/lv_draw_sw_img.c)
#include X(PS/lv_draw_sw_layer.c)
#include X(PS/lv_draw_sw_letter.c)
#include X(PS/lv_draw_sw_line.c)
#include X(PS/lv_draw_sw_polygon.c)
#include X(PS/lv_draw_sw_rect.c)
#include X(PS/lv_draw_sw_transform.c)
