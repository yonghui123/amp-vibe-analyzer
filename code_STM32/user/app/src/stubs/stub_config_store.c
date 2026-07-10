/**
 * @file    stub_config_store.c
 * @brief   Config_Store 桩模块 - 用于 PC 模拟器和 STM32 初期集成
 *
 * 通道配置存储在静态内存中 (由 channel_data.c 管理 JSON 持久化)
 * 采集参数 (AcqParams) 通过 JSON 文件持久化: Config/acq_settings.json
 */

#include "config_store.h"
#include <stdio.h>
#include <string.h>

/* POSIX 文件系统操作仅在 PC 模拟器下可用 */
#ifdef PC_SIMULATOR
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#define MKDIR(path) mkdir(path, 0755)
#endif

/* 采集参数 JSON 文件路径 */
#define ACQ_JSON_PATH  "Config/acq_settings.json"
#endif /* PC_SIMULATOR */

/* 静态存储 */
static ChannelConfig_t s_channels[MAX_TOTAL_CHANNELS];
static AcqParams_t s_acq_params;
static uint8_t s_channel_count = 0;
static bool s_initialized = false;

/* 默认通道名 */
static const char *s_default_names[] = {
    "Current_A", "Current_B", "Current_C", "Temp_1",
    "Vib_X", "Vib_Y", "Vib_Z", "Vib_R"
};

static void fill_defaults(void)
{
    s_channel_count = MAX_TOTAL_CHANNELS;

    for (uint8_t i = 0; i < MAX_TOTAL_CHANNELS; i++) {
        memset(&s_channels[i], 0, sizeof(ChannelConfig_t));
        s_channels[i].channel_id  = i + 1;
        s_channels[i].channel_num = i + 1;
        s_channels[i].enabled = 1;
        s_channels[i].unit_index = 0;
        s_channels[i].sample_freq_hz = (i < MAX_GENERAL_CHANNELS) ? 30 : 1000;
        s_channels[i].coefficient = 1.0f;
        s_channels[i].offset = 0.0f;

        strncpy(s_channels[i].channel_name, s_default_names[i],
                sizeof(s_channels[i].channel_name) - 1);

        if (i < MAX_GENERAL_CHANNELS) {
            s_channels[i].ch_type = CH_TYPE_GENERAL;
            s_channels[i].params.general.alarm_threshold = 50.0f;
            s_channels[i].params.general.alarm_delay_ms = 1000;
        } else {
            s_channels[i].ch_type = CH_TYPE_VIBRATION;
            s_channels[i].params.vibration.sensitivity = 20.0f;
            s_channels[i].params.vibration.suggested_value = 0.50f;
            s_channels[i].params.vibration.alarm_value = 1.00f;
        }
    }

    memset(&s_acq_params, 0, sizeof(AcqParams_t));
    s_acq_params.min_cycle_duration_s = 10;
    s_acq_params.vibration_ch_num = 5;  /* 默认 Vib_X（channel 5，振动类型） */
    s_acq_params.general_ch_num = 1;
    s_acq_params.filter_mode = 0;
    s_acq_params.rpm = 1500;
    s_acq_params.cycle_multiplier = 20;
    s_acq_params.moving_average = 10;
    s_acq_params.general_threshold = 0.0f;
    s_acq_params.threshold_time_s = 1.0f;
    s_acq_params.alarm_threshold_pct = 80;
    s_acq_params.envelope_upper_pct = 20;
    s_acq_params.envelope_lower_pct = 20;
#if defined(PC_SIMULATOR)
    strncpy(s_acq_params.envelope_file_path, "Envelope/envelope_test.csv",
            sizeof(s_acq_params.envelope_file_path) - 1U);
    s_acq_params.envelope_file_path[sizeof(s_acq_params.envelope_file_path) - 1U] = '\0';
#else
    s_acq_params.envelope_file_path[0] = '\0';
#endif
}

void Config_Init(void)
{
    if (!s_initialized) {
        fill_defaults();
        s_initialized = true;
    }
}

uint8_t Config_LoadChannels(ChannelConfig_t *configs, uint8_t max_count)
{
    if (!s_initialized) fill_defaults();

    uint8_t count = (s_channel_count < max_count) ? s_channel_count : max_count;
    memcpy(configs, s_channels, count * sizeof(ChannelConfig_t));
    return count;
}

bool Config_SaveChannels(const ChannelConfig_t *configs, uint8_t count)
{
    if (count > MAX_TOTAL_CHANNELS) return false;
    memcpy(s_channels, configs, count * sizeof(ChannelConfig_t));
    s_channel_count = count;
    return true;
}

/* ==================== AcqParams JSON 序列化/反序列化 ==================== */

#ifdef PC_SIMULATOR

/**
 * @brief  将采集参数序列化为 JSON 字符串
 *
 * JSON 格式示例:
 * {
 *   "version": 1,
 *   "min_cycle_duration_s": 10,
 *   "vibration_ch_id": 5,
 *   "general_ch_id": 1,
 *   ...
 * }
 */
static int acq_serialize_json(const AcqParams_t *p, char *buf, int buf_size)
{
    int pos = 0;
    pos += snprintf(buf + pos, buf_size - pos,
        "{\n"
        "  \"version\": 1,\n"
        "  \"min_cycle_duration_s\": %u,\n"
        "  \"vibration_ch_id\": %u,\n"
        "  \"general_ch_id\": %u,\n"
        "  \"filter_mode\": %u,\n"
        "  \"rpm\": %u,\n"
        "  \"cycle_multiplier\": %u,\n"
        "  \"moving_average\": %u,\n"
        "  \"general_threshold\": %.4f,\n"
        "  \"threshold_time_s\": %.4f,\n"
        "  \"alarm_threshold_pct\": %u,\n"
        "  \"envelope_upper_pct\": %u,\n"
        "  \"envelope_lower_pct\": %u,\n"
        "  \"envelope_file_path\": \"%s\"\n"
        "}\n",
        p->min_cycle_duration_s,
        p->vibration_ch_num,
        p->general_ch_num,
        p->filter_mode,
        p->rpm,
        p->cycle_multiplier,
        p->moving_average,
        (double)p->general_threshold,
        (double)p->threshold_time_s,
        p->alarm_threshold_pct,
        p->envelope_upper_pct,
        p->envelope_lower_pct,
        p->envelope_file_path);
    return pos;
}

/** @brief 简易 JSON key 查找 (复用 channel_data.c 同样的模式) */
static const char *acq_find_key(const char *json, const char *key)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return NULL;
    p += strlen(pattern);
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p == ':') p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

/** @brief 从 JSON 字符串中解析字符串值 (跳过引号) */
static void acq_parse_string(const char *p, char *out, int max_len)
{
    if (!p || *p != '"') { out[0] = '\0'; return; }
    p++;
    int i = 0;
    while (*p && *p != '"' && i < max_len - 1) {
        out[i++] = *p++;
    }
    out[i] = '\0';
}

/**
 * @brief  从 JSON 文件加载采集参数
 * @return true=成功加载, false=文件不存在或解析失败
 */
static bool acq_load_from_json(AcqParams_t *params)
{
    FILE *fp = fopen(ACQ_JSON_PATH, "r");
    if (!fp) return false;

    char json_buf[2048];
    size_t n = fread(json_buf, 1, sizeof(json_buf) - 1, fp);
    fclose(fp);

    if (n == 0) return false;
    json_buf[n] = '\0';

    const char *v;
    if ((v = acq_find_key(json_buf, "min_cycle_duration_s")))
        params->min_cycle_duration_s = (uint16_t)atoi(v);
    if ((v = acq_find_key(json_buf, "vibration_ch_id")))
        params->vibration_ch_num = (uint8_t)atoi(v);
    if ((v = acq_find_key(json_buf, "general_ch_id")))
        params->general_ch_num = (uint8_t)atoi(v);
    if ((v = acq_find_key(json_buf, "filter_mode")))
        params->filter_mode = (uint8_t)atoi(v);
    if ((v = acq_find_key(json_buf, "rpm")))
        params->rpm = (uint16_t)atoi(v);
    if ((v = acq_find_key(json_buf, "cycle_multiplier")))
        params->cycle_multiplier = (uint8_t)atoi(v);
    if ((v = acq_find_key(json_buf, "moving_average")))
        params->moving_average = (uint8_t)atoi(v);
    if ((v = acq_find_key(json_buf, "general_threshold")))
        params->general_threshold = (float)atof(v);
    if ((v = acq_find_key(json_buf, "threshold_time_s")))
        params->threshold_time_s = (float)atof(v);
    if ((v = acq_find_key(json_buf, "alarm_threshold_pct")))
        params->alarm_threshold_pct = (uint8_t)atoi(v);
    if ((v = acq_find_key(json_buf, "envelope_upper_pct")))
        params->envelope_upper_pct = (uint8_t)atoi(v);
    if ((v = acq_find_key(json_buf, "envelope_lower_pct")))
        params->envelope_lower_pct = (uint8_t)atoi(v);
    if ((v = acq_find_key(json_buf, "envelope_file_path")))
        acq_parse_string(v, params->envelope_file_path,
                         sizeof(params->envelope_file_path));

    printf("[ConfigStore] Loaded acq params from %s\n", ACQ_JSON_PATH);
    return true;
}

/**
 * @brief  将采集参数保存到 JSON 文件
 * @return true=成功
 */
static bool acq_save_to_json(const AcqParams_t *params)
{
    MKDIR("Config");

    char json_buf[2048];
    int len = acq_serialize_json(params, json_buf, sizeof(json_buf));
    if (len <= 0) return false;

    FILE *fp = fopen(ACQ_JSON_PATH, "w");
    if (!fp) {
        printf("[ConfigStore] ERROR: Cannot write %s\n", ACQ_JSON_PATH);
        return false;
    }

    size_t written = fwrite(json_buf, 1, (size_t)len, fp);
    fclose(fp);

    printf("[ConfigStore] Saved acq params to %s (%d bytes)\n",
           ACQ_JSON_PATH, len);
    return (written == (size_t)len);
}

#endif /* PC_SIMULATOR */

/* ==================== Public API ==================== */

AcqParams_t Config_LoadAcqParams(void)
{
    if (!s_initialized) fill_defaults();

#ifdef PC_SIMULATOR
    /* 尝试从 JSON 文件加载 (覆盖内存中的默认值) */
    AcqParams_t file_params;
    memcpy(&file_params, &s_acq_params, sizeof(AcqParams_t));
    if (acq_load_from_json(&file_params)) {
#if defined(PC_SIMULATOR)
        if (file_params.envelope_file_path[0] == '\0') {
            strncpy(file_params.envelope_file_path, "Envelope/envelope_test.csv",
                    sizeof(file_params.envelope_file_path) - 1U);
            file_params.envelope_file_path[sizeof(file_params.envelope_file_path) - 1U] = '\0';
        }
#endif
        return file_params;
    }
#endif

    return s_acq_params;
}

bool Config_SaveAcqParams(const AcqParams_t *params)
{
    memcpy(&s_acq_params, params, sizeof(AcqParams_t));

#ifdef PC_SIMULATOR
    /* 同时写入 JSON 文件, 实现重启后持久化 */
    return acq_save_to_json(params);
#else
    return true;
#endif
}

void Config_RestoreDefaults(void)
{
    fill_defaults();
}

void Config_GetDefaultChannels(ChannelConfig_t *configs, uint8_t *count)
{
    *count = MAX_TOTAL_CHANNELS;
    for (uint8_t i = 0; i < MAX_TOTAL_CHANNELS; i++) {
        memset(&configs[i], 0, sizeof(ChannelConfig_t));
        configs[i].channel_id  = i + 1;
        configs[i].channel_num = i + 1;
        configs[i].enabled = 1;
        configs[i].unit_index = 0;
        configs[i].sample_freq_hz = (i < MAX_GENERAL_CHANNELS) ? 30 : 1000;
        configs[i].coefficient = 1.0f;
        strncpy(configs[i].channel_name, s_default_names[i],
                sizeof(configs[i].channel_name) - 1);
        if (i < MAX_GENERAL_CHANNELS) {
            configs[i].ch_type = CH_TYPE_GENERAL;
            configs[i].params.general.alarm_threshold = 50.0f;
            configs[i].params.general.alarm_delay_ms = 1000;
        } else {
            configs[i].ch_type = CH_TYPE_VIBRATION;
            configs[i].params.vibration.sensitivity = 20.0f;
            configs[i].params.vibration.suggested_value = 0.50f;
            configs[i].params.vibration.alarm_value = 1.00f;
        }
    }
}

void Config_GetDefaultAcqParams(AcqParams_t *params)
{
    memset(params, 0, sizeof(AcqParams_t));
    params->min_cycle_duration_s = 10;
    params->vibration_ch_num = 1;
    params->general_ch_num = 1;
    params->cycle_multiplier = 20;
    params->moving_average = 10;
    params->general_threshold = 0.0f;
    params->threshold_time_s = 1.0f;
    params->alarm_threshold_pct = 80;
    params->envelope_upper_pct = 20;
    params->envelope_lower_pct = 20;
}
