/**
 * @file    acq_pipeline.h
 * @brief   八通道采集管线：校准、类型路由、滤波、峰峰值、帧组装
 *
 * 职责边界：
 *   - 读 AD7606 raw → 校准 → 按通道类型路由
 *   - 通用：瞬时值 general_vals[]（20ms 快照）
 *   - 振动：1kHz 环形缓冲 → 20ms 窗口 PP → vib_pp
 *   - 每帧末尾调用 Alarm_ProcessFrame（阶段 8 填报警逻辑）
 *
 * 算法文档：见 acq_pipeline.c 编排说明；纯函数见 signal_algo.h。
 */

#ifndef __ACQ_PIPELINE_H
#define __ACQ_PIPELINE_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#ifndef AD7606_CHANNEL_COUNT
#define AD7606_CHANNEL_COUNT    8
#endif

/**
 * @brief  一帧采集结果（推送 GUI / 报警判定）
 *
 * general_vals 顺序须与主监控界面 main_collect_enabled_general() 生成的
 * s_gen_ch_ids[] 一致，即按通道表索引顺序收集「已启用 + 通用类型」通道。
 */
typedef struct AcqDisplayFrame_s {
    float    general_vals[MAX_GENERAL_CHANNELS]; /**< 已启用通用通道校准值，下标 = 折线序列号 */
    uint8_t  general_count;                      /**< 有效通用通道条数（≤ MAX_GENERAL_CHANNELS） */
    float    vib_pp;                             /**< 选定振动通道一个 UI 周期内的峰峰值（非瞬时值） */
    float    env_upper;                          /**< 当前时刻上包络参考（0~100 显示坐标） */
    float    env_lower;                          /**< 当前时刻下包络参考（0~100 显示坐标） */
    uint16_t rpm;                                /**< 转速（来自采集设置 AcqParams.rpm） */
    uint8_t  vib_valid;                          /**< 1=本帧 vib_pp 有效（已选振动通道且缓冲有样本） */
    uint8_t  env_valid;                          /**< 1=env_upper/lower 来自 CSV 包络插值 */
} AcqDisplayFrame_t;

/**
 * @brief  初始化采集管线
 *
 * 调用时机：ChannelData_Init() 之后（通常在 AcqTask_Init 内）。
 * 执行内容：加载采集参数、初始化报警占位模块、构建路由缓存。
 */
void AcqPipeline_Init(void);

/**
 * @brief  重建路由缓存
 *
 * 在以下时机调用：
 *   - AcqPipeline_Init / Acq_Start
 *   - 通道设置 Save 成功
 *   - 采集设置 Apply 成功
 *
 * 扫描 8 路 ChannelConfig，生成：
 *   - gen_route[]：已启用通用通道 → general_vals 序列索引
 *   - vib_ch_id：AcqParams.vibration_ch_num
 *   - trigger_gen_id：AcqParams.general_ch_num（阶段 8 报警用）
 */
void AcqPipeline_RebuildRoute(void);

/**
 * @brief  处理一帧采集数据
 *
 * 流程：读 raw[8] → 校准 → 按路由填 general_vals / vib_pp → Alarm_ProcessFrame（空）
 *
 * @param  out  输出帧指针，不可为 NULL
 * @return true  成功组装一帧
 * @return false out 为 NULL，或底层尚未读到有效 ADC 快照（板端未 Start 时）
 */
bool AcqPipeline_ProcessFrame(AcqDisplayFrame_t *out);

/**
 * @brief  重置振动环形缓冲与 UI 序号
 *
 * 在 Acq_Start、Acq_Stop、RebuildRoute 时调用，避免跨次采集残留样本。
 */
void AcqPipeline_ResetVibCapture(void);

/**
 * @brief  使能/禁止振动高频样本写入环形缓冲
 * @param  enable  true=Acq 运行中；false=停止采集
 */
void AcqPipeline_SetVibCaptureEnabled(bool enable);

/**
 * @brief  AD7606 每次 8 路采样完成后的钩子（由 bsp_ad7606 弱符号调用）
 *
 * 在中断上下文执行：仅校准选定的 vibration_ch_num 并 push 环形缓冲。
 * 通用通道显示仍由 20ms ProcessFrame 读最新快照。
 *
 * @param  raw  八通道原始值，下标 0 对应 channel_id=1
 */
void AcqPipeline_OnAd7606Sample(const int16_t raw[AD7606_CHANNEL_COUNT]);

/**
 * @brief  计算样本窗口的峰峰值（max − min）
 *
 * 当前为 PRD 5.1 简化版：对调用方传入的整段窗口一次计算。
 * 完整分窗版见 signal_algo.h → SignalAlgo_PeakPeakWindowed()。
 *
 * @param  samples  校准后的振动样本数组
 * @param  count    样本个数
 * @return 峰峰值；count=0 时返回 0
 */
float AcqPipeline_CalcPeakPeak(const float *samples, uint32_t count);

#endif /* __ACQ_PIPELINE_H */
