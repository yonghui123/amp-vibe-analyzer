/**
 * @file    alarm_manager.c
 * @brief   报警管理实现（阶段 8）
 *
 * 结构：
 *   - 纯函数：防抖单步、CheckGeneral/CheckVibration、状态合并
 *   - 模块内 static 状态：累计计时、锁存、PP 历史
 *   - Alarm_ProcessFrame：编排 + GUI_PostAlarm*（Acq 线程安全）
 */

#include "alarm_manager.h"
#include "acq_pipeline.h"
#include "signal_algo.h"
#include "channel_data.h"
#include "config_store.h"
#include "gui_core.h"
#include "data_logger.h"
#include <string.h>

/* ==================== 常量 ==================== */

#define ALARM_PP_HIST_MAX   101U   /* cycle_multiplier 最大 100 → 101 点 */

/* ==================== 内部类型 ==================== */

typedef enum {
    ALARM_PHASE_NORMAL = 0,
    ALARM_PHASE_TRIGGERING,
    ALARM_PHASE_ALARM
} AlarmPhase_t;

typedef struct {
    uint32_t over_accum_ms;
    bool     latched;
    AlarmPhase_t phase;
} AlarmGenChState_t;

typedef struct {
    uint32_t trigger_accum_ms;
    bool     gate_open;
    AlarmPhase_t phase;
} AlarmVibTriggerState_t;

typedef struct {
    float    pp[ALARM_PP_HIST_MAX];
    float    upper[ALARM_PP_HIST_MAX];
    float    lower[ALARM_PP_HIST_MAX];
    uint16_t count;
    bool     latched;
} AlarmVibEnvelopeState_t;

/* ==================== 模块状态 ==================== */

SystemStatus_t g_system_status = STATUS_NORMAL;
bool g_buzzer_enabled = true;
bool g_general_alarm_active = false;
bool g_vibration_alarm_active = false;

static AlarmGenChState_t        s_gen_ch[CHANNEL_DATA_MAX];
static AlarmVibTriggerState_t   s_vib_trigger;
static AlarmVibEnvelopeState_t  s_vib_env;

static bool     s_prev_general_latched;
static bool     s_prev_vibration_latched;

/** 最近锁存的一级报警通道（弹窗用） */
static uint8_t  s_last_gen_alarm_ch;
static char     s_last_gen_alarm_name[21];
static float    s_last_gen_alarm_val;
static float    s_last_gen_alarm_thresh;

static char     s_last_vib_alarm_name[21];
static float    s_last_vib_alarm_val;
static float    s_last_vib_alarm_thresh;

/* ==================== 纯函数 ==================== */

/**
 * @brief  单通道防抖：回落清零，累计达 required_ms 则锁存
 */
static void alarm_debounce_tick(uint32_t *accum_ms, bool is_over,
                                uint32_t dt_ms, uint32_t required_ms,
                                bool *latched, AlarmPhase_t *phase)
{
    if (*latched) {
        *phase = ALARM_PHASE_ALARM;
        return;
    }

    if (!is_over) {
        *accum_ms = 0U;
        *phase = ALARM_PHASE_NORMAL;
        return;
    }

    *accum_ms += dt_ms;
    if (*accum_ms >= required_ms) {
        *latched = true;
        *phase = ALARM_PHASE_ALARM;
    } else {
        *phase = ALARM_PHASE_TRIGGERING;
    }
}

static AlarmType_t alarm_merge_type(bool general, bool vibration)
{
    if (general && vibration) {
        return ALARM_TYPE_DUAL;
    }
    if (general) {
        return ALARM_TYPE_GENERAL;
    }
    if (vibration) {
        return ALARM_TYPE_VIBRATION;
    }
    return ALARM_TYPE_NONE;
}

static SystemStatus_t alarm_merge_status(bool any_triggering, bool any_alarming)
{
    if (any_alarming) {
        return STATUS_ALARMING;
    }
    if (any_triggering) {
        return STATUS_TRIGGERING;
    }
    return STATUS_NORMAL;
}

static bool alarm_get_general_value(const AcqDisplayFrame_t *frame,
                                    uint8_t channel_id, float *out)
{
    if (frame == NULL || out == NULL) {
        return false;
    }

    uint8_t series = 0U;
    uint8_t total  = ChannelData_GetCount();

    for (uint8_t i = 0U; i < total; i++) {
        ChannelConfig_t *ch = ChannelData_GetByIndex(i);
        if (ch == NULL || !ch->enabled || ch->ch_type != CH_TYPE_GENERAL) {
            continue;
        }
        if (ch->channel_id == channel_id) {
            if (series >= frame->general_count) {
                return false;
            }
            *out = frame->general_vals[series];
            return true;
        }
        series++;
    }
    return false;
}

/* ==================== 对外纯判定 API ==================== */

bool Alarm_CheckGeneral(float current_value, float threshold,
                        uint32_t over_duration_ms, uint32_t required_ms)
{
    return (current_value >= threshold) && (over_duration_ms >= required_ms);
}

bool Alarm_CheckVibration(const float *peak_to_peak,
                          const float *upper_envelope,
                          const float *lower_envelope,
                          uint32_t count, uint32_t threshold_percent)
{
    if (peak_to_peak == NULL || upper_envelope == NULL ||
        lower_envelope == NULL || count == 0U) {
        return false;
    }

    float ratio = SignalAlgo_OverLimitRatio(peak_to_peak, upper_envelope,
                                            lower_envelope, count);
    return ratio >= (float)threshold_percent;
}

float Alarm_CalcOverLimitRatio(const float *peak_to_peak,
                               const float *upper_envelope,
                               const float *lower_envelope, uint32_t count)
{
    return SignalAlgo_OverLimitRatio(peak_to_peak, upper_envelope,
                                     lower_envelope, count);
}

/* ==================== 一级：通用通道 ==================== */

static void alarm_process_general(const AcqDisplayFrame_t *frame, uint32_t dt_ms,
                                  bool *any_triggering, bool *any_latched)
{
    uint8_t total = ChannelData_GetCount();
    uint8_t series = 0U;

    for (uint8_t i = 0U; i < total; i++) {
        ChannelConfig_t *ch = ChannelData_GetByIndex(i);
        if (ch == NULL || !ch->enabled || ch->ch_type != CH_TYPE_GENERAL) {
            continue;
        }
        if (series >= frame->general_count) {
            break;
        }

        float value = frame->general_vals[series];
        series++;

        uint8_t idx = (uint8_t)(ch->channel_id - CHANNEL_ID_BASE);
        if (idx >= CHANNEL_DATA_MAX) {
            continue;
        }

        AlarmGenChState_t *st = &s_gen_ch[idx];
        float threshold = ch->params.general.alarm_threshold;
        uint32_t delay_ms = ch->params.general.alarm_delay_ms;
        if (delay_ms == 0U) {
            delay_ms = 1000U;
        }

        bool is_over = (value >= threshold);
        alarm_debounce_tick(&st->over_accum_ms, is_over, dt_ms, delay_ms,
                            &st->latched, &st->phase);

        if (st->phase == ALARM_PHASE_TRIGGERING) {
            *any_triggering = true;
        }
        if (st->latched) {
            *any_latched = true;
            s_last_gen_alarm_ch    = ch->channel_id;
            s_last_gen_alarm_val   = value;
            s_last_gen_alarm_thresh = threshold;
            strncpy(s_last_gen_alarm_name, ch->channel_name,
                    sizeof(s_last_gen_alarm_name) - 1U);
            s_last_gen_alarm_name[sizeof(s_last_gen_alarm_name) - 1U] = '\0';
        }
    }
}

/* ==================== 二级：振动包络 ==================== */

static void alarm_process_vibration(const AcqDisplayFrame_t *frame,
                                    uint32_t dt_ms,
                                    const AcqParams_t *params,
                                    bool *any_triggering, bool *any_latched)
{
    float ref_val = 0.0f;
    if (!alarm_get_general_value(frame, params->general_ch_num, &ref_val)) {
        ref_val = 0.0f;
    }

    bool ref_over = (ref_val >= params->general_threshold);
    uint32_t trigger_ms = (uint32_t)(params->threshold_time_s * 1000.0f);
    if (trigger_ms < 100U) {
        trigger_ms = 100U;
    }

    if (!ref_over) {
        s_vib_trigger.trigger_accum_ms = 0U;
        s_vib_trigger.gate_open = false;
        s_vib_trigger.phase = ALARM_PHASE_NORMAL;
        if (!s_vib_env.latched) {
            s_vib_env.count = 0U;
        }
    } else if (!s_vib_trigger.gate_open) {
        s_vib_trigger.trigger_accum_ms += dt_ms;
        if (s_vib_trigger.trigger_accum_ms >= trigger_ms) {
            s_vib_trigger.gate_open = true;
            s_vib_trigger.phase = ALARM_PHASE_NORMAL;
        } else {
            s_vib_trigger.phase = ALARM_PHASE_TRIGGERING;
            *any_triggering = true;
        }
    }

    if (s_vib_env.latched) {
        *any_latched = true;
        return;
    }

    if (!s_vib_trigger.gate_open) {
        return;
    }

    if (!frame->vib_valid) {
        return;
    }

    uint16_t win = (uint16_t)params->cycle_multiplier + 1U;
    if (win < 2U) {
        win = 2U;
    }
    if (win > ALARM_PP_HIST_MAX) {
        win = ALARM_PP_HIST_MAX;
    }

    if (s_vib_env.count < win) {
        uint16_t n = s_vib_env.count;
        s_vib_env.pp[n] = frame->vib_pp;
        if (frame->env_valid) {
            s_vib_env.upper[n] = frame->env_upper;
            s_vib_env.lower[n] = frame->env_lower;
        } else {
            s_vib_env.upper[n] = 100.0f;
            s_vib_env.lower[n] = 0.0f;
        }
        s_vib_env.count++;
    } else {
        memmove(s_vib_env.pp, &s_vib_env.pp[1],
                (size_t)(win - 1U) * sizeof(float));
        memmove(s_vib_env.upper, &s_vib_env.upper[1],
                (size_t)(win - 1U) * sizeof(float));
        memmove(s_vib_env.lower, &s_vib_env.lower[1],
                (size_t)(win - 1U) * sizeof(float));
        s_vib_env.pp[win - 1U]    = frame->vib_pp;
        s_vib_env.upper[win - 1U] = frame->env_valid ? frame->env_upper : 100.0f;
        s_vib_env.lower[win - 1U] = frame->env_valid ? frame->env_lower : 0.0f;
        s_vib_env.count = win;
    }

    if (s_vib_env.count >= win &&
        Alarm_CheckVibration(s_vib_env.pp, s_vib_env.upper, s_vib_env.lower,
                             s_vib_env.count, params->alarm_threshold_pct)) {
        s_vib_env.latched = true;

        ChannelConfig_t *vch = ChannelData_GetById(params->vibration_ch_num);
        if (vch != NULL) {
            strncpy(s_last_vib_alarm_name, vch->channel_name,
                    sizeof(s_last_vib_alarm_name) - 1U);
        } else {
            strncpy(s_last_vib_alarm_name, "Vibration",
                    sizeof(s_last_vib_alarm_name) - 1U);
        }
        s_last_vib_alarm_name[sizeof(s_last_vib_alarm_name) - 1U] = '\0';
        s_last_vib_alarm_val    = frame->vib_pp;
        s_last_vib_alarm_thresh = (float)params->alarm_threshold_pct;
        *any_latched = true;
    }
}

/* ==================== GUI 联动 ==================== */

static void alarm_notify_gui(bool popup_edge)
{
    GUI_PostAlarmStatus((uint8_t)g_system_status,
                        g_general_alarm_active,
                        g_vibration_alarm_active);

    if (!popup_edge) {
        return;
    }

    AlarmType_t type = alarm_merge_type(g_general_alarm_active,
                                        g_vibration_alarm_active);
    if (type == ALARM_TYPE_NONE) {
        return;
    }

    {
        AlarmRecord_t rec;
        if (type == ALARM_TYPE_DUAL) {
            Alarm_CreateRecord(&rec, ALARM_TYPE_DUAL, 0U, "Dual Alarm",
                               s_last_gen_alarm_val, s_last_gen_alarm_thresh);
            (void)DataLogger_SaveAlarm(&rec);
            GUI_PostAlarmPopup((uint8_t)ALARM_TYPE_DUAL, "Dual Alarm",
                               s_last_gen_alarm_val, s_last_gen_alarm_thresh);
            return;
        }
        if (type == ALARM_TYPE_GENERAL) {
            Alarm_CreateRecord(&rec, ALARM_TYPE_GENERAL, 0U,
                               s_last_gen_alarm_name,
                               s_last_gen_alarm_val, s_last_gen_alarm_thresh);
            (void)DataLogger_SaveAlarm(&rec);
            GUI_PostAlarmPopup((uint8_t)ALARM_TYPE_GENERAL,
                               s_last_gen_alarm_name,
                               s_last_gen_alarm_val, s_last_gen_alarm_thresh);
            return;
        }
        Alarm_CreateRecord(&rec, ALARM_TYPE_VIBRATION, 0U,
                           s_last_vib_alarm_name,
                           s_last_vib_alarm_val, s_last_vib_alarm_thresh);
        (void)DataLogger_SaveAlarm(&rec);
        GUI_PostAlarmPopup((uint8_t)ALARM_TYPE_VIBRATION,
                           s_last_vib_alarm_name,
                           s_last_vib_alarm_val, s_last_vib_alarm_thresh);
    }
}

/* ==================== 对外 API ==================== */

void Alarm_Init(void)
{
    memset(s_gen_ch, 0, sizeof(s_gen_ch));
    memset(&s_vib_trigger, 0, sizeof(s_vib_trigger));
    memset(&s_vib_env, 0, sizeof(s_vib_env));

    g_system_status = STATUS_NORMAL;
    g_buzzer_enabled = true;
    g_general_alarm_active = false;
    g_vibration_alarm_active = false;
    s_prev_general_latched = false;
    s_prev_vibration_latched = false;
}

void Alarm_NotifyConfigChanged(void)
{
    for (uint8_t i = 0U; i < CHANNEL_DATA_MAX; i++) {
        s_gen_ch[i].over_accum_ms = 0U;
        if (!s_gen_ch[i].latched) {
            s_gen_ch[i].phase = ALARM_PHASE_NORMAL;
        }
    }

    if (!s_vib_trigger.gate_open) {
        s_vib_trigger.trigger_accum_ms = 0U;
        s_vib_trigger.phase = ALARM_PHASE_NORMAL;
    }

    if (!s_vib_env.latched) {
        s_vib_env.count = 0U;
    }
}

void Alarm_ProcessFrame(const AcqDisplayFrame_t *frame, uint32_t dt_ms)
{
    if (frame == NULL || dt_ms == 0U) {
        return;
    }

    AcqParams_t params = Config_LoadAcqParams();

    bool any_gen_triggering = false;
    bool any_gen_latched    = false;
    bool any_vib_latched    = false;

    alarm_process_general(frame, dt_ms, &any_gen_triggering, &any_gen_latched);
    alarm_process_vibration(frame, dt_ms, &params,
                            &any_gen_triggering, &any_vib_latched);

    g_general_alarm_active   = any_gen_latched;
    g_vibration_alarm_active = any_vib_latched;

    bool any_alarming = g_general_alarm_active || g_vibration_alarm_active;
    g_system_status = alarm_merge_status(any_gen_triggering, any_alarming);

    bool gen_edge = g_general_alarm_active && !s_prev_general_latched;
    bool vib_edge = g_vibration_alarm_active && !s_prev_vibration_latched;
    bool popup_edge = gen_edge || vib_edge;

    alarm_notify_gui(popup_edge);

    s_prev_general_latched   = g_general_alarm_active;
    s_prev_vibration_latched = g_vibration_alarm_active;

    (void)g_buzzer_enabled;
}

SystemStatus_t Alarm_GetStatus(void)
{
    return g_system_status;
}

void Alarm_Reset(void)
{
    Alarm_Init();
    GUI_PostAlarmStatus((uint8_t)STATUS_NORMAL, false, false);
}

void Alarm_SetBuzzer(bool enabled)
{
    g_buzzer_enabled = enabled;
}

void Alarm_CreateRecord(AlarmRecord_t *record, AlarmType_t type,
                        uint8_t ch_num, const char *ch_name,
                        float cur_val, float thresh)
{
    if (record == NULL) {
        return;
    }
    memset(record, 0, sizeof(AlarmRecord_t));
    record->type = type;
    record->channel_num = ch_num;
    record->current_value = cur_val;
    record->threshold_value = thresh;
    if (ch_name != NULL) {
        strncpy(record->channel_name, ch_name, sizeof(record->channel_name) - 1U);
    }
}
