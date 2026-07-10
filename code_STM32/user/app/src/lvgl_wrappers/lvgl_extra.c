/**
 * LVGL extra 模块包装文件
 * 包含: extra核心 + layouts + themes/default + extra/widgets
 * 注意: lv_calendar_header_dropdown.c 单独编译避免冲突
 */
#define S(x) #x
#define X(y) S(y)

#define PE  ../../../../Middlewares/Third_Party/LVGL/src/extra
#define PLF ../../../../Middlewares/Third_Party/LVGL/src/extra/layouts/flex
#define PLG ../../../../Middlewares/Third_Party/LVGL/src/extra/layouts/grid
#define PTD ../../../../Middlewares/Third_Party/LVGL/src/extra/themes/default
#define PW  ../../../../Middlewares/Third_Party/LVGL/src/extra/widgets

/* extra 核心 */
#include X(PE/lv_extra.c)

/* 布局 */
#include X(PLF/lv_flex.c)
#include X(PLG/lv_grid.c)

/* 默认主题 */
#include X(PTD/lv_theme_default.c)

/* 扩展控件 */
#include X(PW/chart/lv_chart.c)
#include X(PW/keyboard/lv_keyboard.c)
#include X(PW/list/lv_list.c)
#include X(PW/msgbox/lv_msgbox.c)
#include X(PW/spinbox/lv_spinbox.c)
#include X(PW/spinner/lv_spinner.c)
#include X(PW/tabview/lv_tabview.c)
#include X(PW/calendar/lv_calendar.c)
#include X(PW/calendar/lv_calendar_header_arrow.c)
#include X(PW/meter/lv_meter.c)
#include X(PW/led/lv_led.c)
#include X(PW/span/lv_span.c)
#include X(PW/colorwheel/lv_colorwheel.c)
#include X(PW/menu/lv_menu.c)
#include X(PW/tileview/lv_tileview.c)
#include X(PW/win/lv_win.c)
