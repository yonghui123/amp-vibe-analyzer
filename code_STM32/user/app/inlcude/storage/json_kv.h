/**
 * @file    json_kv.h
 * @brief   极简 JSON 键值解析（纯函数，无全局状态）
 */
#ifndef JSON_KV_H
#define JSON_KV_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 定位 "key" 后的值起始位置；失败返回 NULL */
const char *JsonKv_Find(const char *json, const char *key);

int   JsonKv_GetInt(const char *json, const char *key, int def);
float JsonKv_GetFloat(const char *json, const char *key, float def);
bool  JsonKv_GetBool(const char *json, const char *key, bool def);
void  JsonKv_GetString(const char *json, const char *key,
                       char *out, int out_max);

#ifdef __cplusplus
}
#endif

#endif /* JSON_KV_H */
