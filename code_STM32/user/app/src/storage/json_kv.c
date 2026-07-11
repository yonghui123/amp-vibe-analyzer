/**
 * @file    json_kv.c
 */
#include "storage/json_kv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *JsonKv_Find(const char *json, const char *key)
{
    char pattern[64];
    const char *p;

    if (json == NULL || key == NULL) {
        return NULL;
    }
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(json, pattern);
    if (p == NULL) {
        return NULL;
    }
    p += strlen(pattern);
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }
    if (*p == ':') {
        p++;
    }
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }
    return p;
}

int JsonKv_GetInt(const char *json, const char *key, int def)
{
    const char *v = JsonKv_Find(json, key);
    return (v != NULL) ? atoi(v) : def;
}

float JsonKv_GetFloat(const char *json, const char *key, float def)
{
    const char *v = JsonKv_Find(json, key);
    return (v != NULL) ? (float)atof(v) : def;
}

bool JsonKv_GetBool(const char *json, const char *key, bool def)
{
    const char *v = JsonKv_Find(json, key);
    if (v == NULL) {
        return def;
    }
    return (v[0] == 't' || v[0] == 'T' || v[0] == '1');
}

void JsonKv_GetString(const char *json, const char *key,
                      char *out, int out_max)
{
    const char *p;
    int i;

    if (out == NULL || out_max <= 0) {
        return;
    }
    out[0] = '\0';
    p = JsonKv_Find(json, key);
    if (p == NULL || *p != '"') {
        return;
    }
    p++;
    i = 0;
    while (*p && *p != '"' && i < out_max - 1) {
        out[i++] = *p++;
    }
    out[i] = '\0';
}
