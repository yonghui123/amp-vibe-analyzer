/**
 * @file    channel_data.c
 * @brief   通道配置内存模型；持久化统一走 Config_Store → sd_fs
 */
#include "channel_data.h"
#include "config_store.h"
#include "acq_pipeline.h"
#include <string.h>

const char *g_units_general[]   = {"A", "kW", NULL};
const char *g_units_vibration[] = {"um", "mm/s", "m/s", "m/s2", NULL};

static ChannelConfig_t s_channels[CHANNEL_DATA_MAX];
static uint8_t s_channel_count = 0;
static bool s_initialized = false;

void ChannelData_Init(void)
{
    if (s_initialized) {
        return;
    }

    Config_Init();
    s_channel_count = Config_LoadChannels(s_channels, CHANNEL_DATA_MAX);
    if (s_channel_count == 0U) {
        Config_GetDefaultChannels(s_channels, &s_channel_count);
        (void)Config_SaveChannels(s_channels, s_channel_count);
    }
    s_initialized = true;
}

uint8_t ChannelData_GetCount(void)
{
    return s_channel_count;
}

ChannelConfig_t *ChannelData_GetByIndex(uint8_t index)
{
    if (index >= s_channel_count) {
        return NULL;
    }
    return &s_channels[index];
}

ChannelConfig_t *ChannelData_GetById(uint8_t id)
{
    uint8_t i;
    for (i = 0; i < s_channel_count; i++) {
        if (s_channels[i].channel_id == id) {
            return &s_channels[i];
        }
    }
    return NULL;
}

ChannelConfig_t *ChannelData_GetAll(void)
{
    return s_channels;
}

bool ChannelData_Save(void)
{
    if (!Config_SaveChannels(s_channels, s_channel_count)) {
        return false;
    }
    AcqPipeline_RebuildRoute();
    return true;
}

void ChannelData_ResetDefaults(void)
{
    Config_GetDefaultChannels(s_channels, &s_channel_count);
    (void)Config_SaveChannels(s_channels, s_channel_count);
}

const char *ChannelData_GetUnit(const ChannelConfig_t *ch)
{
    if (ch == NULL) {
        return "?";
    }
    if (ch->ch_type == CH_TYPE_GENERAL) {
        return g_units_general[ch->unit_index < 2 ? ch->unit_index : 0];
    }
    return g_units_vibration[ch->unit_index < 4 ? ch->unit_index : 0];
}
