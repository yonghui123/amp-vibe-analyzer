/**

 * @file    touch_calib.h

 * @brief   RA8875 电阻屏四点双线性校准（对齐 v5 bsp_touch.c）

 */



#ifndef __TOUCH_CALIB_H

#define __TOUCH_CALIB_H



#include <stdint.h>

#include <stdbool.h>

#include <stddef.h>



#define TOUCH_CALIB_POINT_COUNT   4

#define TOUCH_CALIB_MAGIC         0x54503443u  /* 'TP4C' */

#define TOUCH_CALIB_MAGIC_LEGACY  0x54503442u  /* 'TP4B' 旧版，结构相同 */

#define TOUCH_CALIB_MARGIN        32

#define TOUCH_EDGE_NUDGE_X        6

#define TOUCH_EDGE_NUDGE_Y        8



typedef struct {

    uint16_t adc_x1;

    uint16_t adc_y1;

    uint16_t adc_x2;

    uint16_t adc_y2;

    uint16_t adc_x3;

    uint16_t adc_y3;

    uint16_t adc_x4;

    uint16_t adc_y4;

    uint16_t lcd_x1;

    uint16_t lcd_y1;

    uint16_t lcd_x2;

    uint16_t lcd_y2;

    uint16_t lcd_x3;

    uint16_t lcd_y3;

    uint16_t lcd_x4;

    uint16_t lcd_y4;

    uint8_t xy_change;

    uint8_t reserved;

} touch_bilinear_t;



typedef struct {

    uint16_t adc_x;

    uint16_t adc_y;

} touch_calib_adc_t;



typedef struct {

    int32_t x;

    int32_t y;

} touch_calib_lcd_t;



typedef enum {
    TOUCH_CALIB_SRC_NONE = 0,
    TOUCH_CALIB_SRC_FLASH,
    TOUCH_CALIB_SRC_V5_PARAM,
    TOUCH_CALIB_SRC_RAM,
} touch_calib_src_t;



void TouchCalib_Init(void);

bool TouchCalib_IsValid(void);

bool TouchCalib_WasLoadedFromFlash(void);

touch_calib_src_t TouchCalib_GetSource(void);



bool TouchCalib_AdcToScreen(uint16_t adc_x, uint16_t adc_y, int32_t *px, int32_t *py);



bool TouchCalib_ComputeFrom4Points(const touch_calib_adc_t adc[TOUCH_CALIB_POINT_COUNT],

                                   const touch_calib_lcd_t target[TOUCH_CALIB_POINT_COUNT]);



void TouchCalib_GetDefaultTargets(touch_calib_lcd_t target[TOUCH_CALIB_POINT_COUNT]);

void TouchCalib_GetBilinear(touch_bilinear_t *out);



#if !defined(PC_SIMULATOR)

bool TouchCalib_Save(void);

bool TouchCalib_TryImportV5Param(void);

/** 诊断：Flash@E0000 magic、v5@C000 ParamVer、当前加载来源 */

void TouchCalib_FormatLoadDiag(char *buf, size_t len);

#endif



#endif /* __TOUCH_CALIB_H */


