/**
 * @file    signal_algo.h
 * @brief   信号处理纯函数算法库（无全局状态、无 I/O、不依赖 GUI/配置存储）
 *
 * 设计原则：
 *   - 所有函数均为「给定输入 → 给定输出」，副作用仅体现在调用方提供的缓冲区/状态结构体上。
 *   - 滤波器状态 (SignalAlgo_BiquadState_t) 由调用方持有并传入，算法模块内部无 static 状态。
 *   - 界面、采集管线、报警模块只负责组参数、调纯函数、传结果。
 *
 * 覆盖 PRD 第 5 章：
 *   5.1 峰峰值（单窗 + 周期分窗）
 *   5.2 平滑滑动平均、转速带通 IIR
 *   5.3 包络边界（基准×百分比 + 线性插值）
 *   超限时间占比（供 4.2 振动包络报警）
 */

#ifndef __SIGNAL_ALGO_H
#define __SIGNAL_ALGO_H

#include <stdint.h>
#include <stdbool.h>

/* ==================== 滤波模式（与 AcqParams.filter_mode 一致） ==================== */

#define SIGNAL_ALGO_FILTER_NONE    0U
#define SIGNAL_ALGO_FILTER_SMOOTH  1U
#define SIGNAL_ALGO_FILTER_RPM     2U

/** 转速带通默认半带宽（%）：f0×(1±此值/100)，PRD 5.2 */
#define SIGNAL_ALGO_DEFAULT_RPM_BW_PCT   10.0f

/** 振动 PP 映射满量程倍数：Y=100 当 PP = sensitivity × 此值 */
#define SIGNAL_ALGO_VIB_FULLSCALE_MULT   4.0f

/* ==================== IIR 双二阶带通（调用方持有状态） ==================== */

/** 归一化双二阶系数（a0 已归一为 1） */
typedef struct {
    float b0, b1, b2;
    float a1, a2;
} SignalAlgo_BiquadCoef_t;

/** 直接 I 型双二阶延迟单元（跨样本/跨帧由调用方保持） */
typedef struct {
    float x1, x2;  /**< 输入 x[n-1], x[n-2] */
    float y1, y2;  /**< 输出 y[n-1], y[n-2] */
} SignalAlgo_BiquadState_t;

/* ==================== 校准 ==================== */

/**
 * @brief  ADC 原始值 → 工程值（PRD：实际值 = 原始值×系数 + 偏移）
 */
float SignalAlgo_Calibrate(int16_t raw, float coefficient, float offset);

/* ==================== 滤波 ==================== */

/**
 * @brief  PRD 滑动平均窗口长度：窗口 = 参数 + 1
 * @param  moving_avg_param  采集设置 moving_average (0~10)
 * @return 实际窗口样本数；param=0 时返回 1（单点，等效不滤波）
 */
uint32_t SignalAlgo_MovingAverageWinSize(uint8_t moving_avg_param);

/**
 * @brief  滑动平均滤波（原地修改 buf）
 *
 * 对每个 i，用 [max(0,i−win+1) .. i] 的算术平均替换 buf[i]。
 * scratch 用于暂存原序列，避免原地污染；scratch_cap ≥ count。
 *
 * @param  win_size  窗口长度，通常来自 SignalAlgo_MovingAverageWinSize()
 */
void SignalAlgo_MovingAverageInPlace(float *buf, uint32_t count, uint32_t win_size,
                                     float *scratch, uint32_t scratch_cap);

/**
 * @brief  根据转速设计 2 阶带通 IIR 系数
 *
 * 中心频率 f0 = rpm/60 (Hz)，半带宽默认 SIGNAL_ALGO_DEFAULT_RPM_BW_PCT%。
 * Q = f0 / bandwidth，标准 RBJ bandpass 双二阶。
 *
 * @param  rpm        转速 (100~100000)
 * @param  sample_hz  采样率 (Hz)，须 > 2×f0
 * @param  bw_pct_half  半带宽百分比（如 10 表示 ±10%）
 * @param  coef       输出系数
 * @return true 设计成功；rpm/sample_hz 非法或超过 Nyquist 时 false
 */
bool SignalAlgo_BandpassDesign(uint16_t rpm, float sample_hz, float bw_pct_half,
                               SignalAlgo_BiquadCoef_t *coef);

/** 清零双二阶延迟状态（换通道/改 RPM/重启采集时调用） */
void SignalAlgo_BiquadReset(SignalAlgo_BiquadState_t *state);

/**
 * @brief  单样本双二阶步进（纯函数，状态显式传入传出）
 */
float SignalAlgo_BiquadStep(float x, const SignalAlgo_BiquadCoef_t *coef,
                            SignalAlgo_BiquadState_t *state);

/**
 * @brief  对样本块做带通 IIR（原地覆盖 buf）
 */
void SignalAlgo_BandpassInPlace(float *buf, uint32_t count,
                                const SignalAlgo_BiquadCoef_t *coef,
                                SignalAlgo_BiquadState_t *state);

/**
 * @brief  按 filter_mode 对振动样本块应用滤波（组合入口，仍无内部全局状态）
 *
 * mode=0 不处理；mode=1 滑动平均；mode=2 带通 IIR（自动 design 系数到 coef_out）。
 * coef_out/state 由调用方分配；mode=2 且 design 失败时不滤波。
 */
void SignalAlgo_ApplyVibFilter(float *buf, uint32_t count,
                               uint8_t filter_mode,
                               uint8_t moving_avg_param,
                               uint16_t rpm,
                               float sample_hz,
                               float *scratch, uint32_t scratch_cap,
                               SignalAlgo_BiquadCoef_t *coef_out,
                               SignalAlgo_BiquadState_t *state);

/* ==================== 峰峰值 PRD 5.1 ==================== */

/**
 * @brief  单窗口峰峰值：max(samples) − min(samples)
 */
float SignalAlgo_PeakPeak(const float *samples, uint32_t count);

/**
 * @brief  PRD 5.1 周期分窗峰峰值
 *
 * 将 samples[0..count-1] 均分为 (cycle_multiplier+1) 窗，每窗输出一个 PP。
 * 最后一窗包含整除余数。
 *
 * @param  pp_out   输出数组，长度 ≥ cycle_multiplier+1
 * @param  pp_cap   pp_out 容量
 * @return 输出点数；参数非法返回 0
 */
uint16_t SignalAlgo_PeakPeakWindowed(const float *samples, uint32_t count,
                                     uint8_t cycle_multiplier,
                                     float *pp_out, uint16_t pp_cap);

/**
 * @brief  修整循环内总样本数 = min_cycle_duration_s × sample_freq_hz
 */
uint32_t SignalAlgo_CycleSampleCount(uint16_t min_cycle_duration_s,
                                     uint16_t sample_freq_hz);

/**
 * @brief  PRD 5.1 单个子窗样本数 = 总样本数 / (cycle_multiplier+1)
 */
uint32_t SignalAlgo_SubWindowSampleCount(uint16_t min_cycle_duration_s,
                                         uint8_t cycle_multiplier,
                                         uint16_t sample_freq_hz);

/* ==================== 显示映射（与 LVGL 无关的数值变换） ==================== */

/**
 * @brief  工程单位 PP → 图表 Y 轴 [0, 100]
 *
 * Y = pp / (sensitivity × fullscale_mult) × 100，钳位到 [0, 100]。
 */
float SignalAlgo_MapVibPpToChartY(float pp_engineering, float sensitivity,
                                  float fullscale_mult);

/* ==================== 包络 PRD 3.2 / 5.3（纯数组运算，无文件 I/O） ==================== */

/**
 * @brief  由基准折线生成上下包络数组
 *
 * upper[i] = baseline[i] × (1 + upper_pct/100)
 * lower[i] = baseline[i] × (1 − lower_pct/100)
 */
void SignalAlgo_EnvelopeFromBaseline(const float *baseline, uint32_t count,
                                     uint8_t upper_pct, uint8_t lower_pct,
                                     float *upper_out, float *lower_out);

/**
 * @brief  对离散序列 y[0..n-1] 在浮点索引 index_f 处线性插值
 *
 * index_f=0 → y[0]；index_f=n-1 → y[n-1]
 */
float SignalAlgo_LinearInterp(const float *y, uint32_t n, float index_f);

/**
 * @brief  将图表点 chart_idx 映射到包络序列并插值，再乘 norm_scale
 *
 * chart_idx 范围 [0, chart_points-1]，均匀映射到包络点索引。
 *
 * @return true 成功；n<2 或输出指针为 NULL 时 false
 */
bool SignalAlgo_EnvelopeAtChartIdx(const float *upper, const float *lower,
                                   uint32_t n,
                                   uint16_t chart_idx, uint16_t chart_points,
                                   float norm_scale,
                                   float *upper_y, float *lower_y);

/* ==================== 报警数学 ==================== */

/**
 * @brief  计算序列相对包络的超限时间占比（PRD 4.2）
 *
 * 对每个 i：若 values[i] > upper[i] 或 values[i] < lower[i]，计为超限点。
 * 返回 (超限点数 / count) × 100，范围 [0, 100]。
 */
float SignalAlgo_OverLimitRatio(const float *values,
                                const float *upper,
                                const float *lower,
                                uint32_t count);

#endif /* __SIGNAL_ALGO_H */
