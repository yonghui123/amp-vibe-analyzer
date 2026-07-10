/**

 * @file    touch_bsp_touch.c

 * @brief   裸机 RA8875 触摸采样（对齐 v5：稳定按压中值，不含松手滑动）

 */



#include "touch_bsp_touch.h"

#include "touch_calib.h"

#include "config.h"

#include "bsp_lcd_ra8875.h"

#include "bsp_ra8875.h"

#include "main.h"

#include "cmsis_os.h"

#include "stm32f4xx_hal.h"



#define BSP_ADC_MAX             1023U

#define BSP_ADC_VALID_OFFSET    2U

#define BSP_BUSY_RETRY          12U

#define BSP_DOWN_VALID_COUNT    6U

#define BSP_SAMPLE_COUNT        10U



bool TouchBsp_PenDown(void)

{

    return HAL_GPIO_ReadPin(TP_INT_GPIO_Port, TP_INT_Pin) == GPIO_PIN_RESET;

}



static bool touch_adc_valid(uint16_t adc_x, uint16_t adc_y)

{

    if (adc_x <= BSP_ADC_VALID_OFFSET || adc_y <= BSP_ADC_VALID_OFFSET) {

        return false;

    }

    if (adc_x >= BSP_ADC_MAX - BSP_ADC_VALID_OFFSET

        || adc_y >= BSP_ADC_MAX - BSP_ADC_VALID_OFFSET) {

        return false;

    }

    return true;

}



bool TouchBsp_TryReadAdc(uint16_t *adc_x, uint16_t *adc_y)

{

    uint16_t raw_x = 0;

    uint16_t raw_y = 0;

    uint8_t retry;



    if (!TouchBsp_PenDown()) {

        return false;

    }



    for (retry = 0; retry < BSP_BUSY_RETRY; retry++) {

        if (!RA8875_IsBusy()) {

            raw_x = RA8875_TouchReadX();

            raw_y = RA8875_TouchReadY();

            if (touch_adc_valid(raw_x, raw_y)) {

                if (adc_x != NULL) {

                    *adc_x = raw_x;

                }

                if (adc_y != NULL) {

                    *adc_y = raw_y;

                }

                return true;

            }

            return false;

        }

#if defined(PC_SIMULATOR)
        osDelay(1);
#else
        /* 短自旋等待 FSMC 刷新结束，避免 osDelay 让出 CPU 导致滑动坐标不更新 */
        for (volatile uint16_t spin = 0; spin < 1200U; spin++) {
            __NOP();
        }
#endif

    }

    return false;

}



static uint16_t touch_median_trimmed(uint16_t *buf, uint8_t count)
{
    uint8_t flag;
    uint8_t i;
    uint16_t tmp;
    uint32_t sum = 0U;

    do {
        flag = 0U;
        for (i = 0; i + 1U < count; i++) {
            if (buf[i] > buf[i + 1U]) {
                tmp = buf[i + 1U];
                buf[i + 1U] = buf[i];
                buf[i] = tmp;
                flag = 1U;
            }
        }
    } while (flag);

    for (i = count / 3U; i < (2U * count) / 3U; i++) {
        sum += buf[i];
    }
    return (uint16_t)(sum / (count / 3U));
}



void TouchBsp_WaitPenRelease(void)

{

    uint8_t idle = 0U;



    for (;;) {

        uint16_t rx = 0;

        uint16_t ry = 0;



        if (TouchBsp_TryReadAdc(&rx, &ry)) {

            idle = 0U;

        } else if (!TouchBsp_PenDown()) {

            if (++idle > 5U) {

                return;

            }

        } else {

            idle = 0U;

        }

        osDelay(10);

    }

}



bool TouchBsp_CaptureCalibAdc(uint16_t *adc_x, uint16_t *adc_y)

{

    uint8_t stable = 0U;

    uint16_t x_buf[BSP_SAMPLE_COUNT];

    uint16_t y_buf[BSP_SAMPLE_COUNT];

    uint8_t sample_i = 0U;



    for (;;) {

        uint16_t rx = 0;

        uint16_t ry = 0;



        if (TouchBsp_TryReadAdc(&rx, &ry)) {

            if (++stable >= BSP_DOWN_VALID_COUNT) {

                x_buf[sample_i] = rx;

                y_buf[sample_i] = ry;

                sample_i++;

                if (sample_i >= BSP_SAMPLE_COUNT) {

                    if (adc_x != NULL) {

                        *adc_x = touch_median_trimmed(x_buf, BSP_SAMPLE_COUNT);

                    }

                    if (adc_y != NULL) {

                        *adc_y = touch_median_trimmed(y_buf, BSP_SAMPLE_COUNT);

                    }

                    return true;

                }

                osDelay(5);

            } else {

                osDelay(10);

            }

        } else {

            stable = 0U;

            sample_i = 0U;

            if (!TouchBsp_PenDown()) {

                osDelay(10);

            }

        }

    }

}



bool TouchBsp_WaitReleaseAdcSample(uint16_t *adc_x, uint16_t *adc_y)

{

    TouchBsp_WaitPenRelease();

    return TouchBsp_CaptureCalibAdc(adc_x, adc_y);

}



bool TouchBsp_ReadAdcFiltered(uint16_t *adc_x, uint16_t *adc_y)

{

    uint16_t x_buf[BSP_SAMPLE_COUNT];

    uint16_t y_buf[BSP_SAMPLE_COUNT];

    uint8_t got = 0U;



    while (got < BSP_SAMPLE_COUNT) {

        uint16_t rx = 0;

        uint16_t ry = 0;



        if (TouchBsp_TryReadAdc(&rx, &ry)) {

            x_buf[got] = rx;

            y_buf[got] = ry;

            got++;

        } else if (!TouchBsp_PenDown()) {

            break;

        }

        osDelay(2);

    }



    if (got < BSP_SAMPLE_COUNT / 3U) {

        return false;

    }



    if (adc_x != NULL) {

        *adc_x = touch_median_trimmed(x_buf, got);

    }

    if (adc_y != NULL) {

        *adc_y = touch_median_trimmed(y_buf, got);

    }

    return true;

}



bool TouchBsp_AdcToScreen(uint16_t adc_x, uint16_t adc_y, int32_t *px, int32_t *py)

{

    if (TouchCalib_IsValid()) {

        return TouchCalib_AdcToScreen(adc_x, adc_y, px, py);

    }



    if (px != NULL) {

        *px = (int32_t)adc_x * (LCD_WIDTH - 1) / BSP_ADC_MAX;

    }

    if (py != NULL) {

        *py = (int32_t)adc_y * (LCD_HEIGHT - 1) / BSP_ADC_MAX;

    }

    return true;

}


