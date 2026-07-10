/**
 * @file    acq_pipeline.c
 * @brief   八通道采集管线：校准、路由、滤波、峰峰值、帧组装
 *
 * ============================================================================
 * 一、整体数据流（每 ACQ_UI_UPDATE_MS ≈ 20ms 执行一次 ProcessFrame）
 * ============================================================================
 *
 *   [硬件/模拟器]
 *        │
 *        ├─ 通用通道路径（低频显示）──────────────────────────────────────┐
 *        │   AD7606_GetLatestRaw() 读 8 路「最新快照」                      │
 *        │   → pipeline_calibrate() 逐路: val = raw×coeff + offset         │
 *        │   → pipeline_fill_general() 按路由表填入 general_vals[]         │
 *        │                                                                 │
 *        └─ 振动通道路径（高频采样 + 低频显示）────────────────────────────┤
 *            AD7606 中断 @1kHz → AcqPipeline_OnAd7606Sample()            │
 *            → 仅校准 vibration_ch_num 一路 → vib_ring_push()             │
 *            ProcessFrame 时:                                              │
 *            → vib_ring_drain_window() 取出上一 UI 周期样本               │
 *            → pipeline_apply_moving_average() 可选滑动平均                 │
 *            → AcqPipeline_CalcPeakPeak()  PP = max − min                 │
 *            → pipeline_vib_pp_to_chart_y()  映射到图表 Y 0~100           │
 *            → GUI_GetVibEnvelopeAtSeq()     查 CSV 包络参考线              │
 *                                                                          │
 *        Alarm_ProcessFrame() 【阶段 8 桩，当前恒不报警】                    │
 *                                                                          │
 * ============================================================================
 * 二、与 PRD 的差异（待后续迭代，见功能实现状态对比清单 §7）
 * ============================================================================
 *   - 7.3~7.7 算法已抽离至 signal_algo.c（纯函数）；本文件只做路由与缓冲编排。
 *   - 实时显示仍用 20ms 单窗 PP；PRD 5.1 分窗版由 SignalAlgo_PeakPeakWindowed 提供。
 */

#include "acq_pipeline.h"
#include "alarm_manager.h"
#include "channel_data.h"
#include "config_store.h"
#include "config.h"
#include "signal_algo.h"
#include <string.h>

#include "gui_core.h"

#if defined(PC_SIMULATOR)
#include <math.h>
#else
#include "bsp_ad7606.h"
#endif

/* ==================== 内部类型 ==================== */

/**
 * 通用通道路由表项：把「逻辑通道号」映射到 raw 数组下标与折线图序列号。
 *
 * 例：通道表中有 CH3、CH5 为已启用通用通道，则：
 *   s_gen_route[0] = {channel_id=3, phys_index=2, series_index=0}
 *   s_gen_route[1] = {channel_id=5, phys_index=4, series_index=1}
 * general_vals[0]、general_vals[1] 分别对应主监控图第 1、2 条折线。
 */
typedef struct {
    uint8_t channel_id;    /**< 逻辑通道号 1~8 */
    uint8_t phys_index;    /**< AD7606 raw 数组下标 0~7（= channel_id − 1） */
    uint8_t series_index;  /**< 主监控通用图折线序号 0..N-1 */
} GenRouteEntry_t;

/* ==================== 静态变量：路由 ==================== */

static GenRouteEntry_t s_gen_route[MAX_GENERAL_CHANNELS];
static uint8_t         s_gen_route_count;   /**< 当前有效通用路由条数 */
static uint8_t         s_vib_ch_id;         /**< 采集设置选定的振动通道号 */
static uint8_t         s_trigger_gen_id;    /**< 采集设置选定的通用参考通道（阶段 8 报警用） */
static AcqParams_t     s_acq_params;        /**< 采集设置参数缓存 */
static bool            s_pipeline_inited;

static bool            s_vib_route_valid;   /**< 振动路由是否有效 */
static uint8_t         s_vib_phys_index;    /**< 振动通道物理下标 0~7 */

/* ==================== 静态变量：振动环形缓冲 ==================== */

/**
 * 振动高频环形缓冲设计说明：
 *
 *   - 容量 ACQ_VIB_RING_SIZE，存校准后的 float 样本。
 *   - s_vib_ring_wpos 单调递增（不在模运算处回绕指针本身），
 *     实际槽位 = wpos % ACQ_VIB_RING_SIZE，这样可区分「写过的总样本数」
 *     与「缓冲内物理位置」，便于计算 avail = wpos − rpos。
 *   - 写端：AD7606 中断或 PC 模拟注入（@1kHz）。
 *   - 读端：仅 ProcessFrame 任务上下文调用 drain_window。
 *   - s_vib_ui_seq：每输出一个 PP 点 +1，用于与 CSV 包络时间轴对齐。
 */
static float             s_vib_ring[ACQ_VIB_RING_SIZE];
static volatile uint32_t s_vib_ring_wpos;
static uint32_t          s_vib_ring_rpos;
static volatile bool     s_vib_capture_enabled;
static uint32_t          s_vib_ui_seq;

/** 转速带通 IIR 系数与状态（由调用方持有，算法模块无全局变量） */
static SignalAlgo_BiquadCoef_t  s_biquad_coef;
static SignalAlgo_BiquadState_t s_biquad_state;

#if defined(PC_SIMULATOR)
static uint32_t          s_pc_sim_tick;
static uint32_t          s_pc_sim_sub_tick;
#endif

/* ==================== 内部函数声明 ==================== */

static bool     pipeline_read_raw(int16_t raw[8]);
static void     pipeline_calibrate(const int16_t raw[8], float calibrated[8]);
static float    pipeline_calibrate_one(uint8_t phys, int16_t raw);
static void     pipeline_fill_general(const float calibrated[8], AcqDisplayFrame_t *out);
static void     pipeline_fill_vibration_pp(AcqDisplayFrame_t *out);
static void     vib_ring_push(float sample);
static uint32_t vib_ring_drain_window(float *out, uint32_t max_count);
static float    pipeline_get_vib_sample_hz(void);
#if defined(PC_SIMULATOR)
static void     pipeline_pc_sim_feed_vib_samples(void);
static void     pipeline_pc_gen_raw(int16_t raw[8], uint32_t tick);
#endif

/* ==================== 振动环形缓冲 ==================== */

/**
 * @brief  向环形缓冲写入一个振动校准样本
 *
 * 调用上下文：AD7606 中断（板端）或 PC 模拟注入循环。
 * 若采集未运行或振动路由无效，直接丢弃，避免 Stop 后残留写入。
 */
static void vib_ring_push(float sample)
{
    if (!s_vib_capture_enabled || !s_vib_route_valid) {
        return;
    }

    uint32_t w = s_vib_ring_wpos;
    s_vib_ring[w % ACQ_VIB_RING_SIZE] = sample;
    s_vib_ring_wpos = w + 1U;  /* 总写入计数 +1，不回绕 */
}

/**
 * @brief  取出「自上次 ProcessFrame 以来」累积的样本，作为本帧 PP 计算窗口
 *
 * 执行步骤：
 *   1. avail = wpos − rpos，表示自上次消费后新写入的样本数。
 *   2. 若 avail > max_count（消费滞后，如 GUI 卡顿）：
 *        丢弃最旧数据，只保留最近 max_count 个点，rpos = wpos − max_count。
 *        这样 PP 仍反映「最近一个 UI 周期」而非过期积压。
 *   3. 按时间顺序拷贝到 out[]，更新 rpos = wpos。
 *
 * @param  out        输出数组，长度至少 max_count
 * @param  max_count  本次最多取出的样本数（通常为 ACQ_VIB_WINDOW_MAX）
 * @return 实际拷贝的样本个数；0 表示尚无新样本
 */
static uint32_t vib_ring_drain_window(float *out, uint32_t max_count)
{
    if (out == NULL || max_count == 0U) {
        return 0U;
    }

    uint32_t w = s_vib_ring_wpos;
    uint32_t r = s_vib_ring_rpos;
    uint32_t avail = w - r;

    if (avail == 0U) {
        return 0U;
    }

    if (avail > max_count) {
        /* 滞后过多：滑动窗口，只保留最新 max_count 个样本 */
        r = w - max_count;
    }

    uint32_t n = w - r;
    for (uint32_t i = 0U; i < n; i++) {
        out[i] = s_vib_ring[(r + i) % ACQ_VIB_RING_SIZE];
    }

    s_vib_ring_rpos = w;
    return n;
}

/* ==================== 校准 ==================== */

/**
 * @brief  单路 ADC 原始值 → 工程值
 *
 * 公式（PRD 3.1.2）：工程值 = 原始值 × 系数 + 偏移
 *
 * @param  phys  物理下标 0~7，对应 channel_id = phys + 1
 * @param  raw   AD7606 有符号 16 位采样值
 */
static float pipeline_calibrate_one(uint8_t phys, int16_t raw)
{
    if (phys >= 8U) {
        return (float)raw;
    }

    ChannelConfig_t *ch = ChannelData_GetById((uint8_t)(phys + 1U));
    if (ch != NULL) {
        return SignalAlgo_Calibrate(raw, ch->coefficient, ch->offset);
    }
    return (float)raw;
}

/** 振动通道采样率（Hz），用于 IIR 设计；无效时回退 ACQ_DEFAULT_ADC_HZ */
static float pipeline_get_vib_sample_hz(void)
{
    ChannelConfig_t *ch = ChannelData_GetById(s_vib_ch_id);
    if (ch != NULL && ch->sample_freq_hz > 0U) {
        return (float)ch->sample_freq_hz;
    }
    return (float)ACQ_DEFAULT_ADC_HZ;
}

/**
 * @brief  读取 8 路最新 ADC 快照
 *
 * 板端：AD7606_GetLatestRaw — 由采集任务周期性更新的缓冲，非中断内逐点读。
 * PC端：生成正弦模拟波形，前 4 路模拟通用、后 4 路模拟振动。
 */
static bool pipeline_read_raw(int16_t raw[8])
{
    if (raw == NULL) {
        return false;
    }

#if defined(PC_SIMULATOR)
    s_pc_sim_tick++;
    pipeline_pc_gen_raw(raw, s_pc_sim_tick);
    return true;
#else
    return AD7606_GetLatestRaw(raw);
#endif
}

#if defined(PC_SIMULATOR)

/** PC 模拟：通用通道类脉冲波形 + 振动通道 PP 连续变化 */
static void pipeline_pc_gen_raw(int16_t raw[8], uint32_t tick)
{
    float t_sec = (float)tick * 0.001f;

    for (uint8_t i = 0; i < 8U; i++) {
        if (i < 4U) {
            float phase = (float)i * 1.2f;
            float carrier = sinf(t_sec * 6.2831853f * 1.5f + phase);
            /* 周期性尖峰，模拟连续采集下的脉冲类波形 */
            float pulse = (fmodf(t_sec * 1.1f + (float)i * 0.17f, 1.0f) < 0.08f)
                          ? 28.0f : 0.0f;
            raw[i] = (int16_t)(50.0f + 22.0f * carrier + pulse);
        } else {
            float pp_chart = 52.0f
                           + 14.0f * sinf(t_sec * 0.35f)
                           +  5.0f * sinf(t_sec * 1.3f + 0.4f);
            float pp_eng = pp_chart * (20.0f * SIGNAL_ALGO_VIB_FULLSCALE_MULT)
                           / 100.0f;
            float amp = pp_eng * 0.5f;
            float vphase = (float)(i - 4U) * 0.7f;
            raw[i] = (int16_t)(amp * sinf(t_sec * 31.415926f + vphase));
        }
    }
}

/**
 * @brief  PC 模拟 1kHz 采样：在一个 UI 周期内注入多份振动样本
 *
 * 计算：samples_per_frame = ADC_HZ × ACQ_UI_UPDATE_MS / 1000
 * 例：1000Hz × 20ms / 1000 = 20 个样本/帧，与板端中断频率语义一致。
 */
static void pipeline_pc_sim_feed_vib_samples(void)
{
    if (!s_vib_capture_enabled || !s_vib_route_valid) {
        return;
    }

    uint32_t samples_per_frame = (ACQ_DEFAULT_ADC_HZ * (uint32_t)ACQ_UI_UPDATE_MS)
                                 / 1000U;
    if (samples_per_frame == 0U) {
        samples_per_frame = 1U;
    }

    for (uint32_t s = 0U; s < samples_per_frame; s++) {
        int16_t raw[8];
        pipeline_pc_gen_raw(raw, s_pc_sim_sub_tick);
        s_pc_sim_sub_tick++;

        float val = pipeline_calibrate_one(s_vib_phys_index,
                                           raw[s_vib_phys_index]);
        vib_ring_push(val);
    }
}

#endif /* PC_SIMULATOR */

/** 批量校准 8 路 raw → calibrated[8] */
static void pipeline_calibrate(const int16_t raw[8], float calibrated[8])
{
    for (uint8_t phys = 0; phys < 8U; phys++) {
        calibrated[phys] = pipeline_calibrate_one(phys, raw[phys]);
    }
}

/* ==================== 振动 PP（调用 signal_algo 纯函数） ==================== */

float AcqPipeline_CalcPeakPeak(const float *samples, uint32_t count)
{
    return SignalAlgo_PeakPeak(samples, count);
}

/* ==================== 填帧 ==================== */

/**
 * @brief  按路由表把校准值填入 general_vals[]
 *
 * 只收集「已启用 + 类型=通用」的通道；顺序与主监控折线序号一致。
 */
static void pipeline_fill_general(const float calibrated[8], AcqDisplayFrame_t *out)
{
    out->general_count = s_gen_route_count;

    for (uint8_t i = 0; i < s_gen_route_count; i++) {
        uint8_t phys = s_gen_route[i].phys_index;
        if (phys < 8U) {
            out->general_vals[s_gen_route[i].series_index] = calibrated[phys];
        }
    }
}

/**
 * @brief  计算本帧振动峰峰值，并查询 CSV 包络参考值
 *
 * 逐步执行：
 *   1. 若振动路由无效 → 清零 vib_valid/env_valid 并返回。
 *   2. PC 模拟：先注入 1kHz 样本（板端由中断 push，此步跳过）。
 *   3. vib_ring_drain_window → 得到上一 UI 周期的 float 样本数组。
 *   4. SignalAlgo_ApplyVibFilter → 平滑/转速 IIR（纯函数，状态在 s_biquad_state）
 *   5. SignalAlgo_PeakPeak → 工程单位 PP
 *   6. SignalAlgo_MapVibPpToChartY → 图表 Y 0~100
 *   7. GUI_GetVibEnvelopeAtSeq(s_vib_ui_seq) → 按 PP 点序号查 CSV 包络上下界。
 *   8. s_vib_ui_seq++，与振动图 X 轴推进同步。
 *
 * 与通用通道区别：general_vals 是「瞬时值」，vib_pp 是「一段时间内的幅度」。
 */
static void pipeline_fill_vibration_pp(AcqDisplayFrame_t *out)
{
    out->vib_pp     = 0.0f;
    out->env_upper  = 0.0f;
    out->env_lower  = 0.0f;
    out->vib_valid  = 0U;
    out->env_valid  = 0U;

    if (!s_vib_route_valid) {
        return;
    }

#if defined(PC_SIMULATOR)
    pipeline_pc_sim_feed_vib_samples();
#endif

    float window[ACQ_VIB_WINDOW_MAX];
    float scratch[ACQ_VIB_WINDOW_MAX];
    uint32_t n = vib_ring_drain_window(window, ACQ_VIB_WINDOW_MAX);
    if (n == 0U) {
        return;  /* 尚无新样本（刚 Start 或采样未就绪） */
    }

    SignalAlgo_ApplyVibFilter(window, n,
                              s_acq_params.filter_mode,
                              s_acq_params.moving_average,
                              s_acq_params.rpm,
                              pipeline_get_vib_sample_hz(),
                              scratch, ACQ_VIB_WINDOW_MAX,
                              &s_biquad_coef, &s_biquad_state);

    float pp_eng = SignalAlgo_PeakPeak(window, n);

    float sensitivity = 20.0f;
    ChannelConfig_t *vib_ch = ChannelData_GetById(s_vib_ch_id);
    if (vib_ch != NULL && vib_ch->ch_type == CH_TYPE_VIBRATION) {
        sensitivity = vib_ch->params.vibration.sensitivity;
    }

    out->vib_pp = SignalAlgo_MapVibPpToChartY(pp_eng, sensitivity,
                                              SIGNAL_ALGO_VIB_FULLSCALE_MULT);
    out->vib_valid = 1U;

    {
        float eu = 0.0f;
        float el = 0.0f;
        if (GUI_GetVibEnvelopeAtSeq(s_vib_ui_seq, &eu, &el)) {
            out->env_upper = eu;
            out->env_lower = el;
            out->env_valid = 1U;
        }
    }

    s_vib_ui_seq++;
}

/* ==================== 对外 API ==================== */

void AcqPipeline_Init(void)
{
    Alarm_Init();

    s_acq_params = Config_LoadAcqParams();
    s_pipeline_inited = true;

    AcqPipeline_ResetVibCapture();
    AcqPipeline_RebuildRoute();
}

/** 清零环形缓冲指针与 UI 序号，避免跨次采集残留 */
void AcqPipeline_ResetVibCapture(void)
{
    s_vib_ring_wpos = 0U;
    s_vib_ring_rpos = 0U;
    s_vib_ui_seq    = 0U;
    SignalAlgo_BiquadReset(&s_biquad_state);
#if defined(PC_SIMULATOR)
    s_pc_sim_sub_tick = 0U;
    s_pc_sim_tick     = 0U;
#endif
}

/**
 * @brief  控制振动高频写入开关
 *
 * enable=true（Acq_Start）：允许中断 push。
 * enable=false（Acq_Stop）：禁止 push，并将 rpos 追到 wpos 丢弃积压。
 */
void AcqPipeline_SetVibCaptureEnabled(bool enable)
{
    s_vib_capture_enabled = enable;
    if (!enable) {
        s_vib_ring_rpos = s_vib_ring_wpos;
    }
}

/**
 * @brief  根据通道设置与采集设置重建路由缓存
 *
 * 扫描 ChannelData 全部 8 路：
 *   - enabled && CH_TYPE_GENERAL → 追加到 s_gen_route[]
 *   - AcqParams.vibration_ch_num → 校验后设置 s_vib_route_valid
 *   - AcqParams.general_ch_num   → 存入 s_trigger_gen_id（阶段 8 二级报警用）
 *
 * 调用时机：Init、Start、通道 Save、采集 Apply。
 */
void AcqPipeline_RebuildRoute(void)
{
    s_gen_route_count = 0U;
    s_vib_ch_id       = 0U;
    s_trigger_gen_id  = 0U;
    s_vib_route_valid = false;
    s_vib_phys_index  = 0U;

    s_acq_params = Config_LoadAcqParams();
    SignalAlgo_BiquadReset(&s_biquad_state);
    s_vib_ch_id      = s_acq_params.vibration_ch_num;
    s_trigger_gen_id = s_acq_params.general_ch_num;

    uint8_t total = ChannelData_GetCount();
    for (uint8_t i = 0;
         i < total && s_gen_route_count < MAX_GENERAL_CHANNELS;
         i++) {
        ChannelConfig_t *ch = ChannelData_GetByIndex(i);
        if (ch == NULL || !ch->enabled) {
            continue;
        }
        if (ch->ch_type != CH_TYPE_GENERAL) {
            continue;
        }
        if (ch->channel_id < CHANNEL_ID_BASE ||
            ch->channel_id > CHANNEL_DATA_MAX) {
            continue;
        }

        GenRouteEntry_t *entry = &s_gen_route[s_gen_route_count];
        entry->channel_id   = ch->channel_id;
        entry->phys_index   = (uint8_t)(ch->channel_id - CHANNEL_ID_BASE);
        entry->series_index = s_gen_route_count;
        s_gen_route_count++;
    }

    if (s_vib_ch_id >= CHANNEL_ID_BASE && s_vib_ch_id <= CHANNEL_DATA_MAX) {
        ChannelConfig_t *vib_ch = ChannelData_GetById(s_vib_ch_id);
        if (vib_ch != NULL && vib_ch->enabled &&
            vib_ch->ch_type == CH_TYPE_VIBRATION) {
            s_vib_phys_index  = (uint8_t)(s_vib_ch_id - CHANNEL_ID_BASE);
            s_vib_route_valid = true;
        }
    }

    Alarm_NotifyConfigChanged();
}

/** AD7606 每次 8 路采样完成回调：仅 push 选定振动通道的校准值 */
void AcqPipeline_OnAd7606Sample(const int16_t raw[AD7606_CHANNEL_COUNT])
{
    if (raw == NULL || !s_vib_capture_enabled || !s_vib_route_valid) {
        return;
    }
    if (s_vib_phys_index >= AD7606_CHANNEL_COUNT) {
        return;
    }

    float val = pipeline_calibrate_one(s_vib_phys_index,
                                       raw[s_vib_phys_index]);
    vib_ring_push(val);
}

#if !defined(PC_SIMULATOR)
/** 覆盖 bsp_ad7606.c 弱符号，将硬件采样钩子接到本管线 */
void AD7606_SampleHook(const int16_t raw[AD7606_CHANNEL_COUNT])
{
    AcqPipeline_OnAd7606Sample(raw);
}
#endif

/**
 * @brief  采集管线主入口：组装一帧 AcqDisplayFrame_t
 *
 * 调用方：AcqTask 每 ACQ_UI_UPDATE_MS 调用一次。
 *
 * 流程：
 *   读 raw → 校准 → 填通用值 → 算振动 PP → 填 rpm → 报警占位
 *
 * @return false 仅当 out==NULL 或板端尚未读到有效 ADC 快照
 */
bool AcqPipeline_ProcessFrame(AcqDisplayFrame_t *out)
{
    if (out == NULL) {
        return false;
    }

    if (!s_pipeline_inited) {
        AcqPipeline_Init();
    }

    memset(out, 0, sizeof(AcqDisplayFrame_t));

    int16_t raw[8];
    if (!pipeline_read_raw(raw)) {
        return false;
    }

    float calibrated[8];
    pipeline_calibrate(raw, calibrated);

    pipeline_fill_general(calibrated, out);
    pipeline_fill_vibration_pp(out);

    out->rpm = s_acq_params.rpm;

    /* 阶段 8：在此判定 general_vals / vib_pp 是否触发报警 */
    Alarm_ProcessFrame(out, ACQ_UI_UPDATE_MS);

    return true;
}
