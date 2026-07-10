/**
 * @file    bsp_ad7606.c
 * @brief   AD7606 八通道 ADC 驱动实现
 *
 * 硬件链路：
 *   TIM12 CH2 PWM → CONVST 触发转换
 *   BUSY 下降沿   → EXTI 中断 → AD7606_ReadNowAdc 顺序读 8 路 FSMC
 *
 * 阶段 1：每帧保存完整 raw[8]，任务侧通过 AD7606_GetLatestRaw 快照读取。
 */

#include "bsp_ad7606.h"
#include "main.h"
#include "stm32f4xx_hal.h"

extern TIM_HandleTypeDef htim12;

/** FSMC 映射地址，连续读 8 次依次为 CH1~CH8 */
#define AD7606_RESULT()     (*(volatile int16_t *)0x60200000UL)

/** 最近一次完成的 8 路原始值（中断写、任务读） */
static volatile int16_t  s_latest_raw[AD7606_CHANNEL_COUNT];
/** 已完成转换组计数（每组含 8 通道） */
static volatile uint32_t s_sample_count;
/** 是否处于连续采集状态 */
static volatile bool     s_recording;
/** 当前采样率 Hz */
static volatile uint32_t s_sample_hz;

/**
 * @brief  将 8 路读数写入最新快照（仅在临界区内调用）
 */
static void adc_store_snapshot(const int16_t raw[AD7606_CHANNEL_COUNT])
{
    for (uint8_t i = 0; i < AD7606_CHANNEL_COUNT; i++) {
        s_latest_raw[i] = raw[i];
    }
    s_sample_count++;
}

void bsp_InitAD7606(void)
{
    AD7606_SetOS(AD_OS_NO);
    AD7606_SetInputRange(0);
    AD7606_Reset();

    s_recording = false;
    s_sample_hz = 0U;
    s_sample_count = 0U;
    for (uint8_t i = 0; i < AD7606_CHANNEL_COUNT; i++) {
        s_latest_raw[i] = 0;
    }
}

void AD7606_SetOS(uint8_t os_mode)
{
    switch (os_mode) {
    case AD_OS_X2:
        HAL_GPIO_WritePin(AD7606_OS0_GPIO_Port, AD7606_OS0_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(AD7606_OS1_GPIO_Port, AD7606_OS1_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(AD7606_OS2_GPIO_Port, AD7606_OS2_Pin, GPIO_PIN_RESET);
        break;
    case AD_OS_X4:
        HAL_GPIO_WritePin(AD7606_OS2_GPIO_Port, AD7606_OS2_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(AD7606_OS1_GPIO_Port, AD7606_OS1_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(AD7606_OS0_GPIO_Port, AD7606_OS0_Pin, GPIO_PIN_RESET);
        break;
    case AD_OS_X8:
        HAL_GPIO_WritePin(AD7606_OS2_GPIO_Port, AD7606_OS2_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(AD7606_OS1_GPIO_Port, AD7606_OS1_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(AD7606_OS0_GPIO_Port, AD7606_OS0_Pin, GPIO_PIN_SET);
        break;
    case AD_OS_X16:
        HAL_GPIO_WritePin(AD7606_OS2_GPIO_Port, AD7606_OS2_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(AD7606_OS1_GPIO_Port, AD7606_OS1_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(AD7606_OS0_GPIO_Port, AD7606_OS0_Pin, GPIO_PIN_RESET);
        break;
    case AD_OS_X32:
        HAL_GPIO_WritePin(AD7606_OS2_GPIO_Port, AD7606_OS2_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(AD7606_OS1_GPIO_Port, AD7606_OS1_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(AD7606_OS0_GPIO_Port, AD7606_OS0_Pin, GPIO_PIN_SET);
        break;
    case AD_OS_X64:
        HAL_GPIO_WritePin(AD7606_OS2_GPIO_Port, AD7606_OS2_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(AD7606_OS1_GPIO_Port, AD7606_OS1_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(AD7606_OS0_GPIO_Port, AD7606_OS0_Pin, GPIO_PIN_RESET);
        break;
    case AD_OS_NO:
    default:
        HAL_GPIO_WritePin(AD7606_OS2_GPIO_Port, AD7606_OS2_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(AD7606_OS1_GPIO_Port, AD7606_OS1_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(AD7606_OS0_GPIO_Port, AD7606_OS0_Pin, GPIO_PIN_RESET);
        break;
    }
}

void AD7606_SetInputRange(uint8_t range)
{
    if (range == 0U) {
        HAL_GPIO_WritePin(AD7606_RAGE_GPIO_Port, AD7606_RAGE_Pin, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(AD7606_RAGE_GPIO_Port, AD7606_RAGE_Pin, GPIO_PIN_SET);
    }
}

void AD7606_Reset(void)
{
    HAL_GPIO_WritePin(AD7606_RESET_GPIO_Port, AD7606_RESET_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(AD7606_RESET_GPIO_Port, AD7606_RESET_Pin, GPIO_PIN_SET);
    HAL_Delay(2);
    HAL_GPIO_WritePin(AD7606_RESET_GPIO_Port, AD7606_RESET_Pin, GPIO_PIN_RESET);
}

void AD7606_StartConvst(void)
{
    /* CONVST 由 TIM12 PWM 驱动；此处保留占位供单次软触发扩展 */
}

void AD7606_ReadNowAdc(void)
{
    int16_t snap[AD7606_CHANNEL_COUNT];

    /* AD7606 一次转换后须连续读 8 次，顺序对应 V1~V8 / channel_id 1~8 */
    for (uint8_t i = 0; i < AD7606_CHANNEL_COUNT; i++) {
        snap[i] = AD7606_RESULT();
    }

    /* 临界区：避免任务读快照时与中断写交错 */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    adc_store_snapshot(snap);
    __set_PRIMASK(primask);

    if (s_recording) {
        AD7606_SampleHook(snap);
    }
}

static uint32_t ad7606_clamp_sample_hz(uint32_t hz)
{
    if (hz == 0U) {
        hz = AD7606_DEFAULT_SAMPLE_HZ;
    }
    if (hz < AD7606_SAMPLE_HZ_MIN) {
        hz = AD7606_SAMPLE_HZ_MIN;
    } else if (hz > AD7606_SAMPLE_HZ_MAX) {
        hz = AD7606_SAMPLE_HZ_MAX;
    }
    return hz;
}

void AD7606_EnterAutoMode(uint32_t sample_hz)
{
    TIM_OC_InitTypeDef sConfigOC = {0};

    sample_hz = ad7606_clamp_sample_hz(sample_hz);
    s_sample_hz = sample_hz;

    HAL_TIM_PWM_Stop(&htim12, TIM_CHANNEL_2);
    HAL_TIM_PWM_Init(&htim12);

    /* TIM12 时钟 84MHz（Prescaler=84-1 → 1MHz 计数） */
    htim12.Instance = TIM12;
    htim12.Init.Prescaler = 84U - 1U;
    htim12.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim12.Init.Period = (1000000U / sample_hz) - 1U;
    htim12.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim12.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&htim12) != HAL_OK) {
        Error_Handler();
    }

    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 50U;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim12, &sConfigOC, TIM_CHANNEL_2) != HAL_OK) {
        Error_Handler();
    }

    HAL_TIM_MspPostInit(&htim12);

    HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 15, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

    HAL_TIM_PWM_Start(&htim12, TIM_CHANNEL_2);
}

void AD7606_StartRecord(uint32_t sample_hz)
{
    AD7606_StopRecord();
    AD7606_Reset();
    AD7606_StartConvst();

    s_sample_count = 0U;
    for (uint8_t i = 0; i < AD7606_CHANNEL_COUNT; i++) {
        s_latest_raw[i] = 0;
    }

    AD7606_EnterAutoMode(sample_hz);
    s_recording = true;
}

void AD7606_StopRecord(void)
{
    HAL_TIM_PWM_Stop(&htim12, TIM_CHANNEL_2);
    HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);
    s_recording = false;
}

bool AD7606_GetLatestRaw(int16_t out[AD7606_CHANNEL_COUNT])
{
    if (out == NULL) {
        return false;
    }

    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    for (uint8_t i = 0; i < AD7606_CHANNEL_COUNT; i++) {
        out[i] = s_latest_raw[i];
    }
    bool valid = (s_sample_count > 0U);
    __set_PRIMASK(primask);

    return valid;
}

uint32_t AD7606_GetSampleCount(void)
{
    return s_sample_count;
}

bool AD7606_IsRecording(void)
{
    return s_recording;
}

uint32_t AD7606_GetSampleHz(void)
{
    return s_sample_hz;
}

/**
 * @brief  默认空实现；acq_pipeline.c 提供强符号重载
 */
__attribute__((weak)) void AD7606_SampleHook(const int16_t raw[AD7606_CHANNEL_COUNT])
{
    (void)raw;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin != AD7606_BUSY_Pin) {
        return;
    }
    if (HAL_GPIO_ReadPin(AD7606_BUSY_GPIO_Port, AD7606_BUSY_Pin) == GPIO_PIN_RESET) {
        AD7606_ReadNowAdc();
    }
}
