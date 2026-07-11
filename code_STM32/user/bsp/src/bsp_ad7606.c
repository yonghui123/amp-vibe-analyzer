/**
 * @file    bsp_ad7606.c
 * @brief   AD7606 八通道同步 ADC 驱动
 *
 * 对照说明（相对 code_source 原版）：
 *   - 硬件链路保持原版：TIM12 PWM→CONVST、BUSY 中断、FSMC 0x60200000 连读 8 路
 *   - 原版注释曾写 adc_raw_data[8][1024]，实际落地是 [128] 且 ReadNowAdc 只保留第 8 路
 *   - 本工程恢复「环形历史缓冲」思路，并改为真正保存 8 路；缓冲在 .bss 静态区（非栈、非堆）
 *   - 为本工程业务保留：GetLatestRaw / SampleHook / 采样计数（acq_pipeline / AcqTask 依赖）
 *   - 不修改 code_source 目录下的原文件
 *
 * 读数时间线：
 *   TIM12(PB15) → CONVST → BUSY 下降沿(PB14) → ReadNowAdc → 写入环形缓冲 + 最新快照
 *   →（采集中）SampleHook → acq_pipeline 振动 ring
 *   → AcqTask 调 GetLatestRaw 取最新 8 路做显示
 */

#include "bsp_ad7606.h"
#include "main.h"
#include "stm32f4xx_hal.h"

extern TIM_HandleTypeDef htim12;

/* AD7606 FSMC 总线地址，只能读，无需写 */
#define AD7606_RESULT()     (*(volatile int16_t *)0x60200000UL)

/*
 * 环形历史缓冲：每通道 ADC_DATA_SIZE 个样本。
 * 放在文件作用域静态区（.bss），启动时清零 —— 不是函数栈，也不用 malloc 堆。
 * 原版注释写过 1024；code_source 实际用 128。8×128×2≈2KB，RAM 可接受。
 * 若改回 1024：8×1024×2=16KB，仍应放静态区，切勿放在中断/任务栈上。
 */
#ifndef ADC_DATA_SIZE
#define ADC_DATA_SIZE       128
#endif

static volatile int16_t  s_adc_raw_data[AD7606_CHANNEL_COUNT][ADC_DATA_SIZE];
static volatile uint32_t s_adc_write_index;   /* 下一写入位置 0..ADC_DATA_SIZE-1 */
static volatile uint32_t s_sample_count;      /* 累计转换组数（每组 8 通道） */
static volatile bool     s_recording;
static volatile uint32_t s_sample_hz;

/** 最新一帧快照，供 GetLatestRaw 快速拷贝（与环形缓冲同步更新） */
static volatile int16_t  s_latest_raw[AD7606_CHANNEL_COUNT];

/*
 * P04 加速度信号采集板接线（原版注释保留，便于对照原理图）：
 *
 *   PD0/FSMC_D2  PD1/FSMC_D3  PD4/FSMC_NOE  PD5/FSMC_NWE
 *   PD8/D13 PD9/D14 PD10/D15  PD14/D0 PD15/D1
 *   PE4/A20 PE5/A21  PE7~PE15/D4~D12
 *   PD7/FSMC_NE1     --- 主片选（TFT 与 AD7606 共用）
 *
 *   PD6/OS0  PA8/OS1  PB10/OS2
 *   PB15/CONVST（TIM12_CH2）  PB11/RANGE  PB1/RESET  PB14/BUSY
 */

static void adc_store_frame(const int16_t raw[AD7606_CHANNEL_COUNT])
{
    uint32_t w = s_adc_write_index;

    for (uint8_t ch = 0; ch < AD7606_CHANNEL_COUNT; ch++) {
        s_adc_raw_data[ch][w] = raw[ch];
        s_latest_raw[ch] = raw[ch];
    }

    s_adc_write_index = (w + 1U) % (uint32_t)ADC_DATA_SIZE;
    s_sample_count++;
}

/**
 * @brief  上电初始化（不启动连续采样）
 */
void bsp_InitAD7606(void)
{
    AD7606_SetOS(AD_OS_NO);
    AD7606_SetInputRange(0);   /* 0=±5V，1=±10V（与原版一致） */
    AD7606_Reset();

    s_recording = false;
    s_sample_hz = 0U;
    s_sample_count = 0U;
    s_adc_write_index = 0U;
    for (uint8_t ch = 0; ch < AD7606_CHANNEL_COUNT; ch++) {
        s_latest_raw[ch] = 0;
        for (uint32_t i = 0; i < (uint32_t)ADC_DATA_SIZE; i++) {
            s_adc_raw_data[ch][i] = 0;
        }
    }
}

/**
 * @brief  配置过采样倍率（OS2:OS1:OS0）
 */
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

/**
 * @brief  输入量程：0=±5V，1=±10V
 */
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
    /* CONVST 由 TIM12 PWM 驱动；保留空函数与原版一致，便于以后软触发 */
}

/**
 * @brief  读取本次转换的 8 路结果（BUSY 中断中调用）
 * @note   相对原版改动：原版连读时丢弃 CH1~CH7、只把 CH8 写入一维缓冲；
 *         本工程写入 s_adc_raw_data[ch][index]，8 路都保留。
 */
void AD7606_ReadNowAdc(void)
{
    int16_t snap[AD7606_CHANNEL_COUNT];

    for (uint8_t i = 0; i < AD7606_CHANNEL_COUNT; i++) {
        snap[i] = AD7606_RESULT();
    }

    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    adc_store_frame(snap);
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

/**
 * @brief  自动采集：TIM12 PWM 触发 + 打开 BUSY 中断
 * @note   时基与原版一致：Prescaler 使计数约 1MHz，Period≈1e6/hz
 */
void AD7606_EnterAutoMode(uint32_t sample_hz)
{
    TIM_OC_InitTypeDef sConfigOC = {0};

    sample_hz = ad7606_clamp_sample_hz(sample_hz);
    s_sample_hz = sample_hz;

    HAL_TIM_PWM_Stop(&htim12, TIM_CHANNEL_2);
    HAL_TIM_PWM_Init(&htim12);

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
    s_adc_write_index = 0U;
    for (uint8_t ch = 0; ch < AD7606_CHANNEL_COUNT; ch++) {
        s_latest_raw[ch] = 0;
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

/**
 * @brief  拷贝某通道环形缓冲中最近 count 个样本（从旧到新）
 * @return 实际拷贝个数
 */
uint32_t AD7606_CopyChannelHistory(uint8_t channel_id, int16_t *out, uint32_t count)
{
    if (out == NULL || count == 0U ||
        channel_id < 1U || channel_id > AD7606_CHANNEL_COUNT) {
        return 0U;
    }

    uint8_t ch = (uint8_t)(channel_id - 1U);
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    uint32_t w = s_adc_write_index;
    uint32_t available = s_sample_count;
    if (available > (uint32_t)ADC_DATA_SIZE) {
        available = (uint32_t)ADC_DATA_SIZE;
    }
    if (count > available) {
        count = available;
    }
    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = (w + (uint32_t)ADC_DATA_SIZE - count + i) % (uint32_t)ADC_DATA_SIZE;
        out[i] = s_adc_raw_data[ch][idx];
    }
    __set_PRIMASK(primask);

    return count;
}

uint32_t AD7606_GetFifoDepth(void)
{
    return (uint32_t)ADC_DATA_SIZE;
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
