/**
 * @file    config_store.c
 * @brief   配置持久化：内存缓存 + sd_fs JSON（通道 / 采集参数）
 */
#include "config_store.h"
#include "storage/config_codec.h"
#include "storage/path_layout.h"
#include "storage/sd_fs.h"

#include <stdio.h>
#include <string.h>

#define CFG_JSON_BUF_CH   4096
#define CFG_JSON_BUF_ACQ  1536

static ChannelConfig_t s_channels[MAX_TOTAL_CHANNELS];
static AcqParams_t     s_acq_params;
static uint8_t         s_channel_count;
static bool            s_initialized;
static char            s_json_ch_buf[CFG_JSON_BUF_CH];

static const char *s_default_names[] = {
    "Current_A", "Current_B", "Current_C", "Temp_1",
    "Vib_X", "Vib_Y", "Vib_Z", "Vib_R"
};

static void fill_default_channels(ChannelConfig_t *configs, uint8_t *count)
{
    uint8_t i;
    *count = MAX_TOTAL_CHANNELS;
    for (i = 0; i < MAX_TOTAL_CHANNELS; i++) {
        memset(&configs[i], 0, sizeof(ChannelConfig_t));
        configs[i].channel_id  = i + 1U;
        configs[i].channel_num = i + 1U;
        configs[i].enabled = 1U;
        configs[i].unit_index = 0U;
        configs[i].sample_freq_hz = (i < MAX_GENERAL_CHANNELS) ? 30U : 1000U;
        configs[i].coefficient = 1.0f;
        configs[i].offset = 0.0f;
        strncpy(configs[i].channel_name, s_default_names[i],
                sizeof(configs[i].channel_name) - 1U);
        if (i < MAX_GENERAL_CHANNELS) {
            configs[i].ch_type = CH_TYPE_GENERAL;
            configs[i].params.general.alarm_threshold = 50.0f;
            configs[i].params.general.alarm_delay_ms = 1000U;
        } else {
            configs[i].ch_type = CH_TYPE_VIBRATION;
            configs[i].params.vibration.sensitivity = 20.0f;
            configs[i].params.vibration.suggested_value = 0.50f;
            configs[i].params.vibration.alarm_value = 1.00f;
        }
    }
}

static void fill_default_acq(AcqParams_t *p)
{
    memset(p, 0, sizeof(*p));
    p->min_cycle_duration_s = 10U;
    p->vibration_ch_num = 5U;
    p->general_ch_num = 1U;
    p->filter_mode = 0U;
    p->rpm = 1500U;
    p->cycle_multiplier = 20U;
    p->moving_average = 10U;
    p->general_threshold = 0.0f;
    p->threshold_time_s = 1.0f;
    p->alarm_threshold_pct = 80U;
    p->envelope_upper_pct = 20U;
    p->envelope_lower_pct = 20U;
#if defined(PC_SIMULATOR)
    strncpy(p->envelope_file_path, "Envelope/envelope_test.csv",
            sizeof(p->envelope_file_path) - 1U);
#endif
}

static bool load_channels_file(void)
{
    int32_t n;
    uint8_t count = 0;

    if (!SdFs_IsReady()) {
        return false;
    }

    n = SdFs_ReadAll(PATH_FILE_CHANNEL_CONFIG, s_json_ch_buf,
                     (uint32_t)sizeof(s_json_ch_buf) - 1U);
    if (n <= 0) {
        return false;
    }
    s_json_ch_buf[n] = '\0';
    if (!ConfigCodec_ChannelsFromJson(s_json_ch_buf, s_channels,
                                      MAX_TOTAL_CHANNELS, &count)) {
        return false;
    }
    s_channel_count = count;
    return true;
}

static bool save_channels_file(const ChannelConfig_t *configs, uint8_t count)
{
    int len;
    int32_t wr;

    if (!SdFs_IsReady()) {
        return false;
    }
    len = ConfigCodec_ChannelsToJson(configs, count, s_json_ch_buf,
                                     (int)sizeof(s_json_ch_buf));
    if (len <= 0) {
        return false;
    }
    wr = SdFs_WriteAll(PATH_FILE_CHANNEL_CONFIG, s_json_ch_buf, (uint32_t)len);
    return (wr == len);
}

static bool load_acq_file(AcqParams_t *out)
{
    char buf[CFG_JSON_BUF_ACQ];
    int32_t n;

    if (!SdFs_IsReady()) {
        return false;
    }
    n = SdFs_ReadAll(PATH_FILE_ACQ_SETTINGS, buf, (uint32_t)sizeof(buf) - 1U);
    if (n <= 0) {
        return false;
    }
    buf[n] = '\0';
    return ConfigCodec_AcqFromJson(buf, out);
}

static bool save_acq_file(const AcqParams_t *params)
{
    char buf[CFG_JSON_BUF_ACQ];
    int len;
    int32_t wr;

    if (!SdFs_IsReady()) {
        return false;
    }
    len = ConfigCodec_AcqToJson(params, buf, (int)sizeof(buf));
    if (len <= 0) {
        return false;
    }
    wr = SdFs_WriteAll(PATH_FILE_ACQ_SETTINGS, buf, (uint32_t)len);
    return (wr == len);
}

void Config_Init(void)
{
    if (s_initialized) {
        return;
    }

    fill_default_channels(s_channels, &s_channel_count);
    fill_default_acq(&s_acq_params);

    if (SdFs_IsReady()) {
        (void)load_channels_file();
        {
            AcqParams_t file_acq = s_acq_params;
            if (load_acq_file(&file_acq)) {
#if defined(PC_SIMULATOR)
                if (file_acq.envelope_file_path[0] == '\0') {
                    strncpy(file_acq.envelope_file_path,
                            "Envelope/envelope_test.csv",
                            sizeof(file_acq.envelope_file_path) - 1U);
                }
#endif
                s_acq_params = file_acq;
            }
        }
    }

    s_initialized = true;
}

uint8_t Config_LoadChannels(ChannelConfig_t *configs, uint8_t max_count)
{
    uint8_t n;
    if (!s_initialized) {
        Config_Init();
    }
    if (configs == NULL || max_count == 0U) {
        return 0U;
    }
    n = (s_channel_count < max_count) ? s_channel_count : max_count;
    memcpy(configs, s_channels, (size_t)n * sizeof(ChannelConfig_t));
    return n;
}

bool Config_SaveChannels(const ChannelConfig_t *configs, uint8_t count)
{
    if (configs == NULL || count == 0U || count > MAX_TOTAL_CHANNELS) {
        return false;
    }
    if (!s_initialized) {
        Config_Init();
    }
    memcpy(s_channels, configs, (size_t)count * sizeof(ChannelConfig_t));
    s_channel_count = count;
    return save_channels_file(s_channels, s_channel_count);
}

AcqParams_t Config_LoadAcqParams(void)
{
    if (!s_initialized) {
        Config_Init();
    }
    return s_acq_params;
}

bool Config_SaveAcqParams(const AcqParams_t *params)
{
    if (params == NULL) {
        return false;
    }
    if (!s_initialized) {
        Config_Init();
    }
    s_acq_params = *params;
    return save_acq_file(&s_acq_params);
}

void Config_RestoreDefaults(void)
{
    fill_default_channels(s_channels, &s_channel_count);
    fill_default_acq(&s_acq_params);
    s_initialized = true;
    (void)save_channels_file(s_channels, s_channel_count);
    (void)save_acq_file(&s_acq_params);
}

void Config_GetDefaultChannels(ChannelConfig_t *configs, uint8_t *count)
{
    if (configs == NULL || count == NULL) {
        return;
    }
    fill_default_channels(configs, count);
}

void Config_GetDefaultAcqParams(AcqParams_t *params)
{
    if (params == NULL) {
        return;
    }
    fill_default_acq(params);
}
