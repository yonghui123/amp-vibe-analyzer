/**
 * @file    touch_bsp_test.c
 * @brief   参考 v5 出厂 touch_test.c：直接 RA8875 绘图 + 触摸 ADC，不经过 LVGL
 */

#include "touch_bsp_test.h"
#include "touch_bsp_touch.h"
#include "touch_calib.h"
#include "config.h"
#include "bsp_tft_lcd.h"
#include "bsp_lcd_ra8875.h"
#include "main.h"
#include "cmsis_os.h"
#include <stdio.h>

#define BSP_TEST_BACK_COLOR     CL_BLUE
#define BSP_TEST_REF_RADIUS     6U
#define BSP_TEST_TEXT_MS        200U

static FONT_T s_font;
static uint32_t s_last_text_ms;

static void touch_bsp_draw_header(void)
{
    char buf[96];
    char chip[24];
    touch_bilinear_t bl;

    LCD_Fill_Rect(0, 0, 20, LCD_GetWidth(), CL_GREEN);
    s_font.BackColor = CL_GREEN;
    LCD_DispStr(5, 3, ">>> BSP TOUCH TEST PAGE <<<", &s_font);

    s_font.BackColor = BSP_TEST_BACK_COLOR;
    LCD_GetChipDescribe(chip);
    LCD_DispStr(5, 23, "BSP Touch Test (no LVGL)", &s_font);
    snprintf(buf, sizeof(buf), "%s  %ux%u", chip,
             (unsigned)LCD_GetWidth(), (unsigned)LCD_GetHeight());
    LCD_DispStr(5, 43, buf, &s_font);

    if (TouchCalib_IsValid()) {
        TouchCalib_GetBilinear(&bl);
        snprintf(buf, sizeof(buf), "Calib: 4-point bilinear XY=%u",
                 (unsigned)bl.xy_change);
        LCD_DispStr(5, 63, buf, &s_font);
        snprintf(buf, sizeof(buf), "TL(%u,%u) BR(%u,%u)",
                 (unsigned)bl.adc_x1, (unsigned)bl.adc_y1,
                 (unsigned)bl.adc_x2, (unsigned)bl.adc_y2);
        LCD_DispStr(5, 83, buf, &s_font);
    } else {
        LCD_DispStr(5, 63, "Calib: NONE - run BSP calib first!", &s_font);
        LCD_DispStr(5, 83, "Pos will mirror until calibrated.", &s_font);
    }

    LCD_DispStr(5, LCD_GetHeight() - 20, "Reset MCU to exit test mode", &s_font);
}

static void touch_bsp_draw_static_frame(void)
{
    touch_calib_lcd_t ref[TOUCH_CALIB_POINT_COUNT];
    uint8_t i;

    LCD_ClrScr(BSP_TEST_BACK_COLOR);
    LCD_DrawRect(0, 0, LCD_GetHeight(), LCD_GetWidth(), CL_WHITE);
    LCD_DrawRect(2, 2, LCD_GetHeight() - 4U, LCD_GetWidth() - 4U, CL_YELLOW);

    TouchCalib_GetDefaultTargets(ref);
    for (i = 0; i < TOUCH_CALIB_POINT_COUNT; i++) {
        LCD_DrawCircle((uint16_t)ref[i].x, (uint16_t)ref[i].y,
                       BSP_TEST_REF_RADIUS, CL_CYAN);
    }

    touch_bsp_draw_header();
}

static void touch_bsp_update_live(uint16_t adc_x, uint16_t adc_y,
                                  int32_t px, int32_t py)
{
    char buf[80];

    snprintf(buf, sizeof(buf), "ADC  X=%4u Y=%4u   ", (unsigned)adc_x, (unsigned)adc_y);
    LCD_DispStr(5, 103, buf, &s_font);

    snprintf(buf, sizeof(buf), "Pos  X=%4d Y=%4d   ", (int)px, (int)py);
    LCD_DispStr(5, 123, buf, &s_font);
}

void TouchBspTest_Run(void)
{
    uint16_t adc_x = 0;
    uint16_t adc_y = 0;
    int32_t px = 0;
    int32_t py = 0;
    bool was_pressed = false;
    int32_t last_px = -1;
    int32_t last_py = -1;

    s_font.FontCode = FC_ST_16;
    s_font.FrontColor = CL_WHITE;
    s_font.BackColor = BSP_TEST_BACK_COLOR;
    s_font.Space = 0;
    s_last_text_ms = 0U;

    touch_bsp_draw_static_frame();

    for (;;) {
        uint32_t now = osKernelGetTickCount();

        if (TouchBsp_ReadAdcFiltered(&adc_x, &adc_y)) {
            (void)TouchBsp_AdcToScreen(adc_x, adc_y, &px, &py);

            if (!was_pressed) {
                LCD_DrawCircle((uint16_t)px, (uint16_t)py, 3, CL_RED);
                was_pressed = true;
            } else if (px != last_px || py != last_py) {
                LCD_DrawCircle((uint16_t)px, (uint16_t)py, 2, CL_YELLOW);
            }

            last_px = px;
            last_py = py;

            if ((now - s_last_text_ms) >= BSP_TEST_TEXT_MS) {
                touch_bsp_update_live(adc_x, adc_y, px, py);
                s_last_text_ms = now;
            }
        } else if (!TouchBsp_PenDown()) {
            was_pressed = false;
            last_px = -1;
            last_py = -1;
        }

        osDelay(5);
    }
}
