/**
 * @file    stub_bsp_ad7606.c
 * @brief   PC 模拟器 AD7606 桩驱动
 *
 * 为 acq_pipeline 提供假 raw[8] 数据，便于阶段 2 在无硬件环境下验证校准与路由。
 * 仅 PC 模拟器 CMake 工程链接本文件；STM32 使用 user/bsp/src/bsp_ad7606.c。
 */

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

/** 与 bsp_ad7606.h 一致的通道数 */
#define AD7606_CHANNEL_COUNT    8

/** 模拟帧计数，用于生成时变波形 */
static uint32_t s_sim_frame_count;

/**
 * @brief  生成并拷贝最近一次 8 路模拟 raw 值
 *
 * 每路为不同相位/幅度的正弦叠加直流偏置，便于肉眼区分通道与验证校准。
 *
 * @param  out  长度至少 8 的 int16 数组；不可为 NULL
 * @return true=成功；false=out 为空
 */
bool AD7606_GetLatestRaw(int16_t out[AD7606_CHANNEL_COUNT])
{
    if (out == NULL) {
        return false;
    }

    s_sim_frame_count++;

    for (uint8_t i = 0; i < AD7606_CHANNEL_COUNT; i++) {
        float t = (float)s_sim_frame_count * 0.05f + (float)i * 0.785398163f;
        float v = 800.0f * sinf(t) + (float)(i * 200);
        out[i] = (int16_t)v;
    }

    return true;
}
