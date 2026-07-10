/**
 * @file    signal_algo.c
 * @brief   信号处理纯函数算法实现
 *
 * 本文件不 include gui_*.h / channel_data / config_store 等模块。
 * 仅依赖标准 C 数学库。
 */

#include "signal_algo.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ==================== 校准 ==================== */

float SignalAlgo_Calibrate(int16_t raw, float coefficient, float offset)
{
    return (float)raw * coefficient + offset;
}

/* ==================== 滑动平均 ==================== */

uint32_t SignalAlgo_MovingAverageWinSize(uint8_t moving_avg_param)
{
    /* PRD 5.2：窗口 = 滑动平均参数 + 1；参数 0 → 窗长 1 */
    return (uint32_t)moving_avg_param + 1U;
}

void SignalAlgo_MovingAverageInPlace(float *buf, uint32_t count, uint32_t win_size,
                                     float *scratch, uint32_t scratch_cap)
{
    if (buf == NULL || count < 2U || win_size <= 1U) {
        return;
    }
    if (scratch == NULL || scratch_cap < count) {
        return;
    }

    memcpy(scratch, buf, count * sizeof(float));

    for (uint32_t i = 0U; i < count; i++) {
        uint32_t start = (i >= win_size - 1U) ? (i - win_size + 1U) : 0U;
        float sum = 0.0f;
        uint32_t n = 0U;
        for (uint32_t j = start; j <= i; j++) {
            sum += scratch[j];
            n++;
        }
        buf[i] = sum / (float)n;
    }
}

/* ==================== IIR 带通（自包含双二阶，PC/STM32 同一代码路径） ==================== */

bool SignalAlgo_BandpassDesign(uint16_t rpm, float sample_hz, float bw_pct_half,
                               SignalAlgo_BiquadCoef_t *coef)
{
    if (coef == NULL || rpm < 100U || sample_hz < 10.0f || bw_pct_half < 0.1f) {
        return false;
    }

    float f0 = (float)rpm / 60.0f;
    if (f0 <= 0.0f || f0 >= sample_hz * 0.45f) {
        return false;
    }

    /*
     * RBJ Audio EQ Cookbook — bandpass (constant skirt gain)
     *   bandwidth = f0 × 2 × (bw_pct_half/100)
     *   Q = f0 / bandwidth
     */
    float bandwidth = f0 * 2.0f * (bw_pct_half / 100.0f);
    if (bandwidth < 0.01f) {
        bandwidth = 0.01f;
    }
    float Q = f0 / bandwidth;

    float w0 = 2.0f * (float)M_PI * f0 / sample_hz;
    float cos_w0 = cosf(w0);
    float sin_w0 = sinf(w0);
    float alpha = sin_w0 / (2.0f * Q);

    float a0 = 1.0f + alpha;
    coef->b0 = alpha / a0;
    coef->b1 = 0.0f;
    coef->b2 = -alpha / a0;
    coef->a1 = (-2.0f * cos_w0) / a0;
    coef->a2 = (1.0f - alpha) / a0;

    return true;
}

void SignalAlgo_BiquadReset(SignalAlgo_BiquadState_t *state)
{
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
    }
}

float SignalAlgo_BiquadStep(float x, const SignalAlgo_BiquadCoef_t *coef,
                            SignalAlgo_BiquadState_t *state)
{
    if (coef == NULL || state == NULL) {
        return x;
    }

    float y = coef->b0 * x
            + coef->b1 * state->x1
            + coef->b2 * state->x2
            - coef->a1 * state->y1
            - coef->a2 * state->y2;

    state->x2 = state->x1;
    state->x1 = x;
    state->y2 = state->y1;
    state->y1 = y;

    return y;
}

void SignalAlgo_BandpassInPlace(float *buf, uint32_t count,
                                const SignalAlgo_BiquadCoef_t *coef,
                                SignalAlgo_BiquadState_t *state)
{
    if (buf == NULL || count == 0U || coef == NULL || state == NULL) {
        return;
    }

    for (uint32_t i = 0U; i < count; i++) {
        buf[i] = SignalAlgo_BiquadStep(buf[i], coef, state);
    }
}

void SignalAlgo_ApplyVibFilter(float *buf, uint32_t count,
                               uint8_t filter_mode,
                               uint8_t moving_avg_param,
                               uint16_t rpm,
                               float sample_hz,
                               float *scratch, uint32_t scratch_cap,
                               SignalAlgo_BiquadCoef_t *coef_out,
                               SignalAlgo_BiquadState_t *state)
{
    if (buf == NULL || count == 0U) {
        return;
    }

    if (filter_mode == SIGNAL_ALGO_FILTER_SMOOTH) {
        uint32_t win = SignalAlgo_MovingAverageWinSize(moving_avg_param);
        if (win > 1U) {
            SignalAlgo_MovingAverageInPlace(buf, count, win, scratch, scratch_cap);
        }
        return;
    }

    if (filter_mode == SIGNAL_ALGO_FILTER_RPM) {
        if (coef_out == NULL || state == NULL) {
            return;
        }
        if (SignalAlgo_BandpassDesign(rpm, sample_hz,
                                      SIGNAL_ALGO_DEFAULT_RPM_BW_PCT, coef_out)) {
            SignalAlgo_BandpassInPlace(buf, count, coef_out, state);
        }
    }
}

/* ==================== 峰峰值 ==================== */

float SignalAlgo_PeakPeak(const float *samples, uint32_t count)
{
    if (samples == NULL || count == 0U) {
        return 0.0f;
    }

    float vmin = samples[0];
    float vmax = samples[0];

    for (uint32_t i = 1U; i < count; i++) {
        if (samples[i] < vmin) {
            vmin = samples[i];
        }
        if (samples[i] > vmax) {
            vmax = samples[i];
        }
    }

    return vmax - vmin;
}

uint16_t SignalAlgo_PeakPeakWindowed(const float *samples, uint32_t count,
                                     uint8_t cycle_multiplier,
                                     float *pp_out, uint16_t pp_cap)
{
    uint16_t num_win = (uint16_t)cycle_multiplier + 1U;

    if (samples == NULL || count == 0U || pp_out == NULL || pp_cap < num_win) {
        return 0U;
    }

    uint32_t win_size = count / (uint32_t)num_win;
    if (win_size == 0U) {
        win_size = 1U;
    }

    for (uint16_t w = 0U; w < num_win; w++) {
        uint32_t start = (uint32_t)w * win_size;
        uint32_t end = (w == (uint16_t)(num_win - 1U))
                       ? count
                       : (start + win_size);

        float vmax = samples[start];
        float vmin = samples[start];
        for (uint32_t i = start + 1U; i < end; i++) {
            if (samples[i] > vmax) {
                vmax = samples[i];
            }
            if (samples[i] < vmin) {
                vmin = samples[i];
            }
        }
        pp_out[w] = vmax - vmin;
    }

    return num_win;
}

uint32_t SignalAlgo_CycleSampleCount(uint16_t min_cycle_duration_s,
                                     uint16_t sample_freq_hz)
{
    return (uint32_t)min_cycle_duration_s * (uint32_t)sample_freq_hz;
}

uint32_t SignalAlgo_SubWindowSampleCount(uint16_t min_cycle_duration_s,
                                         uint8_t cycle_multiplier,
                                         uint16_t sample_freq_hz)
{
    uint32_t total = SignalAlgo_CycleSampleCount(min_cycle_duration_s,
                                                 sample_freq_hz);
    uint32_t num_win = (uint32_t)cycle_multiplier + 1U;
    if (num_win == 0U) {
        return 0U;
    }
    uint32_t win = total / num_win;
    return (win > 0U) ? win : 1U;
}

/* ==================== 显示映射 ==================== */

float SignalAlgo_MapVibPpToChartY(float pp_engineering, float sensitivity,
                                  float fullscale_mult)
{
    if (sensitivity < 0.01f) {
        sensitivity = 20.0f;
    }
    if (fullscale_mult < 0.01f) {
        fullscale_mult = SIGNAL_ALGO_VIB_FULLSCALE_MULT;
    }

    float denom = sensitivity * fullscale_mult;
    float y = pp_engineering / denom * 100.0f;

    if (y < 0.0f) {
        return 0.0f;
    }
    if (y > 100.0f) {
        return 100.0f;
    }
    return y;
}

/* ==================== 包络 ==================== */

void SignalAlgo_EnvelopeFromBaseline(const float *baseline, uint32_t count,
                                     uint8_t upper_pct, uint8_t lower_pct,
                                     float *upper_out, float *lower_out)
{
    if (baseline == NULL || upper_out == NULL || lower_out == NULL || count == 0U) {
        return;
    }

    float upper_factor = 1.0f + (float)upper_pct / 100.0f;
    float lower_factor = 1.0f - (float)lower_pct / 100.0f;
    if (lower_factor < 0.0f) {
        lower_factor = 0.0f;
    }

    for (uint32_t i = 0U; i < count; i++) {
        upper_out[i] = baseline[i] * upper_factor;
        lower_out[i] = baseline[i] * lower_factor;
    }
}

float SignalAlgo_LinearInterp(const float *y, uint32_t n, float index_f)
{
    if (y == NULL || n < 2U) {
        return 0.0f;
    }

    if (index_f <= 0.0f) {
        return y[0];
    }
    if (index_f >= (float)(n - 1U)) {
        return y[n - 1U];
    }

    uint32_t j = (uint32_t)index_f;
    float frac = index_f - (float)j;
    return y[j] * (1.0f - frac) + y[j + 1U] * frac;
}

bool SignalAlgo_EnvelopeAtChartIdx(const float *upper, const float *lower,
                                   uint32_t n,
                                   uint16_t chart_idx, uint16_t chart_points,
                                   float norm_scale,
                                   float *upper_y, float *lower_y)
{
    if (upper == NULL || lower == NULL || n < 2U || chart_points < 2U) {
        return false;
    }

    float t = (float)chart_idx / (float)(chart_points - 1U) * (float)(n - 1U);
    float u = SignalAlgo_LinearInterp(upper, n, t);
    float l = SignalAlgo_LinearInterp(lower, n, t);

    if (upper_y != NULL) {
        *upper_y = u * norm_scale;
    }
    if (lower_y != NULL) {
        *lower_y = l * norm_scale;
    }
    return true;
}

/* ==================== 超限占比 ==================== */

float SignalAlgo_OverLimitRatio(const float *values,
                                const float *upper,
                                const float *lower,
                                uint32_t count)
{
    if (values == NULL || upper == NULL || lower == NULL || count == 0U) {
        return 0.0f;
    }

    uint32_t over = 0U;
    for (uint32_t i = 0U; i < count; i++) {
        if (values[i] > upper[i] || values[i] < lower[i]) {
            over++;
        }
    }

    return (float)over / (float)count * 100.0f;
}
