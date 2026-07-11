/**
 * @file main.c
 * LVGL v8.4 PC 模拟器 - SDL2 手动实现显示/输入驱动
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "lvgl.h"
#include <SDL.h>
#include "gui_core.h"
#include "gui_calib.h"
#include "config.h"
#include "acq_task.h"
#include "storage/sd_fs.h"

/* 显示分辨率 */
#define DISP_HOR_RES 800
#define DISP_VER_RES 480

/* SDL 全局变量 */
static SDL_Window *sdl_window = NULL;
static SDL_Renderer *sdl_renderer = NULL;
static SDL_Texture *sdl_texture = NULL;

/* 鼠标状态 */
static bool mouse_pressed = false;
static int16_t mouse_x = 0;
static int16_t mouse_y = 0;

/* 键盘状态 */
static uint32_t last_key = 0;
static lv_indev_state_t key_state = LV_INDEV_STATE_REL;

/* LVGL flush 回调 - 将像素数据写入 SDL 纹理 */
static void sdl_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    /* 更新纹理的指定区域 */
    SDL_Rect rect = {
        .x = area->x1,
        .y = area->y1,
        .w = area->x2 - area->x1 + 1,
        .h = area->y2 - area->y1 + 1
    };

    SDL_UpdateTexture(sdl_texture, &rect, color_p, rect.w * sizeof(lv_color_t));

    /* 通知 LVGL flush 完成 */
    lv_disp_flush_ready(disp_drv);
}

/* LVGL 鼠标输入回调 */
static void sdl_mouse_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    (void)indev_drv;
    data->point.x = mouse_x;
    data->point.y = mouse_y;
    data->state = mouse_pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
}

/* LVGL 键盘输入回调 */
static void sdl_keyboard_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    (void)indev_drv;
    data->state = key_state;
    data->key = last_key;
    if (key_state == LV_INDEV_STATE_PR) {
        /* Release after one read so LVGL processes the keypress */
        key_state = LV_INDEV_STATE_REL;
    }
}

/* 初始化 SDL2 显示 */
static void sdl_init(void)
{
    SDL_Init(SDL_INIT_VIDEO);

    sdl_window = SDL_CreateWindow(
        "LVGL PC Simulator (800x480)",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        DISP_HOR_RES,
        DISP_VER_RES,
        SDL_WINDOW_SHOWN
    );

    sdl_renderer = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_ACCELERATED);
    sdl_texture = SDL_CreateTexture(
        sdl_renderer,
        SDL_PIXELFORMAT_RGB565,
        SDL_TEXTUREACCESS_STREAMING,
        DISP_HOR_RES,
        DISP_VER_RES
    );
}

/* SDL 按键映射到 LVGL 按键码 */
static uint32_t sdl_key_to_lvgl(SDL_Keycode key)
{
    switch (key) {
        case SDLK_BACKSPACE:  return LV_KEY_BACKSPACE;
        case SDLK_RETURN:     return LV_KEY_ENTER;
        case SDLK_KP_ENTER:   return LV_KEY_ENTER;
        case SDLK_ESCAPE:     return LV_KEY_ESC;
        case SDLK_DELETE:     return LV_KEY_DEL;
        case SDLK_TAB:        return LV_KEY_NEXT;
        case SDLK_LEFT:       return LV_KEY_LEFT;
        case SDLK_RIGHT:      return LV_KEY_RIGHT;
        case SDLK_UP:         return LV_KEY_UP;
        case SDLK_DOWN:       return LV_KEY_DOWN;
        case SDLK_HOME:       return LV_KEY_HOME;
        case SDLK_END:        return LV_KEY_END;
        default:              return 0;
    }
}

/* 处理 SDL 事件 */
static void sdl_event_handler(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                SDL_DestroyTexture(sdl_texture);
                SDL_DestroyRenderer(sdl_renderer);
                SDL_DestroyWindow(sdl_window);
                SDL_Quit();
                exit(0);
                break;
            case SDL_MOUSEBUTTONDOWN:
                mouse_pressed = true;
                mouse_x = event.button.x;
                mouse_y = event.button.y;
                break;
            case SDL_MOUSEBUTTONUP:
                mouse_pressed = false;
                mouse_x = event.button.x;
                mouse_y = event.button.y;
                break;
            case SDL_MOUSEMOTION:
                mouse_x = event.motion.x;
                mouse_y = event.motion.y;
                break;
            case SDL_KEYDOWN: {
                /* 特殊键映射 */
                uint32_t lv_key = sdl_key_to_lvgl(event.key.keysym.sym);
                if (lv_key != 0) {
                    last_key = lv_key;
                    key_state = LV_INDEV_STATE_PR;
                }
                break;
            }
            case SDL_TEXTINPUT: {
                /* 普通字符输入 - 逐字符发送给 LVGL */
                const char *text = event.text.text;
                if (text && text[0]) {
                    last_key = (uint32_t)text[0];
                    key_state = LV_INDEV_STATE_PR;
                }
                break;
            }
        }
    }
}

/* 渲染 SDL 纹理到屏幕 */
static void sdl_render(void)
{
    SDL_RenderClear(sdl_renderer);
    SDL_RenderCopy(sdl_renderer, sdl_texture, NULL, NULL);
    SDL_RenderPresent(sdl_renderer);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("Starting LVGL PC Simulator...\n");

    /* 初始化 LVGL */
    lv_init();

    /* 初始化 SDL2 */
    sdl_init();

    /* 创建显示缓冲区 */
    static lv_disp_draw_buf_t disp_buf;
    static lv_color_t buf[DISP_HOR_RES * 50];  /* 50 行缓冲 */
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, DISP_HOR_RES * 50);

    /* 注册显示驱动 */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = sdl_flush_cb;
    disp_drv.hor_res = DISP_HOR_RES;
    disp_drv.ver_res = DISP_VER_RES;
    lv_disp_drv_register(&disp_drv);

    /* 注册鼠标输入驱动 */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = sdl_mouse_read_cb;
    lv_indev_drv_register(&indev_drv);

    /* 注册键盘输入驱动 */
    static lv_indev_drv_t kb_drv;
    lv_indev_drv_init(&kb_drv);
    kb_drv.type = LV_INDEV_TYPE_KEYPAD;
    kb_drv.read_cb = sdl_keyboard_read_cb;
    lv_indev_t *kb_indev = lv_indev_drv_register(&kb_drv);

    /* 键盘驱动关联到默认分组, 否则键盘事件无法路由到 textarea 等控件 */
    lv_group_t *def_group = lv_group_get_default();
    if (def_group == NULL) {
        def_group = lv_group_create();
        lv_group_set_default(def_group);
    }
    lv_indev_set_group(kb_indev, def_group);

    /* 设置默认主题 */
    lv_theme_t *th = lv_theme_default_init(
        lv_disp_get_default(),
        lv_palette_main(LV_PALETTE_BLUE),
        lv_palette_main(LV_PALETTE_RED),
        true,    /* 暗色主题 */
        &lv_font_montserrat_14
    );
    lv_disp_set_theme(lv_disp_get_default(), th);

    /* ===== SD 工具层（PC：./sd_root） ===== */
    if (!SdFs_Init()) {
        printf("SdFs_Init failed (err=%d), storage APIs unavailable\n",
               SdFs_LastError());
    }

    /* ===== 启动 GUI 界面 ===== */
#if GUI_TOUCH_CALIB_MODE
    GUI_CalibInit();
#else
    GUI_Init();
#endif

    printf("LVGL PC Simulator started!\n");
    printf("Window: %dx%d\n", DISP_HOR_RES, DISP_VER_RES);
    printf("Close the window to exit.\n");

    /* 主循环 */
    uint32_t last_tick = SDL_GetTicks();
    while (1) {
        /* 处理 SDL 事件 */
        sdl_event_handler();

        /* 更新 LVGL tick */
        uint32_t current_tick = SDL_GetTicks();
        lv_tick_inc(current_tick - last_tick);
        last_tick = current_tick;

        /* LVGL 处理 */
        lv_timer_handler();

        /* 采集任务轮询（无 FreeRTOS，20ms 节流在 AcqTask_Poll 内） */
        AcqTask_Poll();

        /* 渲染到 SDL 窗口 */
        sdl_render();

        SDL_Delay(5);
    }

    return 0;
}
