/**
 * @file    touch_calib.c
 * @brief   RA8875 四点双线性触摸校准（算法对齐 v5 TOUCH_TransX/Y）
 *
 * 持久化：优先 SD 卡 Config/touch_calib.bin（不依赖片内 Flash 空闲扇区）。
 * 仍可读旧版片内 Flash@0x080E0000，便于迁移；新保存只写 SD。
 */

#include "touch_calib.h"
#include "config.h"
#include "storage/sd_fs.h"
#include "storage/path_layout.h"

#if !defined(PC_SIMULATOR)
#include "bsp_tft_lcd.h"
#else
static inline uint16_t LCD_GetWidth(void)
{
    return LCD_WIDTH;
}

static inline uint16_t LCD_GetHeight(void)
{
    return LCD_HEIGHT;
}
#endif

#include <stdio.h>
#include <string.h>

#if !defined(PC_SIMULATOR)
#include "stm32f4xx_hal.h"
#endif

typedef struct {
    uint32_t magic;
    touch_bilinear_t params;
    uint16_t crc;
} touch_calib_store_t;

static touch_bilinear_t s_bilinear;
static bool s_valid;
static bool s_persisted;   /* 来自 Flash 或 SD */
static touch_calib_src_t s_source;

#if !defined(PC_SIMULATOR)
/** 旧版存储地址（只读兼容，不再写入） */
#define TOUCH_CALIB_FLASH_ADDR  0x080E0000UL
#define V5_PARAM_FLASH_ADDR     0x0800C000UL

typedef struct {
    uint32_t ParamVer;
    uint8_t ucBackLight;
    uint8_t TouchDirection;
    uint8_t XYChange;
    uint16_t usAdcX1;
    uint16_t usAdcY1;
    uint16_t usAdcX2;
    uint16_t usAdcY2;
    uint16_t usAdcX3;
    uint16_t usAdcY3;
    uint16_t usAdcX4;
    uint16_t usAdcY4;
    uint16_t usLcdX1;
    uint16_t usLcdY1;
    uint16_t usLcdX2;
    uint16_t usLcdY2;
    uint16_t usLcdX3;
    uint16_t usLcdY3;
    uint16_t usLcdX4;
    uint16_t usLcdY4;
} touch_v5_param_t;
#endif

static uint16_t touch_calib_crc16(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0xFFFFU;
    uint32_t i;
    uint8_t bit;

    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (bit = 0; bit < 8U; bit++) {
            if (crc & 1U) {
                crc = (uint16_t)((crc >> 1) ^ 0xA001U);
            } else {
                crc = (uint16_t)(crc >> 1);
            }
        }
    }
    return crc;
}

static bool touch_calib_apply_store(const touch_calib_store_t *stored,
                                    touch_calib_src_t src)
{
    uint16_t crc;

    if (stored->magic != TOUCH_CALIB_MAGIC
        && stored->magic != TOUCH_CALIB_MAGIC_LEGACY) {
        return false;
    }

    crc = touch_calib_crc16((const uint8_t *)&stored->params,
                            sizeof(stored->params));
    if (crc != stored->crc) {
        return false;
    }

    s_bilinear = stored->params;
    s_valid = true;
    s_persisted = true;
    s_source = src;
    return true;
}

static bool touch_calib_load_from_sd(void)
{
    touch_calib_store_t stored;
    int32_t n;

    if (!SdFs_IsReady()) {
        return false;
    }

    n = SdFs_ReadAll(PATH_FILE_TOUCH_CALIB, &stored, sizeof(stored));
    if (n < (int32_t)sizeof(stored)) {
        return false;
    }

    return touch_calib_apply_store(&stored, TOUCH_CALIB_SRC_SD);
}

#if !defined(PC_SIMULATOR)
static bool touch_calib_load_from_flash(void)
{
    const touch_calib_store_t *stored =
        (const touch_calib_store_t *)TOUCH_CALIB_FLASH_ADDR;

    return touch_calib_apply_store(stored, TOUCH_CALIB_SRC_FLASH);
}
#endif

static void touch_calib_clamp_screen(int32_t *px, int32_t *py)
{
    int32_t w = (int32_t)LCD_GetWidth() - 1;
    int32_t h = (int32_t)LCD_GetHeight() - 1;

    if (*px < 0) {
        *px = 0;
    } else if (*px > w) {
        *px = w;
    }
    if (*py < 0) {
        *py = 0;
    } else if (*py > h) {
        *py = h;
    }
}

static int32_t touch_calib_lerp(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                                uint16_t x)
{
    if (x2 == x1) {
        return (int32_t)y1;
    }
    return (int32_t)y1
         + ((int32_t)(y2 - y1) * (int32_t)((int32_t)x - (int32_t)x1))
           / (int32_t)(x2 - x1);
}

static int32_t touch_calib_abs16(int32_t v)
{
    return (v < 0) ? -v : v;
}

static void touch_calib_detect_xy_change(touch_bilinear_t *p)
{
    int32_t dx;
    int32_t dy;

    p->xy_change = 0U;
    dx = touch_calib_abs16((int32_t)p->adc_x1 - (int32_t)p->adc_x2);
    dy = touch_calib_abs16((int32_t)p->adc_y1 - (int32_t)p->adc_y2);

    if (LCD_GetHeight() < LCD_GetWidth()) {
        if (dx < dy) {
            p->xy_change = 1U;
        }
    } else if (dx > dy) {
        p->xy_change = 1U;
    }
}

static int32_t touch_calib_trans_x(uint16_t adc_x, uint16_t adc_y)
{
    const touch_bilinear_t *tp = &s_bilinear;
    uint16_t x;
    int32_t x1;
    int32_t x2;

    if (tp->xy_change == 0U) {
        x = adc_x;
        x1 = touch_calib_lerp(tp->adc_y1, tp->adc_x1, tp->adc_y2, tp->adc_x3, adc_y);
        x2 = touch_calib_lerp(tp->adc_y1, tp->adc_x4, tp->adc_y2, tp->adc_x2, adc_y);
    } else {
        x = adc_y;
        x1 = touch_calib_lerp(tp->adc_x1, tp->adc_y1, tp->adc_x2, tp->adc_y3, adc_x);
        x2 = touch_calib_lerp(tp->adc_x1, tp->adc_y4, tp->adc_x2, tp->adc_y2, adc_x);
    }

    if (x == 0U) {
        return 0;
    }
    return touch_calib_lerp((uint16_t)x1, tp->lcd_x1, (uint16_t)x2, tp->lcd_x2, x);
}

static int32_t touch_calib_trans_y(uint16_t adc_x, uint16_t adc_y)
{
    const touch_bilinear_t *tp = &s_bilinear;
    uint16_t x;
    int32_t x1;
    int32_t x2;

    if (tp->xy_change == 0U) {
        x = adc_y;
        x1 = touch_calib_lerp(tp->adc_x1, tp->adc_y1, tp->adc_x2, tp->adc_y4, adc_x);
        x2 = touch_calib_lerp(tp->adc_x1, tp->adc_y3, tp->adc_x2, tp->adc_y2, adc_x);
    } else {
        x = adc_x;
        x1 = touch_calib_lerp(tp->adc_y1, tp->adc_x1, tp->adc_y2, tp->adc_x4, adc_y);
        x2 = touch_calib_lerp(tp->adc_y1, tp->adc_x3, tp->adc_y2, tp->adc_x2, adc_y);
    }

    if (x == 0U) {
        return 0;
    }
    return touch_calib_lerp((uint16_t)x1, tp->lcd_y1, (uint16_t)x2, tp->lcd_y2, x);
}

void TouchCalib_GetDefaultTargets(touch_calib_lcd_t target[TOUCH_CALIB_POINT_COUNT])
{
    if (target == NULL) {
        return;
    }

    target[0].x = TOUCH_CALIB_MARGIN;
    target[0].y = TOUCH_CALIB_MARGIN;
    target[1].x = (int32_t)LCD_GetWidth() - TOUCH_CALIB_MARGIN;
    target[1].y = (int32_t)LCD_GetHeight() - TOUCH_CALIB_MARGIN;
    target[2].x = TOUCH_CALIB_MARGIN;
    target[2].y = (int32_t)LCD_GetHeight() - TOUCH_CALIB_MARGIN;
    target[3].x = (int32_t)LCD_GetWidth() - TOUCH_CALIB_MARGIN;
    target[3].y = TOUCH_CALIB_MARGIN;
}

void TouchCalib_Init(void)
{
    bool keep_ram = s_valid;

    s_persisted = false;

    /* 优先 SD（正式存储）；再尝试旧 Flash；最后可选 v5 出厂区 */
    if (touch_calib_load_from_sd()) {
        return;
    }

#if !defined(PC_SIMULATOR)
    if (touch_calib_load_from_flash()) {
        return;
    }

#if GUI_TOUCH_IMPORT_V5_PARAM
    if (TouchCalib_TryImportV5Param()) {
        return;
    }
#endif

    if (keep_ram) {
        s_source = TOUCH_CALIB_SRC_RAM;
        return;
    }

    s_valid = false;
    s_source = TOUCH_CALIB_SRC_NONE;
    memset(&s_bilinear, 0, sizeof(s_bilinear));
#else
    (void)keep_ram;
    /* PC：无文件时给一组屏幕角点，便于模拟器触摸 */
    s_valid = true;
    memset(&s_bilinear, 0, sizeof(s_bilinear));
    s_bilinear.lcd_x2 = LCD_WIDTH - TOUCH_CALIB_MARGIN;
    s_bilinear.lcd_y2 = LCD_HEIGHT - TOUCH_CALIB_MARGIN;
    s_bilinear.lcd_x1 = TOUCH_CALIB_MARGIN;
    s_bilinear.lcd_y1 = TOUCH_CALIB_MARGIN;
    s_bilinear.lcd_x3 = TOUCH_CALIB_MARGIN;
    s_bilinear.lcd_y3 = LCD_HEIGHT - TOUCH_CALIB_MARGIN;
    s_bilinear.lcd_x4 = LCD_WIDTH - TOUCH_CALIB_MARGIN;
    s_bilinear.lcd_y4 = TOUCH_CALIB_MARGIN;
    s_source = TOUCH_CALIB_SRC_RAM;
#endif
}

bool TouchCalib_IsValid(void)
{
    return s_valid;
}

bool TouchCalib_WasLoadedFromFlash(void)
{
    /* 兼容旧 API：Flash 或 SD 持久化加载均视为已从非易失存储加载 */
    return s_persisted;
}

touch_calib_src_t TouchCalib_GetSource(void)
{
    return s_source;
}

void TouchCalib_GetBilinear(touch_bilinear_t *out)
{
    if (out != NULL) {
        *out = s_bilinear;
    }
}

bool TouchCalib_ComputeFrom4Points(const touch_calib_adc_t adc[TOUCH_CALIB_POINT_COUNT],
                                   const touch_calib_lcd_t target[TOUCH_CALIB_POINT_COUNT])
{
    touch_bilinear_t p;

    if (adc == NULL || target == NULL) {
        return false;
    }

    memset(&p, 0, sizeof(p));
    p.adc_x1 = adc[0].adc_x;
    p.adc_y1 = adc[0].adc_y;
    p.adc_x2 = adc[1].adc_x;
    p.adc_y2 = adc[1].adc_y;
    p.adc_x3 = adc[2].adc_x;
    p.adc_y3 = adc[2].adc_y;
    p.adc_x4 = adc[3].adc_x;
    p.adc_y4 = adc[3].adc_y;

    p.lcd_x1 = (uint16_t)target[0].x;
    p.lcd_y1 = (uint16_t)target[0].y;
    p.lcd_x2 = (uint16_t)target[1].x;
    p.lcd_y2 = (uint16_t)target[1].y;
    p.lcd_x3 = (uint16_t)target[2].x;
    p.lcd_y3 = (uint16_t)target[2].y;
    p.lcd_x4 = (uint16_t)target[3].x;
    p.lcd_y4 = (uint16_t)target[3].y;

    touch_calib_detect_xy_change(&p);

    s_bilinear = p;
    s_valid = true;
    s_source = TOUCH_CALIB_SRC_RAM;
    return true;
}

static void touch_calib_edge_nudge(int32_t *px, int32_t *py)
{
    int32_t m = TOUCH_CALIB_MARGIN;
    int32_t w = (int32_t)LCD_GetWidth() - 1;

    if (*py < m && TOUCH_EDGE_NUDGE_Y > 0) {
        *py += (m - *py) * TOUCH_EDGE_NUDGE_Y / m;
    }
    if (*px < m && TOUCH_EDGE_NUDGE_X > 0) {
        *px += (m - *px) * TOUCH_EDGE_NUDGE_X / m;
    }
    if (*px > w - m && TOUCH_EDGE_NUDGE_X > 0) {
        *px -= (*px - (w - m)) * TOUCH_EDGE_NUDGE_X / m;
    }
}

bool TouchCalib_AdcToScreen(uint16_t adc_x, uint16_t adc_y, int32_t *px, int32_t *py)
{
    int32_t x;
    int32_t y;

    if (px == NULL || py == NULL || !s_valid) {
        return false;
    }

    x = touch_calib_trans_x(adc_x, adc_y);
    y = touch_calib_trans_y(adc_x, adc_y);
    touch_calib_edge_nudge(&x, &y);
    touch_calib_clamp_screen(&x, &y);

    *px = x;
    *py = y;
    return true;
}

bool TouchCalib_Save(void)
{
    touch_calib_store_t store;
    int32_t n;

    if (!s_valid) {
        return false;
    }
    if (!SdFs_IsReady()) {
        return false;
    }

    memset(&store, 0, sizeof(store));
    store.magic = TOUCH_CALIB_MAGIC;
    store.params = s_bilinear;
    store.crc = touch_calib_crc16((const uint8_t *)&store.params,
                                 sizeof(store.params));

    n = SdFs_WriteAll(PATH_FILE_TOUCH_CALIB, &store, sizeof(store));
    if (n != (int32_t)sizeof(store)) {
        return false;
    }

    /* 回读校验 */
    {
        touch_calib_store_t verify;
        int32_t rn = SdFs_ReadAll(PATH_FILE_TOUCH_CALIB, &verify, sizeof(verify));
        if (rn < (int32_t)sizeof(verify)
            || !touch_calib_apply_store(&verify, TOUCH_CALIB_SRC_SD)) {
            return false;
        }
    }

    return true;
}

#if !defined(PC_SIMULATOR)
bool TouchCalib_TryImportV5Param(void)
{
    const touch_v5_param_t *p = (const touch_v5_param_t *)V5_PARAM_FLASH_ADDR;
    touch_bilinear_t bl;
    touch_calib_lcd_t lcd[TOUCH_CALIB_POINT_COUNT];

    if (p->ParamVer == 0U || p->ParamVer == 0xFFFFFFFFU) {
        return false;
    }
    if (p->usAdcX1 == 0U && p->usAdcY1 == 0U) {
        return false;
    }

    TouchCalib_GetDefaultTargets(lcd);
    memset(&bl, 0, sizeof(bl));
    bl.adc_x1 = p->usAdcX1;
    bl.adc_y1 = p->usAdcY1;
    bl.adc_x2 = p->usAdcX2;
    bl.adc_y2 = p->usAdcY2;
    bl.adc_x3 = p->usAdcX3;
    bl.adc_y3 = p->usAdcY3;
    bl.adc_x4 = p->usAdcX4;
    bl.adc_y4 = p->usAdcY4;
    bl.lcd_x1 = (p->usLcdX1 != 0U) ? p->usLcdX1 : (uint16_t)lcd[0].x;
    bl.lcd_y1 = (p->usLcdY1 != 0U) ? p->usLcdY1 : (uint16_t)lcd[0].y;
    bl.lcd_x2 = (p->usLcdX2 != 0U) ? p->usLcdX2 : (uint16_t)lcd[1].x;
    bl.lcd_y2 = (p->usLcdY2 != 0U) ? p->usLcdY2 : (uint16_t)lcd[1].y;
    bl.lcd_x3 = (p->usLcdX3 != 0U) ? p->usLcdX3 : (uint16_t)lcd[2].x;
    bl.lcd_y3 = (p->usLcdY3 != 0U) ? p->usLcdY3 : (uint16_t)lcd[2].y;
    bl.lcd_x4 = (p->usLcdX4 != 0U) ? p->usLcdX4 : (uint16_t)lcd[3].x;
    bl.lcd_y4 = (p->usLcdY4 != 0U) ? p->usLcdY4 : (uint16_t)lcd[3].y;
    bl.xy_change = p->XYChange;

    s_bilinear = bl;
    s_valid = true;
    s_persisted = false;
    s_source = TOUCH_CALIB_SRC_V5_PARAM;
    return true;
}

void TouchCalib_FormatLoadDiag(char *buf, size_t len)
{
    if (buf == NULL || len == 0U) {
        return;
    }

    if (TouchCalib_IsValid()) {
        if (s_source == TOUCH_CALIB_SRC_SD) {
            snprintf(buf, len, "OK SD %s", PATH_FILE_TOUCH_CALIB);
        } else if (s_source == TOUCH_CALIB_SRC_FLASH) {
            const touch_calib_store_t *tp4 =
                (const touch_calib_store_t *)TOUCH_CALIB_FLASH_ADDR;
            snprintf(buf, len, "OK Flash@E0000 magic=%08lX",
                     (unsigned long)tp4->magic);
        } else if (s_source == TOUCH_CALIB_SRC_V5_PARAM) {
            const touch_v5_param_t *v5 =
                (const touch_v5_param_t *)V5_PARAM_FLASH_ADDR;
            snprintf(buf, len, "OK v5@C000 ParamVer=%08lX XY=%u",
                     (unsigned long)v5->ParamVer, (unsigned)v5->XYChange);
        } else if (s_source == TOUCH_CALIB_SRC_RAM) {
            snprintf(buf, len, "OK RAM (not saved)");
        } else {
            snprintf(buf, len, "OK (source unknown)");
        }
        return;
    }

    snprintf(buf, len, "NONE sd=%d ready=%d",
             SdFs_Exists(PATH_FILE_TOUCH_CALIB) ? 1 : 0,
             SdFs_IsReady() ? 1 : 0);
}
#endif
