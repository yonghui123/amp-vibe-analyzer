/**
 * LVGL core 模块包装文件
 * 将 core/ 目录下的所有 .c 文件合并为一个编译单元
 * 用于 Keil 工程批量添加 LVGL 源码
 */

#define S(x) #x
#define X(y) S(y)

#define P ../../../../Middlewares/Third_Party/LVGL/src/core

#include X(P/lv_disp.c)
#include X(P/lv_event.c)
#include X(P/lv_group.c)
#include X(P/lv_indev.c)
#include X(P/lv_indev_scroll.c)
#include X(P/lv_obj.c)
#include X(P/lv_obj_class.c)
#include X(P/lv_obj_draw.c)
#include X(P/lv_obj_pos.c)
#include X(P/lv_obj_scroll.c)
#include X(P/lv_obj_style.c)
#include X(P/lv_obj_style_gen.c)
#include X(P/lv_obj_tree.c)
#include X(P/lv_refr.c)
#include X(P/lv_theme.c)
