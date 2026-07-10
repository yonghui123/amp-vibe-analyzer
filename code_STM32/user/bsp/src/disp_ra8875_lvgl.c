/**
 * @file disp_ra8875_lvgl.c
 * RA8875 显示/触摸 LVGL 驱动适配层
 */

#include "disp_ra8875_lvgl.h"
#include "touch_calib.h"
#include "touch_bsp_touch.h"
#include "config.h"
#include "bsp_lcd_ra8875.h"
#include "bsp_ra8875.h"
#include "main.h"
#include "stm32f4xx_hal.h"

#define SCREEN_WIDTH    800
#define SCREEN_HEIGHT   480
/* 条带高度 ≈ DISP_BUF_SIZE / WIDTH；/20 → 24 行/条 */
#define DISP_BUF_SIZE   (SCREEN_WIDTH * SCREEN_HEIGHT / 20)

#define TOUCH_RAW_MIN           0
#define TOUCH_RAW_MAX           1023

static lv_disp_draw_buf_t s_disp_buf;
static lv_color_t s_buf1[DISP_BUF_SIZE];
#if LV_MEM_CUSTOM
static lv_color_t s_buf2[DISP_BUF_SIZE];
#endif

static lv_disp_drv_t s_disp_drv;
static lv_indev_drv_t s_indev_drv;

#if !defined(PC_SIMULATOR)
static uint16_t s_last_adc_x;
static uint16_t s_last_adc_y;
#endif

static int32_t s_touch_x;
static int32_t s_touch_y;
static int32_t s_touch_report_x;
static int32_t s_touch_report_y;
static lv_indev_state_t s_touch_state = LV_INDEV_STATE_RELEASED;
static bool s_touch_filter_armed = false;
static bool s_touch_has_valid = false;

static bool s_calib_collect_mode = false;
static bool s_calib_was_pressed = false;
static uint32_t s_calib_sum_x;
static uint32_t s_calib_sum_y;
static uint16_t s_calib_count;
static bool s_calib_sample_ready = false;
/** 无有效 ADC 时上报的安全坐标（内容区中部，不在 Tab 栏） */
#define TOUCH_SAFE_X            0
#define TOUCH_SAFE_Y            (SCREEN_HEIGHT / 2)
static uint16_t s_calib_result_x;
static uint16_t s_calib_result_y;

static void touch_map_coords(uint16_t adc_x, uint16_t adc_y, int32_t *px, int32_t *py)
{
    if (!TouchCalib_AdcToScreen(adc_x, adc_y, px, py)) {
        if (px != NULL) {
            *px = 0;
        }
        if (py != NULL) {
            *py = 0;
        }
    }
}

static void touch_filter_apply(int32_t raw_x, int32_t raw_y)
{
#if !GUI_TOUCH_LVGL_FILTER
    s_touch_x = raw_x;
    s_touch_y = raw_y;
    s_touch_filter_armed = true;
    return;
#endif
    if (!s_touch_filter_armed || s_touch_state == LV_INDEV_STATE_RELEASED) {
        s_touch_x = raw_x;
        s_touch_y = raw_y;
        s_touch_filter_armed = true;
        return;
    }

    s_touch_x = (s_touch_x * 3 + raw_x) / 4;
    s_touch_y = (s_touch_y * 3 + raw_y) / 4;
}

static void touch_calib_reset_accumulator(void)
{
    s_calib_sum_x = 0U;
    s_calib_sum_y = 0U;
    s_calib_count = 0U;
    s_calib_sample_ready = false;
}

static void touch_calib_accumulate(uint16_t adc_x, uint16_t adc_y)
{
    s_calib_sum_x += adc_x;
    s_calib_sum_y += adc_y;
    s_calib_count++;
}

static void touch_calib_finalize_sample(void)
{
    if (s_calib_count < 3U) {
        s_calib_sum_x = 0U;
        s_calib_sum_y = 0U;
        s_calib_count = 0U;
        return;
    }

    s_calib_result_x = (uint16_t)(s_calib_sum_x / s_calib_count);
    s_calib_result_y = (uint16_t)(s_calib_sum_y / s_calib_count);
    s_calib_sum_x = 0U;
    s_calib_sum_y = 0U;
    s_calib_count = 0U;
    s_calib_sample_ready = true;
}

/**
 * @brief  读取 RA8875 触摸并更新 s_touch_x/y/state
 * @note   使用 TouchBsp_TryReadAdc（busy 重试），避免 LVGL 刷屏时坐标卡在 (0,0)
 */
static void touch_poll_hardware(void)
{
    uint16_t adc_x = 0;
    uint16_t adc_y = 0;
    int32_t pixel_x = 0;
    int32_t pixel_y = 0;

    if (!TouchBsp_PenDown()) {
        if (s_calib_collect_mode && s_calib_was_pressed) {
            touch_calib_finalize_sample();
        }
        s_calib_was_pressed = false;
        s_touch_state = LV_INDEV_STATE_RELEASED;
        s_touch_filter_armed = false;
        s_touch_has_valid = false;
        s_touch_x = 0;
        s_touch_y = 0;
        s_touch_report_x = 0;
        s_touch_report_y = 0;
        return;
    }

    if (!TouchBsp_TryReadAdc(&adc_x, &adc_y)) {
        /* 手指仍按下：保持 PRESSED，并继续上报上次有效坐标 */
        s_touch_state = LV_INDEV_STATE_PRESSED;
        if (s_touch_filter_armed) {
            s_touch_has_valid = true;
        }
        return;
    }

#if !defined(PC_SIMULATOR)
    s_last_adc_x = adc_x;
    s_last_adc_y = adc_y;
#endif

    if (s_calib_collect_mode) {
        if (!s_calib_was_pressed) {
            touch_calib_reset_accumulator();
        }
        touch_calib_accumulate(adc_x, adc_y);
        s_calib_was_pressed = true;
        s_touch_state = LV_INDEV_STATE_PRESSED;
        return;
    }

    touch_map_coords(adc_x, adc_y, &pixel_x, &pixel_y);
    touch_filter_apply(pixel_x, pixel_y);
    s_touch_has_valid = true;
    s_touch_state = LV_INDEV_STATE_PRESSED;
    s_touch_report_x = s_touch_x;
    s_touch_report_y = s_touch_y;
}

static void ra8875_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint16_t w = (uint16_t)(area->x2 - area->x1 + 1);
    uint16_t h = (uint16_t)(area->y2 - area->y1 + 1);
    uint32_t count = (uint32_t)w * h;
    lv_color_t *p = color_p;

    RA8875_StartDirectDraw((uint16_t)area->x1, (uint16_t)area->y1, h, w);
    for (uint32_t i = 0; i < count; i++) {
        RA8875_WriteData16(p->full);
        p++;
    }
    RA8875_QuitDirectDraw();

    lv_disp_flush_ready(drv);
}

static void ra8875_touch_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    (void)indev_drv;

    touch_poll_hardware();

    data->state = s_touch_state;
    if (s_touch_has_valid) {
        data->point.x = s_touch_x;
        data->point.y = s_touch_y;
    } else {
        /*
         * LVGL 在 read_cb 前会用 last_raw_point 预填 data->point。
         * 若不覆盖，校准/误触残留在 (768,32) 等 Tab 栏坐标会在
         * PRESSED 时触发 btnmatrix 切到最后一页。
         */
        data->point.x = TOUCH_SAFE_X;
        data->point.y = TOUCH_SAFE_Y;
    }
}

static void disp_ra8875_init(void)
{
#if LV_MEM_CUSTOM
    lv_disp_draw_buf_init(&s_disp_buf, s_buf1, s_buf2, DISP_BUF_SIZE);
#else
    lv_disp_draw_buf_init(&s_disp_buf, s_buf1, NULL, DISP_BUF_SIZE);
#endif

    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.draw_buf = &s_disp_buf;
    s_disp_drv.flush_cb = ra8875_flush_cb;
    s_disp_drv.hor_res = SCREEN_WIDTH;
    s_disp_drv.ver_res = SCREEN_HEIGHT;
    lv_disp_drv_register(&s_disp_drv);
}

static void touch_ra8875_init(void)
{
    RA8875_TouchInit();

    lv_indev_drv_init(&s_indev_drv);
    s_indev_drv.type = LV_INDEV_TYPE_POINTER;
    s_indev_drv.read_cb = ra8875_touch_read_cb;
    s_indev_drv.disp = lv_disp_get_default();
    lv_indev_drv_register(&s_indev_drv);
}

void DISP_RA8875_LVGL_Init(void)
{
    if (!TouchCalib_IsValid()) {
        TouchCalib_Init();
    }
    disp_ra8875_init();
    touch_ra8875_init();
}

void DISP_SetTouchCalibCollectMode(bool enable)
{
    s_calib_collect_mode = enable;
    s_calib_was_pressed = false;
    touch_calib_reset_accumulator();
    if (!enable) {
        DISP_ResetTouchState();
    }
}

void DISP_ResetTouchState(void)
{
    s_touch_filter_armed = false;
    s_touch_has_valid = false;
    s_touch_x = 0;
    s_touch_y = 0;
    s_touch_report_x = 0;
    s_touch_report_y = 0;
    s_touch_state = LV_INDEV_STATE_RELEASED;

    /* 清除 LVGL indev 内部残留坐标（last_raw_point 等） */
    lv_indev_t *indev = lv_indev_get_next(NULL);
    while (indev != NULL) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            indev->proc.types.pointer.last_raw_point.x = TOUCH_SAFE_X;
            indev->proc.types.pointer.last_raw_point.y = TOUCH_SAFE_Y;
            indev->proc.types.pointer.last_point.x = TOUCH_SAFE_X;
            indev->proc.types.pointer.last_point.y = TOUCH_SAFE_Y;
            indev->proc.types.pointer.act_point.x = TOUCH_SAFE_X;
            indev->proc.types.pointer.act_point.y = TOUCH_SAFE_Y;
            indev->proc.state = LV_INDEV_STATE_RELEASED;
            lv_indev_reset(indev, NULL);
        }
        indev = lv_indev_get_next(indev);
    }
}

bool DISP_TakeCalibSample(uint16_t *adc_x, uint16_t *adc_y)
{
    if (!s_calib_sample_ready) {
        return false;
    }

    s_calib_sample_ready = false;
    if (adc_x != NULL) {
        *adc_x = s_calib_result_x;
    }
    if (adc_y != NULL) {
        *adc_y = s_calib_result_y;
    }
    return true;
}

#if !defined(PC_SIMULATOR)
void DISP_GetLastTouchRaw(uint16_t *adc_x, uint16_t *adc_y)
{
    if (adc_x != NULL) {
        *adc_x = s_last_adc_x;
    }
    if (adc_y != NULL) {
        *adc_y = s_last_adc_y;
    }
}

bool DISP_GetTouchPoint(int32_t *x, int32_t *y, lv_indev_state_t *state)
{
    if (x != NULL) {
        *x = s_touch_report_x;
    }
    if (y != NULL) {
        *y = s_touch_report_y;
    }
    if (state != NULL) {
        *state = s_touch_state;
    }

    return (s_touch_report_x != 0 || s_touch_report_y != 0
            || s_touch_state == LV_INDEV_STATE_PRESSED);
}
#endif
