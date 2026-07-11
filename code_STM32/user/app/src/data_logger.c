/**
 * @file    data_logger.c
 * @brief   SD 数据记录组件（缓冲刷盘 + CSV）
 */
#include "data_logger.h"
#include "storage/path_layout.h"
#include "storage/sd_fs.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(PC_SIMULATOR)
#include <time.h>
#else
#include "stm32f4xx_hal.h"
#endif

#define DL_POINT_BUF_MAX     64U
#define DL_FLUSH_INTERVAL_MS 1000U
#define DL_ALARM_FILE        PATH_DIR_ALARMDATA "/alarms.csv"
#define DL_LINE_MAX          640
#define DL_PATH_MAX          160
#define DL_ALARM_CACHE_MAX   8U

static bool     s_inited;
static bool     s_session_open;
static char     s_session_path[DL_PATH_MAX];
static DataPoint_t s_point_buf[DL_POINT_BUF_MAX];
static uint32_t s_point_count;
static uint32_t s_last_flush_ms;
static uint32_t s_next_alarm_id;
static uint32_t s_alarm_total;
static AlarmRecord_t s_alarm_scratch[DL_ALARM_CACHE_MAX];

/* ---------- 时间 ---------- */

static uint32_t dl_now_sec(void)
{
#if defined(PC_SIMULATOR)
    return (uint32_t)time(NULL);
#else
    /* 无 RTC 时：以 2026-07-01 为软起点 + 上电秒 */
    return 1751328000UL + (HAL_GetTick() / 1000U);
#endif
}

static uint32_t dl_now_ms(void)
{
#if defined(PC_SIMULATOR)
    return (uint32_t)(time(NULL) * 1000);
#else
    return HAL_GetTick();
#endif
}

static void dl_ymd(uint32_t ts, uint16_t *y, uint8_t *m, uint8_t *d)
{
    /* 简化：按 86400 天换算自 1970，足够生成目录名 */
    uint32_t days = ts / 86400UL;
    uint32_t z = days + 719468UL;
    uint32_t era = z / 146097UL;
    uint32_t doe = z - era * 146097UL;
    uint32_t yoe = (doe - doe / 1460UL + doe / 36524UL - doe / 146096UL) / 365UL;
    uint32_t yyyy = yoe + era * 400UL;
    uint32_t doy = doe - (365UL * yoe + yoe / 4UL - yoe / 100UL);
    uint32_t mp = (5UL * doy + 2UL) / 153UL;
    uint32_t dd = doy - (153UL * mp + 2UL) / 5UL + 1UL;
    uint32_t mm = mp + ((mp < 10UL) ? 3UL : -9UL);
    yyyy += (mm <= 2UL) ? 1UL : 0UL;
    if (y) {
        *y = (uint16_t)yyyy;
    }
    if (m) {
        *m = (uint8_t)mm;
    }
    if (d) {
        *d = (uint8_t)dd;
    }
}

static void dl_sanitize_name(const char *in, char *out, size_t out_sz)
{
    size_t i = 0, j = 0;
    if (out_sz == 0U) {
        return;
    }
    if (in == NULL || in[0] == '\0') {
        strncpy(out, "ch", out_sz - 1U);
        out[out_sz - 1U] = '\0';
        return;
    }
    while (in[i] != '\0' && j + 1U < out_sz) {
        char c = in[i++];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-') {
            out[j++] = c;
        } else if (c == ' ') {
            out[j++] = '_';
        }
    }
    out[j] = '\0';
    if (j == 0U) {
        strncpy(out, "ch", out_sz - 1U);
        out[out_sz - 1U] = '\0';
    }
}

/* ---------- 点缓冲刷盘 ---------- */

static bool dl_flush_points(void)
{
    char line[128];
    uint32_t i;
    int32_t wr;
    int n;

    if (!s_session_open || s_point_count == 0U) {
        return true;
    }
    if (!SdFs_IsReady()) {
        return false;
    }

    for (i = 0; i < s_point_count; i++) {
        const DataPoint_t *dp = &s_point_buf[i];
        n = snprintf(line, sizeof(line), "%lu,%u,%.6f,%.6f,%u\n",
                     (unsigned long)dp->timestamp,
                     (unsigned)dp->channel_num,
                     (double)dp->signal_value,
                     (double)dp->peak_to_peak,
                     (unsigned)dp->flags);
        if (n <= 0) {
            continue;
        }
        wr = SdFs_Append(s_session_path, line, (uint32_t)n);
        if (wr != n) {
            return false;
        }
    }
    s_point_count = 0U;
    s_last_flush_ms = dl_now_ms();
    return true;
}

static bool dl_ensure_alarm_header(void)
{
    const char *hdr =
        "id,timestamp,type,channel_num,channel_name,current,threshold,"
        "over_duration_s,duration_s,process_status,notes\n";
    if (SdFs_Exists(DL_ALARM_FILE)) {
        return true;
    }
    return SdFs_WriteAll(DL_ALARM_FILE, hdr, (uint32_t)strlen(hdr)) > 0;
}

static bool dl_parse_alarm_line(const char *line, AlarmRecord_t *r)
{
    char name[21];
    char status[10];
    char notes[501];
    unsigned long id, ts;
    unsigned type, ch;
    float cur, thr, over_d, dur;
    int n;

    if (line == NULL || r == NULL || line[0] == '\0' || line[0] == 'i') {
        return false; /* skip header */
    }
    name[0] = status[0] = notes[0] = '\0';
    /* notes 可能含逗号：先扫前 9 字段，剩余作 notes */
    n = sscanf(line,
               "%lu,%lu,%u,%u,%20[^,],%f,%f,%f,%f,%9[^,],%500[^\n]",
               &id, &ts, &type, &ch, name, &cur, &thr, &over_d, &dur,
               status, notes);
    if (n < 9) {
        return false;
    }
    memset(r, 0, sizeof(*r));
    r->id = (uint32_t)id;
    r->timestamp = (uint32_t)ts;
    r->type = (AlarmType_t)type;
    r->channel_num = (uint8_t)ch;
    strncpy(r->channel_name, name, sizeof(r->channel_name) - 1U);
    r->current_value = cur;
    r->threshold_value = thr;
    r->over_duration_s = over_d;
    r->duration_s = dur;
    if (n >= 10) {
        strncpy(r->process_status, status, sizeof(r->process_status) - 1U);
    }
    if (n >= 11) {
        strncpy(r->notes, notes, sizeof(r->notes) - 1U);
    }
    return true;
}

static uint32_t dl_load_all_alarms(AlarmRecord_t *buf, uint32_t max_count)
{
    SdFsFile f;
    char line[DL_LINE_MAX];
    uint32_t line_len = 0;
    uint32_t n = 0;
    uint32_t max_id = 0;

    if (!SdFs_IsReady() || !SdFs_Exists(DL_ALARM_FILE)) {
        return 0U;
    }
    memset(&f, 0, sizeof(f));
    if (!SdFs_Open(&f, DL_ALARM_FILE, SD_FS_MODE_READ)) {
        return 0U;
    }

    for (;;) {
        char ch = 0;
        int32_t rd = SdFs_Read(&f, &ch, 1U);
        if (rd <= 0) {
            if (line_len > 0U && n < max_count) {
                line[line_len] = '\0';
                if (dl_parse_alarm_line(line, &buf[n])) {
                    if (buf[n].id > max_id) {
                        max_id = buf[n].id;
                    }
                    n++;
                }
            }
            break;
        }
        if (ch == '\n') {
            line[line_len] = '\0';
            if (n < max_count && dl_parse_alarm_line(line, &buf[n])) {
                if (buf[n].id > max_id) {
                    max_id = buf[n].id;
                }
                n++;
            }
            line_len = 0U;
            continue;
        }
        if (ch != '\r' && line_len + 1U < sizeof(line)) {
            line[line_len++] = ch;
        }
    }
    (void)SdFs_Close(&f);
    if (max_id + 1U > s_next_alarm_id) {
        s_next_alarm_id = max_id + 1U;
    }
    s_alarm_total = n;
    return n;
}

static bool dl_rewrite_alarms(const AlarmRecord_t *recs, uint32_t count)
{
    char line[DL_LINE_MAX];
    const char *hdr =
        "id,timestamp,type,channel_num,channel_name,current,threshold,"
        "over_duration_s,duration_s,process_status,notes\n";
    uint32_t i;
    SdFsFile f;

    if (!SdFs_IsReady()) {
        return false;
    }
    memset(&f, 0, sizeof(f));
    if (!SdFs_Open(&f, DL_ALARM_FILE, SD_FS_MODE_WRITE)) {
        return false;
    }
    if (SdFs_Write(&f, hdr, (uint32_t)strlen(hdr)) < 0) {
        (void)SdFs_Close(&f);
        return false;
    }
    for (i = 0; i < count; i++) {
        const AlarmRecord_t *r = &recs[i];
        int n = snprintf(line, sizeof(line),
            "%lu,%lu,%u,%u,%s,%.4f,%.4f,%.3f,%.3f,%s,%s\n",
            (unsigned long)r->id, (unsigned long)r->timestamp,
            (unsigned)r->type, (unsigned)r->channel_num, r->channel_name,
            (double)r->current_value, (double)r->threshold_value,
            (double)r->over_duration_s, (double)r->duration_s,
            r->process_status[0] ? r->process_status : "Open",
            r->notes);
        if (n > 0 && SdFs_Write(&f, line, (uint32_t)n) < 0) {
            (void)SdFs_Close(&f);
            return false;
        }
    }
    return SdFs_Close(&f);
}

/* ---------- Public ---------- */

void DataLogger_Init(void)
{
    if (s_inited) {
        return;
    }
    s_session_open = false;
    s_session_path[0] = '\0';
    s_point_count = 0U;
    s_last_flush_ms = 0U;
    s_next_alarm_id = 1U;
    s_alarm_total = 0U;
    if (SdFs_IsReady()) {
        (void)dl_load_all_alarms(s_alarm_scratch, DL_ALARM_CACHE_MAX);
    }
    s_inited = true;
}

bool DataLogger_BeginSession(uint8_t channel_num, const char *channel_name)
{
    uint16_t y;
    uint8_t mo, d;
    char month_dir[48];
    char safe[24];
    char hdr[96];
    uint32_t ts = dl_now_sec();
    int n;

    if (!s_inited) {
        DataLogger_Init();
    }
    if (!SdFs_IsReady()) {
        return false;
    }

    DataLogger_EndSession();

    dl_ymd(ts, &y, &mo, &d);
    if (!PathLayout_RealDataMonth(y, mo, month_dir, (uint32_t)sizeof(month_dir))) {
        return false;
    }
    if (!SdFs_EnsureDir(month_dir)) {
        return false;
    }

    dl_sanitize_name(channel_name, safe, sizeof(safe));
    n = snprintf(s_session_path, sizeof(s_session_path),
                 "%s/%lu-%s.csv", month_dir, (unsigned long)ts, safe);
    if (n <= 0 || (size_t)n >= sizeof(s_session_path)) {
        return false;
    }

    n = snprintf(hdr, sizeof(hdr),
                 "timestamp,channel,signal,peak_to_peak,flags\n");
    if (SdFs_WriteAll(s_session_path, hdr, (uint32_t)n) != n) {
        s_session_path[0] = '\0';
        return false;
    }

    (void)channel_num;
    (void)d;
    s_session_open = true;
    s_point_count = 0U;
    s_last_flush_ms = dl_now_ms();
    return true;
}

void DataLogger_EndSession(void)
{
    if (s_session_open) {
        (void)dl_flush_points();
    }
    s_session_open = false;
    s_session_path[0] = '\0';
    s_point_count = 0U;
}

bool DataLogger_Flush(void)
{
    return dl_flush_points();
}

bool DataLogger_SavePoint(const DataPoint_t *dp)
{
    if (dp == NULL || !s_session_open) {
        return false;
    }
    if (s_point_count >= DL_POINT_BUF_MAX) {
        if (!dl_flush_points()) {
            return false;
        }
    }
    s_point_buf[s_point_count++] = *dp;
    if ((dl_now_ms() - s_last_flush_ms) >= DL_FLUSH_INTERVAL_MS) {
        return dl_flush_points();
    }
    return true;
}

uint32_t DataLogger_SavePointsBulk(const DataPoint_t *dp, uint32_t count)
{
    uint32_t i;
    uint32_t ok = 0;
    if (dp == NULL) {
        return 0U;
    }
    for (i = 0; i < count; i++) {
        if (DataLogger_SavePoint(&dp[i])) {
            ok++;
        }
    }
    return ok;
}

uint32_t DataLogger_QueryHistory(uint8_t channel_num,
                                 uint32_t start_time, uint32_t end_time,
                                 DataPoint_t *buffer, uint32_t max_count)
{
    /* 首期：若有活动会话文件则读之；否则读最近一次路径失败返回 0。
     * 完整按月扫描留待后续；此处读当前会话或 Export 源不足时返回已缓冲点。 */
    uint32_t n = 0;
    uint32_t i;
    (void)start_time;
    (void)end_time;

    if (buffer == NULL || max_count == 0U) {
        return 0U;
    }

    /* 从打开的会话文件解析 */
    if (s_session_path[0] != '\0' && SdFs_IsReady()) {
        SdFsFile f;
        char line[128];
        uint32_t llen = 0;
        memset(&f, 0, sizeof(f));
        if (SdFs_Open(&f, s_session_path, SD_FS_MODE_READ)) {
            for (;;) {
                char ch = 0;
                int32_t rd = SdFs_Read(&f, &ch, 1U);
                if (rd <= 0) {
                    break;
                }
                if (ch == '\n') {
                    unsigned long ts;
                    unsigned chn, flags;
                    float sig, pp;
                    line[llen] = '\0';
                    if (llen > 0 && line[0] != 't' &&
                        sscanf(line, "%lu,%u,%f,%f,%u",
                               &ts, &chn, &sig, &pp, &flags) >= 4) {
                        if (channel_num == 0U || chn == channel_num) {
                            if (n < max_count) {
                                buffer[n].timestamp = (uint32_t)ts;
                                buffer[n].channel_num = (uint8_t)chn;
                                buffer[n].signal_value = sig;
                                buffer[n].peak_to_peak = pp;
                                buffer[n].flags = (uint8_t)flags;
                                n++;
                            }
                        }
                    }
                    llen = 0U;
                    continue;
                }
                if (ch != '\r' && llen + 1U < sizeof(line)) {
                    line[llen++] = ch;
                }
            }
            (void)SdFs_Close(&f);
            return n;
        }
    }

    /* 回退：内存缓冲 */
    for (i = 0; i < s_point_count && n < max_count; i++) {
        if (channel_num == 0U || s_point_buf[i].channel_num == channel_num) {
            buffer[n++] = s_point_buf[i];
        }
    }
    return n;
}

bool DataLogger_SaveAlarm(const AlarmRecord_t *record)
{
    AlarmRecord_t r;
    char line[DL_LINE_MAX];
    int n;
    int32_t wr;

    if (record == NULL || !SdFs_IsReady()) {
        return false;
    }
    if (!s_inited) {
        DataLogger_Init();
    }
    if (!dl_ensure_alarm_header()) {
        return false;
    }

    r = *record;
    if (r.id == 0U) {
        r.id = s_next_alarm_id++;
    }
    if (r.timestamp == 0U) {
        r.timestamp = dl_now_sec();
    }
    if (r.process_status[0] == '\0') {
        strncpy(r.process_status, "Open", sizeof(r.process_status) - 1U);
    }

    n = snprintf(line, sizeof(line),
                 "%lu,%lu,%u,%u,%s,%.4f,%.4f,%.3f,%.3f,%s,%s\n",
                 (unsigned long)r.id, (unsigned long)r.timestamp,
                 (unsigned)r.type, (unsigned)r.channel_num, r.channel_name,
                 (double)r.current_value, (double)r.threshold_value,
                 (double)r.over_duration_s, (double)r.duration_s,
                 r.process_status, r.notes);
    if (n <= 0) {
        return false;
    }
    wr = SdFs_Append(DL_ALARM_FILE, line, (uint32_t)n);
    if (wr == n) {
        s_alarm_total++;
        return true;
    }
    return false;
}

bool DataLogger_UpdateAlarm(const AlarmRecord_t *record)
{
    uint32_t n;
    uint32_t i;
    bool found = false;

    if (record == NULL) {
        return false;
    }
    n = dl_load_all_alarms(s_alarm_scratch, DL_ALARM_CACHE_MAX);
    for (i = 0; i < n; i++) {
        if (s_alarm_scratch[i].id == record->id) {
            s_alarm_scratch[i] = *record;
            found = true;
            break;
        }
    }
    if (!found) {
        return false;
    }
    return dl_rewrite_alarms(s_alarm_scratch, n);
}

uint32_t DataLogger_QueryAlarms(uint8_t type,
                                uint32_t start_time, uint32_t end_time,
                                AlarmRecord_t *buffer, uint32_t max_count)
{
    uint32_t total;
    uint32_t i;
    uint32_t out = 0;

    if (buffer == NULL || max_count == 0U) {
        return 0U;
    }
    total = dl_load_all_alarms(s_alarm_scratch, DL_ALARM_CACHE_MAX);
    for (i = 0; i < total && out < max_count; i++) {
        if (type != 0U && (uint8_t)s_alarm_scratch[i].type != type) {
            continue;
        }
        if (s_alarm_scratch[i].timestamp < start_time ||
            s_alarm_scratch[i].timestamp > end_time) {
            continue;
        }
        buffer[out++] = s_alarm_scratch[i];
    }
    return out;
}

bool DataLogger_ExportCSV(uint8_t channel_num,
                          uint32_t start_time, uint32_t end_time,
                          const char *filename)
{
    char out_path[DL_PATH_MAX];
    int n;

    if (filename == NULL || filename[0] == '\0' || !SdFs_IsReady()) {
        return false;
    }
    n = snprintf(out_path, sizeof(out_path), "%s/%s", PATH_DIR_EXPORT, filename);
    if (n <= 0 || (size_t)n >= sizeof(out_path)) {
        return false;
    }

    if (channel_num == 0U) {
        uint32_t total = dl_load_all_alarms(s_alarm_scratch, DL_ALARM_CACHE_MAX);
        uint32_t i;
        char line[DL_LINE_MAX];
        const char *hdr =
            "id,timestamp,type,channel_num,channel_name,current,threshold,"
            "over_duration_s,duration_s,process_status,notes\n";
        SdFsFile f;
        memset(&f, 0, sizeof(f));
        if (!SdFs_Open(&f, out_path, SD_FS_MODE_WRITE)) {
            return false;
        }
        (void)SdFs_Write(&f, hdr, (uint32_t)strlen(hdr));
        for (i = 0; i < total; i++) {
            if (s_alarm_scratch[i].timestamp < start_time ||
                s_alarm_scratch[i].timestamp > end_time) {
                continue;
            }
            n = snprintf(line, sizeof(line),
                "%lu,%lu,%u,%u,%s,%.4f,%.4f,%.3f,%.3f,%s,%s\n",
                (unsigned long)s_alarm_scratch[i].id,
                (unsigned long)s_alarm_scratch[i].timestamp,
                (unsigned)s_alarm_scratch[i].type,
                (unsigned)s_alarm_scratch[i].channel_num,
                s_alarm_scratch[i].channel_name,
                (double)s_alarm_scratch[i].current_value,
                (double)s_alarm_scratch[i].threshold_value,
                (double)s_alarm_scratch[i].over_duration_s,
                (double)s_alarm_scratch[i].duration_s,
                s_alarm_scratch[i].process_status,
                s_alarm_scratch[i].notes);
            if (n > 0) {
                (void)SdFs_Write(&f, line, (uint32_t)n);
            }
        }
        return SdFs_Close(&f);
    } else {
        DataPoint_t pts[64];
        uint32_t cnt = DataLogger_QueryHistory(channel_num, start_time, end_time,
                                               pts, 64U);
        uint32_t i;
        char line[128];
        const char *hdr = "timestamp,channel,signal,peak_to_peak,flags\n";
        SdFsFile f;
        memset(&f, 0, sizeof(f));
        if (!SdFs_Open(&f, out_path, SD_FS_MODE_WRITE)) {
            return false;
        }
        (void)SdFs_Write(&f, hdr, (uint32_t)strlen(hdr));
        for (i = 0; i < cnt; i++) {
            n = snprintf(line, sizeof(line), "%lu,%u,%.6f,%.6f,%u\n",
                         (unsigned long)pts[i].timestamp,
                         (unsigned)pts[i].channel_num,
                         (double)pts[i].signal_value,
                         (double)pts[i].peak_to_peak,
                         (unsigned)pts[i].flags);
            if (n > 0) {
                (void)SdFs_Write(&f, line, (uint32_t)n);
            }
        }
        return SdFs_Close(&f);
    }
}

void DataLogger_Cleanup(uint32_t retention_days)
{
    uint32_t now = dl_now_sec();
    uint32_t keep_sec = retention_days * 86400UL;
    uint16_t y;
    uint8_t m;
    int yi, mi;

    if (!SdFs_IsReady() || retention_days == 0U) {
        return;
    }
    dl_ymd(now, &y, &m, NULL);

    /* 粗扫近 5 年月份目录，过旧则删（目录内文件逐个删较复杂，先删已知路径模式） */
    for (yi = (int)y - 5; yi <= (int)y; yi++) {
        if (yi < 2020) {
            continue;
        }
        for (mi = 1; mi <= 12; mi++) {
            char dir[48];
            uint16_t yy = (uint16_t)yi;
            uint8_t mm = (uint8_t)mi;
            uint32_t approx_ts;
            if (yy > y || (yy == y && mm > m)) {
                continue;
            }
            /* 月初近似时间戳 */
            approx_ts = (uint32_t)((yy - 1970) * 365UL + (mm - 1) * 30UL) * 86400UL;
            if (now > approx_ts && (now - approx_ts) > keep_sec) {
                if (PathLayout_RealDataMonth(yy, mm, dir, (uint32_t)sizeof(dir))) {
                    /* 无法递归删目录时至少尝试 Remove 目录（空目录） */
                    (void)SdFs_Remove(dir);
                }
            }
        }
    }
}

uint32_t DataLogger_GetAlarmCount(void)
{
    if (!s_inited) {
        DataLogger_Init();
    }
    return s_alarm_total;
}
