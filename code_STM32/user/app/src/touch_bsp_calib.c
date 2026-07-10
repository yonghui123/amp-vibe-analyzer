/**

 * @file    touch_bsp_calib.c

 * @brief   裸机五点触摸校准（参考 v5 TOUCH_Calibration，不经过 LVGL）

 */



#include "touch_bsp_calib.h"

#include "touch_bsp_touch.h"

#include "touch_calib.h"

#include "config.h"

#include "bsp_tft_lcd.h"

#include "bsp_lcd_ra8875.h"

#include "main.h"

#include "cmsis_os.h"

#include <stdio.h>

#include <stdbool.h>



#define BSP_CALIB_BACK      CL_BLUE

#define BSP_CALIB_CROSS_R   10U

#define BSP_CALIB_MIN_DIST2 400U



static FONT_T s_font;



static void calib_draw_cross(int32_t x, int32_t y, uint16_t color)

{

    LCD_DrawCircle((uint16_t)x, (uint16_t)y, BSP_CALIB_CROSS_R, color);

    LCD_DrawLine((uint16_t)(x - BSP_CALIB_CROSS_R), (uint16_t)y,

                 (uint16_t)(x + BSP_CALIB_CROSS_R), (uint16_t)y, color);

    LCD_DrawLine((uint16_t)x, (uint16_t)(y - BSP_CALIB_CROSS_R),

                 (uint16_t)x, (uint16_t)(y + BSP_CALIB_CROSS_R), color);

}



static void calib_show_status(char *line1, char *line2)

{

    LCD_DispStr(5, 110, line1, &s_font);

    if (line2 != NULL) {

        LCD_DispStr(5, 130, line2, &s_font);

    }

}



static bool calib_adc_distinct(const touch_calib_adc_t adc[TOUCH_CALIB_POINT_COUNT],

                               uint8_t step, uint16_t adc_x, uint16_t adc_y)

{

    int32_t dx;

    int32_t dy;



    for (uint8_t i = 0; i < step; i++) {

        dx = (int32_t)adc_x - (int32_t)adc[i].adc_x;

        dy = (int32_t)adc_y - (int32_t)adc[i].adc_y;

        if ((uint32_t)(dx * dx + dy * dy) < BSP_CALIB_MIN_DIST2) {

            return false;

        }

    }

    return true;

}



static void calib_show_done(bool saved)

{

    LCD_ClrScr(BSP_CALIB_BACK);

    LCD_DispStr(5, 40, saved ? "Calibration Complete" : "Save Failed (RAM OK)", &s_font);

    LCD_DispStr(5, 70,

                saved ? "Params saved to Flash."

                      : "Flash write failed; test uses RAM.",

                &s_font);

    LCD_DispStr(5, 100, "Starting GUI...", &s_font);

}



bool TouchBspCalib_Do(void)

{

    touch_calib_adc_t adc[TOUCH_CALIB_POINT_COUNT];

    touch_calib_lcd_t target[TOUCH_CALIB_POINT_COUNT];

    char buf[72];
    uint8_t step;
    bool saved;



    s_font.FontCode = FC_ST_16;

    s_font.FrontColor = CL_WHITE;

    s_font.BackColor = BSP_CALIB_BACK;

    s_font.Space = 0;



    TouchCalib_GetDefaultTargets(target);



    LCD_ClrScr(BSP_CALIB_BACK);

    LCD_DrawRect(0, 0, LCD_GetHeight(), LCD_GetWidth(), CL_WHITE);

    LCD_DispStr(5, 8, "BSP Touch Calibration (no LVGL)", &s_font);

    LCD_DispStr(5, 28, "TL,BR,BL,TR. Hold still 0.5s.", &s_font);

    calib_show_status("Waiting step 1/4...", NULL);



    for (step = 0; step < TOUCH_CALIB_POINT_COUNT; step++) {

        calib_draw_cross(target[step].x, target[step].y, CL_YELLOW);



        TouchBsp_WaitPenRelease();

        for (;;) {
            uint16_t raw_x = 0;
            uint16_t raw_y = 0;

            TouchBsp_CaptureCalibAdc(&raw_x, &raw_y);



            if (!calib_adc_distinct(adc, step, raw_x, raw_y)) {

                calib_show_status("Too close to prior point.", "Retry this cross.");

                continue;

            }



            adc[step].adc_x = raw_x;

            adc[step].adc_y = raw_y;



            snprintf(buf, sizeof(buf), "OK %u/%u raw(%u,%u)",

                     (unsigned)(step + 1U), TOUCH_CALIB_POINT_COUNT,

                     (unsigned)raw_x, (unsigned)raw_y);

            calib_show_status(buf, NULL);

            calib_draw_cross(target[step].x, target[step].y, CL_GREEN);

            osDelay(300);

            break;

        }

    }



    if (!TouchCalib_ComputeFrom4Points(adc, target)) {

        LCD_ClrScr(CL_RED);

        LCD_DispStr(5, 40, "Compute failed. Reboot and retry.", &s_font);

        return false;

    }



    saved = TouchCalib_Save();

    if (saved) {

        TouchCalib_Init();

    }



    calib_show_done(saved);

    osDelay(800);

    return true;

}



void TouchBspCalib_Run(void)

{

    (void)TouchBspCalib_Do();

    for (;;) {

        osDelay(1000);

    }

}


