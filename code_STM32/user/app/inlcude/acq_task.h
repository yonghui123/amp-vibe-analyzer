/**
 * @file    acq_task.h
 * @brief   采集任务对外接口
 *
 * 职责：
 *   - 管理采集启停（与主监控 Start/Stop 按钮对接）
 *   - 阶段 2 起在 Init 中初始化采集管线
 *   - 阶段 3 起在独立任务（或 PC 主循环 Poll）中周期调用
 *     AcqPipeline_ProcessFrame 并推送 GUI_UpdateRealtimeData
 *
 * 阶段说明：
 *   - 阶段 0：仅 Start/Stop 运行标志
 *   - 阶段 2：Init 调用 AcqPipeline_Init
 *   - 阶段 3：FreeRTOS AcqTask 线程 / PC 模拟器 AcqTask_Poll
 */

#ifndef __ACQ_TASK_H
#define __ACQ_TASK_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  初始化采集任务模块
 *
 * STM32：创建 AcqTask 线程（栈 2KB，优先级 Normal，低于 GUI）。
 * PC 模拟器：仅初始化管线，由主循环调用 AcqTask_Poll。
 */
void AcqTask_Init(void);

/**
 * @brief  开始采集
 *
 * 重建路由、启动 AD7606 连续采样（PC 模拟器无硬件），置运行标志。
 * 由 UI「Start Acq」按钮调用。
 */
void Acq_Start(void);

/**
 * @brief  停止采集
 *
 * 清除运行标志、停止 AD7606。由 UI「Stop Acq」按钮调用。
 */
void Acq_Stop(void);

/**
 * @brief  查询采集是否正在运行
 * @return true  已 Start 且未 Stop
 */
bool Acq_IsRunning(void);

/**
 * @brief  获取已成功处理的 UI 帧计数
 *
 * 每 20ms 成功 ProcessFrame 一次约 +1，Start 后约 50 次/秒（T3.1 验收用）。
 */
uint32_t Acq_GetFrameCount(void);

/**
 * @brief  获取 ProcessFrame 失败次数
 *
 * 典型原因：板端 Start 后首帧 ADC 尚未就绪，或 Stop 后残留调用。
 */
uint32_t Acq_GetFrameFailCount(void);

#if defined(PC_SIMULATOR)
/**
 * @brief  PC 模拟器主循环轮询入口
 *
 * 无 FreeRTOS 时在 main while(1) 中调用；内部按 ACQ_UI_UPDATE_MS 节流。
 */
void AcqTask_Poll(void);
#endif

#endif /* __ACQ_TASK_H */
