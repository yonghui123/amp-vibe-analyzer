/**
 * @file    channel_data.c
 * @brief   共享通道配置数据模块 - JSON 持久化实现
 */

#include "channel_data.h"
#include "acq_pipeline.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* POSIX 文件系统操作仅在 PC 模拟器下可用 */
#ifdef PC_SIMULATOR
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#define MKDIR(path) mkdir(path, 0755)
#endif
#endif /* PC_SIMULATOR */

/* ==================== 单位表 ==================== */
const char *g_units_general[]   = {"A", "kW", NULL};
const char *g_units_vibration[] = {"um", "mm/s", "m/s", "m/s2", NULL};

/* ==================== 静态数据 ==================== */
static ChannelConfig_t s_channels[CHANNEL_DATA_MAX];
static uint8_t s_channel_count = 0;
static bool s_initialized = false;

/* JSON 文件路径 (PC 模拟器) */
#ifdef PC_SIMULATOR
#define CHANNEL_JSON_PATH  "Config/channel_config.json"
#else
#define CHANNEL_JSON_PATH  "/sd/channel_config.json"  /* STM32 待适配 */
#endif

/* 默认通道名 */
static const char *s_default_names[] = {
    "Current_A", "Current_B", "Current_C", "Temp_1",
    "Vib_X", "Vib_Y", "Vib_Z", "Vib_R"
};

/* ==================== 内部函数 ==================== */

static void fill_defaults(void)
{
    s_channel_count = CHANNEL_DATA_MAX;
    memset(s_channels, 0, sizeof(s_channels));

    for (uint8_t i = 0; i < CHANNEL_DATA_MAX; i++) {
        s_channels[i].channel_id  = i + CHANNEL_ID_BASE;
        s_channels[i].channel_num = i + 1;
        s_channels[i].enabled     = 1;
        s_channels[i].unit_index  = 0;
        s_channels[i].sample_freq_hz = (i < 4) ? 30 : 1000;
        s_channels[i].coefficient = 1.0f;
        s_channels[i].offset      = 0.0f;

        strncpy(s_channels[i].channel_name, s_default_names[i],
                sizeof(s_channels[i].channel_name) - 1);

        if (i < 4) {
            s_channels[i].ch_type = CH_TYPE_GENERAL;
            s_channels[i].params.general.alarm_threshold = 50.0f;
            s_channels[i].params.general.alarm_delay_ms  = 1000;
        } else {
            s_channels[i].ch_type = CH_TYPE_VIBRATION;
            s_channels[i].params.vibration.sensitivity     = 20.0f;
            s_channels[i].params.vibration.suggested_value = 0.50f;
            s_channels[i].params.vibration.alarm_value     = 1.00f;
        }
    }
}

/**
 * @brief 将通道数据序列化为 JSON 字符串
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 写入的字节数
 */
static int serialize_json(char *buf, int buf_size)
{
    int pos = 0;
    pos += snprintf(buf + pos, buf_size - pos,
                    "{\n  \"version\": 1,\n  \"channels\": [\n");

    for (uint8_t i = 0; i < s_channel_count; i++) {
        ChannelConfig_t *c = &s_channels[i];
        pos += snprintf(buf + pos, buf_size - pos,
            "    {\n"
            "      \"id\": %u,\n"
            "      \"num\": %u,\n"
            "      \"enabled\": %s,\n"
            "      \"type\": %u,\n"
            "      \"name\": \"%s\",\n"
            "      \"unit\": %u,\n"
            "      \"freq\": %u,\n"
            "      \"coeff\": %.4f,\n"
            "      \"offset\": %.4f,\n",
            c->channel_id, c->channel_num,
            c->enabled ? "true" : "false",
            (unsigned)c->ch_type, c->channel_name,
            (unsigned)c->unit_index, c->sample_freq_hz,
            (double)c->coefficient, (double)c->offset);

        if (c->ch_type == CH_TYPE_GENERAL) {
            pos += snprintf(buf + pos, buf_size - pos,
                "      \"alarm_threshold\": %.2f,\n"
                "      \"alarm_delay_ms\": %u\n",
                (double)c->params.general.alarm_threshold,
                c->params.general.alarm_delay_ms);
        } else {
            pos += snprintf(buf + pos, buf_size - pos,
                "      \"sensitivity\": %.4f,\n"
                "      \"suggested_value\": %.4f,\n"
                "      \"alarm_value\": %.4f\n",
                (double)c->params.vibration.sensitivity,
                (double)c->params.vibration.suggested_value,
                (double)c->params.vibration.alarm_value);
        }

        pos += snprintf(buf + pos, buf_size - pos,
            "    }%s\n", (i < s_channel_count - 1) ? "," : "");
    }

    pos += snprintf(buf + pos, buf_size - pos, "  ]\n}\n");
    return pos;
}

/* 简易 JSON 解析辅助 */
static const char *find_key(const char *json, const char *key)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return NULL;
    p += strlen(pattern);
    /* Skip whitespace and colon */
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p == ':') p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

static int parse_int(const char *p)
{
    return atoi(p);
}

static float parse_float(const char *p)
{
    return (float)atof(p);
}

static bool parse_bool(const char *p)
{
    return (p && p[0] == 't');
}

static void parse_string(const char *p, char *out, int max_len)
{
    if (!p || *p != '"') { out[0] = '\0'; return; }
    p++; /* skip opening quote */
    int i = 0;
    while (*p && *p != '"' && i < max_len - 1) {
        out[i++] = *p++;
    }
    out[i] = '\0';
}

/**
 * @brief 从 JSON 字符串解析通道数据
 * @return true=成功解析
 */
static bool parse_json(const char *json)
{
    const char *arr = strstr(json, "\"channels\"");
    if (!arr) return false;

    /* Find the array start */
    arr = strchr(arr, '[');
    if (!arr) return false;
    arr++;

    s_channel_count = 0;
    memset(s_channels, 0, sizeof(s_channels));

    /* Parse each object in the array */
    const char *obj_start = arr;
    while ((obj_start = strchr(obj_start, '{')) != NULL) {
        obj_start++;
        const char *obj_end = strchr(obj_start, '}');
        if (!obj_end) break;

        if (s_channel_count >= CHANNEL_DATA_MAX) break;
        ChannelConfig_t *c = &s_channels[s_channel_count];

        /* Parse fields */
        const char *v;
        if ((v = find_key(obj_start, "id")))     c->channel_id  = (uint8_t)parse_int(v);
        if ((v = find_key(obj_start, "num")))    c->channel_num = (uint8_t)parse_int(v);
        if ((v = find_key(obj_start, "enabled"))) c->enabled    = parse_bool(v) ? 1 : 0;
        if ((v = find_key(obj_start, "type")))   c->ch_type     = (uint8_t)parse_int(v);
        if ((v = find_key(obj_start, "name")))   parse_string(v, c->channel_name, sizeof(c->channel_name));
        if ((v = find_key(obj_start, "unit")))   c->unit_index  = (uint8_t)parse_int(v);
        if ((v = find_key(obj_start, "freq")))   c->sample_freq_hz = (uint16_t)parse_int(v);
        if ((v = find_key(obj_start, "coeff")))  c->coefficient = parse_float(v);
        if ((v = find_key(obj_start, "offset"))) c->offset      = parse_float(v);

        if (c->ch_type == CH_TYPE_GENERAL) {
            if ((v = find_key(obj_start, "alarm_threshold")))
                c->params.general.alarm_threshold = parse_float(v);
            if ((v = find_key(obj_start, "alarm_delay_ms")))
                c->params.general.alarm_delay_ms = (uint16_t)parse_int(v);
        } else {
            if ((v = find_key(obj_start, "sensitivity")))
                c->params.vibration.sensitivity = parse_float(v);
            if ((v = find_key(obj_start, "suggested_value")))
                c->params.vibration.suggested_value = parse_float(v);
            if ((v = find_key(obj_start, "alarm_value")))
                c->params.vibration.alarm_value = parse_float(v);
        }

        s_channel_count++;
        obj_start = obj_end + 1;
    }

    return (s_channel_count > 0);
}

/* ==================== 公共 API ==================== */

void ChannelData_Init(void)
{
    if (s_initialized) return;

    /* 尝试从 JSON 文件加载 (仅 PC 模拟器) */
#ifdef PC_SIMULATOR
    FILE *fp = fopen(CHANNEL_JSON_PATH, "r");
    if (fp) {
        char json_buf[8192];
        size_t n = fread(json_buf, 1, sizeof(json_buf) - 1, fp);
        fclose(fp);

        if (n > 0) {
            json_buf[n] = '\0';
            if (parse_json(json_buf)) {
                printf("[ChannelData] Loaded %u channels from %s\n",
                       s_channel_count, CHANNEL_JSON_PATH);
                s_initialized = true;
                return;
            }
        }
    }
#endif /* PC_SIMULATOR */

    /* JSON 不存在或解析失败, 使用默认值 */
    fill_defaults();
    s_initialized = true;
}

uint8_t ChannelData_GetCount(void)
{
    return s_channel_count;
}

ChannelConfig_t *ChannelData_GetByIndex(uint8_t index)
{
    if (index >= s_channel_count) return NULL;
    return &s_channels[index];
}

ChannelConfig_t *ChannelData_GetById(uint8_t id)
{
    for (uint8_t i = 0; i < s_channel_count; i++) {
        if (s_channels[i].channel_id == id) return &s_channels[i];
    }
    return NULL;
}

ChannelConfig_t *ChannelData_GetAll(void)
{
    return s_channels;
}

bool ChannelData_Save(void)
{
#ifdef PC_SIMULATOR
    /* 确保目录存在 */
    MKDIR("Config");

    char json_buf[8192];
    int len = serialize_json(json_buf, sizeof(json_buf));
    if (len <= 0) return false;

    FILE *fp = fopen(CHANNEL_JSON_PATH, "w");
    if (!fp) {
        printf("[ChannelData] ERROR: Cannot write %s\n", CHANNEL_JSON_PATH);
        return false;
    }

    size_t written = fwrite(json_buf, 1, (size_t)len, fp);
    fclose(fp);

    printf("[ChannelData] Saved %u channels to %s (%d bytes)\n",
           s_channel_count, CHANNEL_JSON_PATH, len);
    if (written == (size_t)len) {
        AcqPipeline_RebuildRoute();
        return true;
    }
    return false;
#else
    /* STM32: 文件系统待适配 (SD卡 + FatFS) */
    return false;
#endif /* PC_SIMULATOR */
}

void ChannelData_ResetDefaults(void)
{
    fill_defaults();
}

const char *ChannelData_GetUnit(const ChannelConfig_t *ch)
{
    if (!ch) return "?";
    if (ch->ch_type == CH_TYPE_GENERAL) {
        return g_units_general[ch->unit_index < 2 ? ch->unit_index : 0];
    } else {
        return g_units_vibration[ch->unit_index < 4 ? ch->unit_index : 0];
    }
}
