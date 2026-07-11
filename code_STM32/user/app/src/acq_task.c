/**
 * @file    acq_task.c
 * @brief   采集任务实现
 *
 * 阶段 3：
 *   - STM32：FreeRTOS AcqTask 线程，20ms 周期 ProcessFrame → GUI 推送
 *   - PC 模拟器：AcqTask_Poll 由 main 循环调用
 *   - Start/Stop 对接 AD7606_StartRecord / StopRecord
 * 阶段 4：通用曲线实时推送
 * 阶段 5：振动 PP + 包络参考点推送
 * 阶段 6：GUI_PostRealtimeData（STM32 经 lv_async_call 线程安全）
 */

#include "acq_task.h"
#include "acq_pipeline.h"
#include "gui_core.h"
#include "config.h"
#include "config_store.h"
#include "channel_data.h"
#include "data_logger.h"
#include <string.h>

#if defined(PC_SIMULATOR)
#include "lvgl.h"
#else
#include "cmsis_os.h"
#include "bsp_ad7606.h"
#endif

/* ==================== 配置宏 ==================== */

/** AcqTask 栈大小（字节），文档要求 ≥ 2KB */
#define ACQ_TASK_STACK_BYTES    (512U * 4U)

/* ==================== 静态变量 ==================== */

/** 采集运行标志（GUI 任务写、Acq 任务读） */
static volatile bool s_acq_running = false;

/** 成功推送到 GUI 的帧计数 */
static volatile uint32_t s_frame_count = 0U;

/** ProcessFrame 失败计数 */
static volatile uint32_t s_frame_fail_count = 0U;

#if !defined(PC_SIMULATOR)

/** AcqTask 线程句柄 */
static osThreadId_t s_acq_task_handle;

/** AcqTask 静态栈（512 × 4 = 2048 字节） */
static StackType_t s_acq_task_stack[ACQ_TASK_STACK_BYTES / sizeof(StackType_t)];

/** AcqTask 静态 TCB */
static StaticTask_t s_acq_task_tcb;

#endif

#if defined(PC_SIMULATOR)

/** PC 模拟器：上次 ProcessFrame 的 lv_tick 时间戳 */
static uint32_t s_pc_last_process_ms = 0U;

#endif

/* ==================== 内部函数声明 ==================== */

#if !defined(PC_SIMULATOR)
/**
 * @brief  FreeRTOS AcqTask 线程入口
 * @param  argument  CMSIS-RTOS 预留参数，未使用
 */
static void AcqTask_Entry(void *argument);
#endif

/**
 * @brief  执行一次采集帧处理并推送 GUI
 *
 * 调用 AcqPipeline_ProcessFrame，成功则 GUI_UpdateRealtimeData。
 */
static void acq_task_process_once(void);

/**
 * @brief  将 AcqDisplayFrame_t 推送到主监控图表
 * @param  frame  本帧数据，不可为 NULL
 */
static void acq_task_push_gui(const AcqDisplayFrame_t *frame);

/* ==================== 内部函数实现 ==================== */

/**
 * @brief  将 AcqDisplayFrame_t 异步投递到 GUI（阶段 6：lv_async_call）
 */
static void acq_task_push_gui(const AcqDisplayFrame_t *frame)
{
    if (frame == NULL) {
        return;
    }

    if (frame->general_count == 0U && frame->vib_valid == 0U) {
        return;
    }

    GUI_PostRealtimeData(
        frame->general_count > 0U ? frame->general_vals : NULL,
        frame->general_count,
        frame->vib_pp,
        frame->vib_valid,
        frame->env_upper,
        frame->env_lower,
        frame->env_valid,
        frame->rpm);
}

/**
 * @brief  处理单帧：管线 ProcessFrame + 统计 + GUI 推送
 */
static void acq_task_process_once(void)
{
    AcqDisplayFrame_t frame;

    if (AcqPipeline_ProcessFrame(&frame)) {
        s_frame_count++;
        acq_task_push_gui(&frame);

        /* 低频落盘：每帧写振动 PP / 首路通用值（缓冲由 DataLogger 刷盘） */
        {
            DataPoint_t dp;
            AcqParams_t ap = Config_LoadAcqParams();
            memset(&dp, 0, sizeof(dp));
            dp.timestamp = s_frame_count; /* 会话内相对序号；绝对时间由 logger 可再扩展 */
            if (frame.vib_valid) {
                dp.channel_num = ap.vibration_ch_num;
                dp.signal_value = frame.vib_pp;
                dp.peak_to_peak = frame.vib_pp;
                (void)DataLogger_SavePoint(&dp);
            } else if (frame.general_count > 0U) {
                dp.channel_num = ap.general_ch_num;
                dp.signal_value = frame.general_vals[0];
                (void)DataLogger_SavePoint(&dp);
            }
        }
    } else {
        s_frame_fail_count++;
    }
}

#if !defined(PC_SIMULATOR)

/**
 * @brief  AcqTask 主循环：运行中每 ACQ_UI_UPDATE_MS 处理一帧
 */
static void AcqTask_Entry(void *argument)
{
    (void)argument;

    for (;;) {
        if (s_acq_running) {
            acq_task_process_once();
        }
        osDelay(ACQ_UI_UPDATE_MS);
    }
}

#endif /* !PC_SIMULATOR */

/* ==================== 对外 API ==================== */

/**
 * @brief  初始化采集任务模块
 */
void AcqTask_Init(void)
{
    s_acq_running      = false;
    s_frame_count      = 0U;
    s_frame_fail_count = 0U;

#if defined(PC_SIMULATOR)
    s_pc_last_process_ms = 0U;
#endif

    AcqPipeline_Init();

#if !defined(PC_SIMULATOR)
    static const osThreadAttr_t acq_task_attr = {
        .name       = "acqTask",
        .stack_mem  = s_acq_task_stack,
        .stack_size = sizeof(s_acq_task_stack),
        .cb_mem     = &s_acq_task_tcb,
        .cb_size    = sizeof(s_acq_task_tcb),
        /* 低于 guiTask（osPriorityAboveNormal），避免与 lv_timer_handler 争抢 */
        .priority   = (osPriority_t)osPriorityNormal,
    };

    s_acq_task_handle = osThreadNew(AcqTask_Entry, NULL, &acq_task_attr);
    if (s_acq_task_handle == NULL) {
        /* 创建失败时后续 Start 仍置标志，但不会有后台处理 */
    }
#endif
}

/**
 * @brief  启动采集
 */
void Acq_Start(void)
{
    ChannelConfig_t *ch;
    AcqParams_t ap;

    if (s_acq_running) {
        return;
    }

    AcqPipeline_RebuildRoute();
    AcqPipeline_ResetVibCapture();
    AcqPipeline_SetVibCaptureEnabled(true);
    GUI_ResetMainCharts();

    ap = Config_LoadAcqParams();
    ch = ChannelData_GetById(ap.vibration_ch_num);
    (void)DataLogger_BeginSession(ap.vibration_ch_num,
                                   (ch != NULL) ? ch->channel_name : "acq");

#if !defined(PC_SIMULATOR)
    AD7606_StartRecord(ACQ_DEFAULT_ADC_HZ);
#else
    s_pc_last_process_ms = 0U;
#endif

    s_acq_running = true;
}

/**
 * @brief  停止采集
 */
void Acq_Stop(void)
{
    if (!s_acq_running) {
        return;
    }

    s_acq_running = false;
    AcqPipeline_SetVibCaptureEnabled(false);
    DataLogger_EndSession();

#if !defined(PC_SIMULATOR)
    AD7606_StopRecord();
#endif
}

/**
 * @brief  查询采集是否运行中
 */
bool Acq_IsRunning(void)
{
    return s_acq_running;
}

/**
 * @brief  获取成功帧计数
 */
uint32_t Acq_GetFrameCount(void)
{
    return s_frame_count;
}

/**
 * @brief  获取失败帧计数
 */
uint32_t Acq_GetFrameFailCount(void)
{
    return s_frame_fail_count;
}

#if defined(PC_SIMULATOR)

/**
 * @brief  PC 模拟器主循环轮询
 *
 * 按 ACQ_UI_UPDATE_MS 节流调用 acq_task_process_once。
 */
void AcqTask_Poll(void)
{
    if (!s_acq_running) {
        return;
    }

    uint32_t now = lv_tick_get();
    if ((now - s_pc_last_process_ms) < (uint32_t)ACQ_UI_UPDATE_MS) {
        return;
    }

    s_pc_last_process_ms = now;
    acq_task_process_once();
}

#endif /* PC_SIMULATOR */
