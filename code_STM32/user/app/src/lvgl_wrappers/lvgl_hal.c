/**
 * LVGL hal 模块包装文件
 */
#define S(x) #x
#define X(y) S(y)
#define P ../../../../Middlewares/Third_Party/LVGL/src/hal

#include X(P/lv_hal_disp.c)
#include X(P/lv_hal_indev.c)
#include X(P/lv_hal_tick.c)
