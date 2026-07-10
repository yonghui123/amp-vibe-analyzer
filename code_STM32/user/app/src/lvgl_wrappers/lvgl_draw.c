/**
 * LVGL draw 模块包装文件（不含 sw 子目录）
 */
#define S(x) #x
#define X(y) S(y)

#define P ../../../../Middlewares/Third_Party/LVGL/src/draw

#include X(P/lv_draw.c)
#include X(P/lv_draw_arc.c)
#include X(P/lv_draw_img.c)
#include X(P/lv_draw_label.c)
#include X(P/lv_draw_layer.c)
#include X(P/lv_draw_line.c)
#include X(P/lv_draw_mask.c)
#include X(P/lv_draw_rect.c)
#include X(P/lv_draw_transform.c)
#include X(P/lv_draw_triangle.c)
#include X(P/lv_img_buf.c)
#include X(P/lv_img_cache.c)
#include X(P/lv_img_decoder.c)
