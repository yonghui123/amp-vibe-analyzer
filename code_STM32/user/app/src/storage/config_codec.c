/**
 * @file    config_codec.c
 */
#include "storage/config_codec.h"
#include "storage/json_kv.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int ConfigCodec_ChannelsToJson(const ChannelConfig_t *configs, uint8_t count,
                               char *buf, int buf_size)
{
    int pos = 0;
    uint8_t i;

    if (configs == NULL || buf == NULL || buf_size <= 0) {
        return -1;
    }

    pos += snprintf(buf + pos, (size_t)(buf_size - pos),
                    "{\n  \"version\": 1,\n  \"channels\": [\n");

    for (i = 0; i < count; i++) {
        const ChannelConfig_t *c = &configs[i];
        pos += snprintf(buf + pos, (size_t)(buf_size - pos),
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
            pos += snprintf(buf + pos, (size_t)(buf_size - pos),
                "      \"alarm_threshold\": %.2f,\n"
                "      \"alarm_delay_ms\": %u\n",
                (double)c->params.general.alarm_threshold,
                c->params.general.alarm_delay_ms);
        } else {
            pos += snprintf(buf + pos, (size_t)(buf_size - pos),
                "      \"sensitivity\": %.4f,\n"
                "      \"suggested_value\": %.4f,\n"
                "      \"alarm_value\": %.4f\n",
                (double)c->params.vibration.sensitivity,
                (double)c->params.vibration.suggested_value,
                (double)c->params.vibration.alarm_value);
        }

        pos += snprintf(buf + pos, (size_t)(buf_size - pos),
                        "    }%s\n", (i + 1U < count) ? "," : "");
        if (pos >= buf_size - 8) {
            return -1;
        }
    }

    pos += snprintf(buf + pos, (size_t)(buf_size - pos), "  ]\n}\n");
    return pos;
}

bool ConfigCodec_ChannelsFromJson(const char *json,
                                  ChannelConfig_t *configs, uint8_t max_count,
                                  uint8_t *out_count)
{
    const char *arr;
    const char *obj_start;
    uint8_t n = 0;

    if (json == NULL || configs == NULL || max_count == 0 || out_count == NULL) {
        return false;
    }

    arr = strstr(json, "\"channels\"");
    if (arr == NULL) {
        return false;
    }
    arr = strchr(arr, '[');
    if (arr == NULL) {
        return false;
    }
    arr++;

    memset(configs, 0, (size_t)max_count * sizeof(ChannelConfig_t));
    obj_start = arr;
    while ((obj_start = strchr(obj_start, '{')) != NULL) {
        const char *obj_end;
        ChannelConfig_t *c;
        char name_tmp[21];

        obj_start++;
        obj_end = strchr(obj_start, '}');
        if (obj_end == NULL) {
            break;
        }
        if (n >= max_count) {
            break;
        }

        c = &configs[n];
        c->channel_id  = (uint8_t)JsonKv_GetInt(obj_start, "id", (int)(n + 1));
        c->channel_num = (uint8_t)JsonKv_GetInt(obj_start, "num", (int)(n + 1));
        c->enabled     = JsonKv_GetBool(obj_start, "enabled", true) ? 1U : 0U;
        c->ch_type     = (uint8_t)JsonKv_GetInt(obj_start, "type", 0);
        JsonKv_GetString(obj_start, "name", name_tmp, (int)sizeof(name_tmp));
        strncpy(c->channel_name, name_tmp, sizeof(c->channel_name) - 1U);
        c->unit_index     = (uint8_t)JsonKv_GetInt(obj_start, "unit", 0);
        c->sample_freq_hz = (uint16_t)JsonKv_GetInt(obj_start, "freq", 30);
        c->coefficient    = JsonKv_GetFloat(obj_start, "coeff", 1.0f);
        c->offset         = JsonKv_GetFloat(obj_start, "offset", 0.0f);

        if (c->ch_type == CH_TYPE_GENERAL) {
            c->params.general.alarm_threshold =
                JsonKv_GetFloat(obj_start, "alarm_threshold", 50.0f);
            c->params.general.alarm_delay_ms =
                (uint16_t)JsonKv_GetInt(obj_start, "alarm_delay_ms", 1000);
        } else {
            c->params.vibration.sensitivity =
                JsonKv_GetFloat(obj_start, "sensitivity", 20.0f);
            c->params.vibration.suggested_value =
                JsonKv_GetFloat(obj_start, "suggested_value", 0.50f);
            c->params.vibration.alarm_value =
                JsonKv_GetFloat(obj_start, "alarm_value", 1.00f);
        }

        n++;
        obj_start = obj_end + 1;
    }

    *out_count = n;
    return (n > 0U);
}

int ConfigCodec_AcqToJson(const AcqParams_t *p, char *buf, int buf_size)
{
    if (p == NULL || buf == NULL || buf_size <= 0) {
        return -1;
    }
    return snprintf(buf, (size_t)buf_size,
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
}

bool ConfigCodec_AcqFromJson(const char *json, AcqParams_t *params)
{
    if (json == NULL || params == NULL) {
        return false;
    }
    if (JsonKv_Find(json, "min_cycle_duration_s") == NULL &&
        JsonKv_Find(json, "vibration_ch_id") == NULL) {
        return false;
    }

    params->min_cycle_duration_s =
        (uint16_t)JsonKv_GetInt(json, "min_cycle_duration_s",
                                params->min_cycle_duration_s);
    params->vibration_ch_num =
        (uint8_t)JsonKv_GetInt(json, "vibration_ch_id", params->vibration_ch_num);
    params->general_ch_num =
        (uint8_t)JsonKv_GetInt(json, "general_ch_id", params->general_ch_num);
    params->filter_mode =
        (uint8_t)JsonKv_GetInt(json, "filter_mode", params->filter_mode);
    params->rpm = (uint16_t)JsonKv_GetInt(json, "rpm", params->rpm);
    params->cycle_multiplier =
        (uint8_t)JsonKv_GetInt(json, "cycle_multiplier", params->cycle_multiplier);
    params->moving_average =
        (uint8_t)JsonKv_GetInt(json, "moving_average", params->moving_average);
    params->general_threshold =
        JsonKv_GetFloat(json, "general_threshold", params->general_threshold);
    params->threshold_time_s =
        JsonKv_GetFloat(json, "threshold_time_s", params->threshold_time_s);
    params->alarm_threshold_pct =
        (uint8_t)JsonKv_GetInt(json, "alarm_threshold_pct",
                               params->alarm_threshold_pct);
    params->envelope_upper_pct =
        (uint8_t)JsonKv_GetInt(json, "envelope_upper_pct",
                               params->envelope_upper_pct);
    params->envelope_lower_pct =
        (uint8_t)JsonKv_GetInt(json, "envelope_lower_pct",
                               params->envelope_lower_pct);
    JsonKv_GetString(json, "envelope_file_path",
                     params->envelope_file_path,
                     (int)sizeof(params->envelope_file_path));
    return true;
}
