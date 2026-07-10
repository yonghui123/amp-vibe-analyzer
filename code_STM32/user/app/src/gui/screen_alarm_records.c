/**
 * @file    screen_alarm_records.c
 * @brief   SignalX GUI - Alarm Records Screen (Tab 4)
 *
 * Layout (800×444 content area, after 36px tab bar):
 *
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │ [Part A: Alarm list,  y=0..227]                                         │
 * │  "Alarm Records & Curve View"(bold)  [Refresh]  [Export Excel]          │
 * │  Filter Type:[All▼]  Start Time:[2026-06-30][📅]  End Time:[...][📅]  [Search]│
 * │  ┌──────────┬──────┬───────┬──────────┬───────────┬──────┬──────┬──────┬──────┐│
 * │  │Alarm Time│ Type │Channel│Chan Name │Trigger Cond│CurVal│Thresh│Ovr(s)│Status││
 * │  │ (5 rows empty / paginated) ...                                        ││
 * │  └──────────┴──────┴───────┴──────────┴───────────┴──────┴──────┴──────┴──────┘│
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │ [Part B: Curve area, y=230..444]                                        │
 * │  ┌ Left panel (w=200) ───────┐  ┌ Right chart (w=596) ────────────────┐│
 * │  │ Curve Settings            │  │       Historical Curve              ││
 * │  │  Channel Select:[   ▼]   │  │100─                                 ││
 * │  │  [View Curve            ] │  │ 80─                                 ││
 * │  │  [Export Curve CSV      ] │  │C 60─                                ││
 * │  │ Alarm Notes               │  │h 40─                                ││
 * │  │  Status:[              ] │  │n 20─                                 ││
 * │  │  Notes:                   │  │  0─────────────────────────────     ││
 * │  │  [                      ] │  │    0  20  40  60  80  100           ││
 * │  │  [Save Notes            ] │  │              Time                   ││
 * │  └───────────────────────────┘  └─────────────────────────────────────┘│
 * └─────────────────────────────────────────────────────────────────────────┘
 *
 * Date Picker Popup (lv_calendar + lv_calendar_header_dropdown):
 *  Appears centered when calendar button is clicked.
 *  Year/month selected via dropdowns; day selected by tapping in calendar grid.
 */

/* ==================== Includes ==================== */
#include "lvgl.h"
#include "gui_core.h"
#include "gui_widgets.h"
#include "config.h"
#include "alarm_manager.h"
#include "data_logger.h"
#include <string.h>
#include <stdio.h>

/* ==================== Layout constants ==================== */
/* 800×480 display, tab bar=36px → content height=444px */
#define ALM_CONTENT_W       790     /* usable width (5px padding each side) */
#define ALM_LEFT_MARGIN       5     /* left margin for all elements */

/* Part A: Alarm list section */
#define ALM_BTN_ROW_Y         4     /* button bar y */
#define ALM_BTN_H            30     /* button height */
#define ALM_FILTER_ROW_Y     38     /* filter bar y */
#define ALM_FILTER_H         28     /* filter control height */
#define ALM_TABLE_Y          71     /* table start y */
#define ALM_TABLE_H         157     /* table height (header + 5 rows) */
#define ALM_UPPER_H         230     /* total upper section height */

/* Part B: Lower section */
#define ALM_SEP_Y           228     /* separator line y */
#define ALM_LOWER_Y         232     /* lower section start y */
#define ALM_LOWER_H         212     /* lower section height (444-232) */
#define ALM_PANEL_W         198     /* left control panel width */
#define ALM_CHART_START_X   202     /* chart section start x */
#define ALM_CHART_W         596     /* chart section width */

/* Date picker popup */
#define ALM_DP_PANEL_W      300     /* popup panel width                       */
#define ALM_DP_PANEL_H      375     /* popup panel height (fits 6-row calendar) */
#define ALM_DP_CAL_W        260     /* calendar widget width                   */
#define ALM_DP_CAL_H        275     /* calendar widget height (header+6 rows)  */

/* ==================== Table definition ==================== */
#define ALARM_TABLE_ROWS      6     /* header row + 5 data rows */
#define ALARM_TABLE_COLS      9     /* 9-column table per design */
#define ALARM_DATA_ROWS       5     /* visible data rows */
#define ALARM_CHART_POINTS  100
#define ALARM_NOTES_MAX_LEN 500

/* Table column indices */
enum {
    ALM_COL_TIME      = 0,
    ALM_COL_TYPE      = 1,
    ALM_COL_CHANNEL   = 2,
    ALM_COL_CHNAME    = 3,
    ALM_COL_TRIGCOND  = 4,
    ALM_COL_CURVAL    = 5,
    ALM_COL_THRESHOLD = 6,
    ALM_COL_OVERLIMIT = 7,
    ALM_COL_STATUS    = 8
};

/* Column pixel widths (sum = 788, fills 790px table minus 2px border) */
static const lv_coord_t g_col_widths[ALARM_TABLE_COLS] = {
    158,  /* Alarm Time      (+8) */
     62,  /* Type                 */
     55,  /* Channel              */
    100,  /* Channel Name         */
    110,  /* Trigger Cond   (+10) */
     68,  /* Cur Val              */
     68,  /* Threshold            */
     78,  /* Over-lim(s)          */
     89,  /* Status         (+10) */
    /* total = 788 ≈ table_w(790) - border(2) */
};

static const char *g_alm_headers[ALARM_TABLE_COLS] = {
    "Alarm Time", "Type", "Channel", "Channel Name",
    "Trigger Cond", "Cur Val", "Threshold", "Over-lim(s)", "Status"
};

/* ==================== Static GUI objects ==================== */
/* Part A */
static lv_obj_t *g_alm_dd_type       = NULL;
static lv_obj_t *g_alm_lbl_start     = NULL;   /* start date display label */
static lv_obj_t *g_alm_lbl_end       = NULL;   /* end date display label   */
static lv_obj_t *g_alm_table         = NULL;

/* Part B – left panel */
static lv_obj_t *g_alm_dd_channel    = NULL;
static lv_obj_t *g_alm_ta_status     = NULL;
static lv_obj_t *g_alm_ta_notes      = NULL;

/* Part B – right chart */
static lv_obj_t *g_alm_chart         = NULL;
static lv_chart_series_t *g_alm_ser  = NULL;

/* ==================== Date picker state ==================== */
static lv_obj_t            *g_dp_popup      = NULL;  /* popup overlay object  */
static lv_obj_t            *g_dp_lbl_sel    = NULL;  /* "Selected: ..." label */
static lv_calendar_date_t   g_dp_selected   = {2026, 6, 1};
static bool                 g_dp_is_start   = true;  /* editing start or end  */

/* ==================== Data buffer ==================== */
static AlarmRecord_t g_alm_records[ALARM_DATA_ROWS];
static uint32_t      g_alm_record_count = 0;
static int32_t       g_selected_row     = -1;

/* ==================== Forward declarations ==================== */
static void alm_btn_refresh_cb(lv_event_t *e);
static void alm_btn_export_cb(lv_event_t *e);
static void alm_btn_search_cb(lv_event_t *e);
static void alm_btn_view_curve_cb(lv_event_t *e);
static void alm_btn_export_csv_cb(lv_event_t *e);
static void alm_btn_save_notes_cb(lv_event_t *e);
static void alm_filter_type_cb(lv_event_t *e);
static void alm_table_draw_cb(lv_event_t *e);
static void alm_table_click_cb(lv_event_t *e);
static void alm_refresh_table(void);
static void alm_load_records(void);
/* date picker */
static void alm_dp_cal_cb(lv_event_t *e);
static void alm_dp_ok_cb(lv_event_t *e);
static void alm_dp_cancel_cb(lv_event_t *e);
static void alm_dp_open(bool is_start);
static void alm_btn_cal_start_cb(lv_event_t *e);
static void alm_btn_cal_end_cb(lv_event_t *e);

/* ==================== Helpers ==================== */

static void alm_fmt_time(char *buf, uint8_t sz, uint32_t ts)
{
    uint32_t t   = ts;
    uint8_t  sec = (uint8_t)(t % 60); t /= 60;
    uint8_t  min = (uint8_t)(t % 60); t /= 60;
    uint8_t  hr  = (uint8_t)(t % 24); t /= 24;
    uint16_t day = (uint16_t)(t % 365);
    uint8_t  mon = (uint8_t)(day / 30 + 1);
    uint8_t  dom = (uint8_t)(day % 30 + 1);
    lv_snprintf(buf, sz, "%02u-%02u %02u:%02u:%02u",
                (unsigned)mon, (unsigned)dom,
                (unsigned)hr,  (unsigned)min, (unsigned)sec);
}

static const char *alm_trig_str(AlarmType_t t)
{
    switch (t) {
        case ALARM_TYPE_GENERAL:   return ">= Threshold";
        case ALARM_TYPE_VIBRATION: return "PP>Envelope";
        case ALARM_TYPE_DUAL:      return ">Thr & PP>Env";
        default:                   return "-";
    }
}

/* Rough date-string "YYYY-MM-DD" → epoch seconds (approximate, Y2024 base) */
static uint32_t alm_date_to_ts(const char *s)
{
    int y = 0, m = 0, d = 0;
    if (s && s[0] >= '0' && s[0] <= '9') {
        y = ((int)(s[0]-'0')*1000 + (int)(s[1]-'0')*100 +
             (int)(s[2]-'0')*10   + (int)(s[3]-'0'));
        m = ((int)(s[5]-'0')*10   + (int)(s[6]-'0'));
        d = ((int)(s[8]-'0')*10   + (int)(s[9]-'0'));
    }
    if (y < 2000 || m < 1 || d < 1) return 0;
    return (uint32_t)(((uint32_t)(y - 2024)) * 365 * 86400u +
                      (uint32_t)(m - 1)      *  30 * 86400u +
                      (uint32_t)(d - 1)             * 86400u);
}

/* ==================== Table custom draw (header vs data rows) ============
 *  Row 0 → light gray header background
 *  Row 1+ → white background (overrides the default LV_PART_ITEMS style)
 * ======================================================================= */

static void alm_table_draw_cb(lv_event_t *e)
{
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (dsc->part != LV_PART_ITEMS) return;

    lv_obj_t *tbl  = lv_event_get_target(e);
    uint16_t  cols = lv_table_get_col_cnt(tbl);
    uint16_t  row  = (cols > 0) ? (uint16_t)(dsc->id / cols) : 0;

    if (row == 0) {
        /* Header row: light gray */
        dsc->rect_dsc->bg_color = lv_color_hex(0xE9ECEF);
        dsc->rect_dsc->bg_opa   = LV_OPA_COVER;
    } else {
        /* Data rows: pure white */
        dsc->rect_dsc->bg_color = lv_color_hex(0xFFFFFF);
        dsc->rect_dsc->bg_opa   = LV_OPA_COVER;
    }
}

/* ==================== Table refresh ==================== */

static void alm_refresh_table(void)
{
    if (!g_alm_table) return;

    for (uint16_t c = 0; c < ALARM_TABLE_COLS; c++)
        lv_table_set_cell_value(g_alm_table, 0, c, g_alm_headers[c]);

    for (uint16_t row = 1; row <= ALARM_DATA_ROWS; row++) {
        uint16_t idx = row - 1;
        if (idx < g_alm_record_count) {
            AlarmRecord_t *r = &g_alm_records[idx];
            char buf[32];

            alm_fmt_time(buf, sizeof(buf), r->timestamp);
            lv_table_set_cell_value(g_alm_table, row, ALM_COL_TIME, buf);

            const char *ts;
            switch (r->type) {
                case ALARM_TYPE_GENERAL:   ts = "General";   break;
                case ALARM_TYPE_VIBRATION: ts = "Vibration"; break;
                case ALARM_TYPE_DUAL:      ts = "Dual";      break;
                default:                   ts = "None";      break;
            }
            lv_table_set_cell_value(g_alm_table, row, ALM_COL_TYPE, ts);

            lv_snprintf(buf, sizeof(buf), "CH%u", (unsigned)r->channel_num);
            lv_table_set_cell_value(g_alm_table, row, ALM_COL_CHANNEL, buf);
            lv_table_set_cell_value(g_alm_table, row, ALM_COL_CHNAME,
                                    r->channel_name);
            lv_table_set_cell_value(g_alm_table, row, ALM_COL_TRIGCOND,
                                    alm_trig_str(r->type));

            GUI_FormatFloat(buf, sizeof(buf), r->current_value, 2);
            lv_table_set_cell_value(g_alm_table, row, ALM_COL_CURVAL, buf);

            GUI_FormatFloat(buf, sizeof(buf), r->threshold_value, 2);
            lv_table_set_cell_value(g_alm_table, row, ALM_COL_THRESHOLD, buf);

            GUI_FormatFloat(buf, sizeof(buf), r->over_duration_s, 1);
            lv_table_set_cell_value(g_alm_table, row, ALM_COL_OVERLIMIT, buf);

            lv_table_set_cell_value(g_alm_table, row, ALM_COL_STATUS,
                                    r->process_status[0] ? r->process_status : "-");
        } else {
            for (uint16_t c = 0; c < ALARM_TABLE_COLS; c++)
                lv_table_set_cell_value(g_alm_table, row, c, "");
        }
    }
}

/* ==================== Load records ==================== */

static void alm_load_records(void)
{
    if (!g_alm_dd_type) return;

    uint8_t type_filter = (uint8_t)lv_dropdown_get_selected(g_alm_dd_type);

    /* Parse date range from display labels */
    uint32_t t_start = 0;
    uint32_t t_end   = 0xFFFFFFFFUL;
    if (g_alm_lbl_start) {
        const char *s = lv_label_get_text(g_alm_lbl_start);
        uint32_t ts = alm_date_to_ts(s);
        if (ts > 0) t_start = ts;
    }
    if (g_alm_lbl_end) {
        const char *s = lv_label_get_text(g_alm_lbl_end);
        uint32_t ts = alm_date_to_ts(s);
        if (ts > 0) t_end = ts + 86399u;  /* include end date fully */
    }

    g_alm_record_count = DataLogger_QueryAlarms(
        type_filter, t_start, t_end,
        g_alm_records, ALARM_DATA_ROWS);

    g_selected_row = -1;
    alm_refresh_table();
}

/* ==================== Date picker callbacks ==================== */

static void alm_dp_cal_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;

    lv_obj_t *cal = lv_event_get_target(e);
    lv_calendar_date_t d;
    if (lv_calendar_get_pressed_date(cal, &d) == LV_RES_OK) {
        g_dp_selected = d;
        if (g_dp_lbl_sel) {
            char buf[40];
            lv_snprintf(buf, sizeof(buf), "Selected: %04d-%02d-%02d",
                        (int)d.year, (int)d.month, (int)d.day);
            lv_label_set_text(g_dp_lbl_sel, buf);
            lv_obj_set_style_text_color(g_dp_lbl_sel,
                                        lv_color_hex(0x007BFF), 0);
        }
    }
}

static void alm_dp_ok_cb(lv_event_t *e)
{
    (void)e;

    char buf[12];
    lv_snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                (int)g_dp_selected.year,
                (int)g_dp_selected.month,
                (int)g_dp_selected.day);

    /* Write date to the target label */
    lv_obj_t *target = g_dp_is_start ? g_alm_lbl_start : g_alm_lbl_end;
    if (target) {
        lv_label_set_text(target, buf);
        lv_obj_set_style_text_color(target, lv_color_hex(0x333333), 0);
    }

    /* Close popup */
    if (g_dp_popup) {
        lv_obj_del(g_dp_popup);
        g_dp_popup   = NULL;
        g_dp_lbl_sel = NULL;
    }
}

static void alm_dp_cancel_cb(lv_event_t *e)
{
    (void)e;
    if (g_dp_popup) {
        lv_obj_del(g_dp_popup);
        g_dp_popup   = NULL;
        g_dp_lbl_sel = NULL;
    }
}

/* ==================== Open date picker popup ==================== */

static void alm_dp_open(bool is_start)
{
    if (g_dp_popup) return;  /* already open */

    g_dp_is_start       = is_start;
    g_dp_selected.year  = 2026;
    g_dp_selected.month = 6;
    g_dp_selected.day   = 1;

    /* Try to pre-fill from current label */
    lv_obj_t *cur_lbl = is_start ? g_alm_lbl_start : g_alm_lbl_end;
    if (cur_lbl) {
        const char *s = lv_label_get_text(cur_lbl);
        if (s && s[4] == '-') {
            g_dp_selected.year  = (uint16_t)(
                (s[0]-'0')*1000+(s[1]-'0')*100+(s[2]-'0')*10+(s[3]-'0'));
            g_dp_selected.month = (int8_t)((s[5]-'0')*10+(s[6]-'0'));
            g_dp_selected.day   = (int8_t)((s[8]-'0')*10+(s[9]-'0'));
        }
    }

    /* Semi-transparent overlay on top layer */
    g_dp_popup = lv_obj_create(lv_layer_top());
    lv_obj_set_size(g_dp_popup, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(g_dp_popup, 0, 0);
    lv_obj_set_style_bg_color(g_dp_popup, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(g_dp_popup, LV_OPA_50, 0);
    lv_obj_set_style_border_width(g_dp_popup, 0, 0);
    lv_obj_set_style_pad_all(g_dp_popup, 0, 0);
    lv_obj_clear_flag(g_dp_popup, LV_OBJ_FLAG_SCROLLABLE);

    /* White panel centered in screen */
    lv_obj_t *panel = lv_obj_create(g_dp_popup);
    lv_obj_set_size(panel, ALM_DP_PANEL_W, ALM_DP_PANEL_H);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(panel, 6, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_pad_all(panel, 8, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    /* Title */
    lv_obj_t *lbl_title = lv_label_create(panel);
    lv_label_set_text(lbl_title,
                      is_start ? "Select Start Date" : "Select End Date");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x333333), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 2);

    /* Calendar widget */
    lv_obj_t *cal = lv_calendar_create(panel);
    lv_obj_set_size(cal, ALM_DP_CAL_W, ALM_DP_CAL_H);
    lv_obj_align(cal, LV_ALIGN_TOP_MID, 0, 26);
    lv_calendar_set_today_date(cal, 2026, 6, 30);
    lv_calendar_set_showed_date(cal,
                                g_dp_selected.year,
                                g_dp_selected.month);
    lv_obj_set_style_text_font(cal, &lv_font_montserrat_12, LV_PART_ITEMS);

#if LV_USE_CALENDAR_HEADER_DROPDOWN
    lv_calendar_header_dropdown_create(cal);
#elif LV_USE_CALENDAR_HEADER_ARROW
    lv_calendar_header_arrow_create(cal);
#endif

    lv_obj_add_event_cb(cal, alm_dp_cal_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Set year dropdown list to a useful range (avoids default "1901") */
#if LV_USE_CALENDAR_HEADER_DROPDOWN
    lv_calendar_header_dropdown_set_year_list(cal,
        "2030\n2029\n2028\n2027\n2026\n2025\n2024\n2023\n2022\n2021\n2020");
    /* Re-show the correct month/year after setting the list */
    lv_calendar_set_showed_date(cal,
                                g_dp_selected.year,
                                g_dp_selected.month);
#endif

    /* "Selected: ..." label – positioned just below the calendar */
    g_dp_lbl_sel = lv_label_create(panel);
    {
        char buf[40];
        lv_snprintf(buf, sizeof(buf), "Selected: %04d-%02d-%02d",
                    (int)g_dp_selected.year,
                    (int)g_dp_selected.month,
                    (int)g_dp_selected.day);
        lv_label_set_text(g_dp_lbl_sel, buf);
    }
    lv_obj_set_style_text_font(g_dp_lbl_sel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(g_dp_lbl_sel, lv_color_hex(0x555555), 0);
    /* y = calendar_y_offset(26) + calendar_h + gap(8) */
    lv_obj_align(g_dp_lbl_sel, LV_ALIGN_TOP_MID, 0, 26 + ALM_DP_CAL_H + 8);

    /* Cancel button */
    lv_obj_t *btn_cancel = lv_btn_create(panel);
    lv_obj_set_size(btn_cancel, 118, 32);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x888888), 0);
    lv_obj_set_style_radius(btn_cancel, 4, 0);
    lv_obj_t *lbl_c = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_c, "Cancel");
    lv_obj_set_style_text_font(lbl_c, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_c);
    lv_obj_add_event_cb(btn_cancel, alm_dp_cancel_cb, LV_EVENT_CLICKED, NULL);

    /* OK button */
    lv_obj_t *btn_ok = lv_btn_create(panel);
    lv_obj_set_size(btn_ok, 118, 32);
    lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(btn_ok, lv_color_hex(0x007BFF), 0);
    lv_obj_set_style_radius(btn_ok, 4, 0);
    lv_obj_t *lbl_o = lv_label_create(btn_ok);
    lv_label_set_text(lbl_o, "OK");
    lv_obj_set_style_text_font(lbl_o, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_o);
    lv_obj_add_event_cb(btn_ok, alm_dp_ok_cb, LV_EVENT_CLICKED, NULL);
}

static void alm_btn_cal_start_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) alm_dp_open(true);
}

static void alm_btn_cal_end_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) alm_dp_open(false);
}

/* ==================== Button callbacks ==================== */

static void alm_btn_refresh_cb(lv_event_t *e)
{
    (void)e;
    GUI_ShowLoading(true);
    alm_load_records();
    GUI_ShowLoading(false);
    GUI_Toast("Alarm list refreshed", 1500);
}

static void alm_btn_export_cb(lv_event_t *e)
{
    (void)e;
    GUI_ShowLoading(true);
    bool ok = DataLogger_ExportCSV(0, 0, 0xFFFFFFFFUL, "alarm_export.csv");
    GUI_ShowLoading(false);
    GUI_Toast(ok ? "Exported: alarm_export.csv" : "Export failed!", 2000);
}

static void alm_btn_search_cb(lv_event_t *e)
{
    (void)e;
    GUI_ShowLoading(true);
    alm_load_records();
    GUI_ShowLoading(false);
    GUI_Toast("Search complete", 1200);
}

static void alm_filter_type_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) alm_load_records();
}

static void alm_table_click_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;

    lv_obj_t *tbl = lv_event_get_target(e);
    uint16_t row, col;
    lv_table_get_selected_cell(tbl, &row, &col);

    if (row == 0 || row == LV_TABLE_CELL_NONE) return;
    int32_t idx = (int32_t)row - 1;
    if (idx < 0 || (uint32_t)idx >= g_alm_record_count) return;

    g_selected_row = idx;
    if (g_alm_ta_status)
        lv_textarea_set_text(g_alm_ta_status, g_alm_records[idx].process_status);
    if (g_alm_ta_notes)
        lv_textarea_set_text(g_alm_ta_notes, g_alm_records[idx].notes);
}

static void alm_btn_view_curve_cb(lv_event_t *e)
{
    (void)e;
    if (!g_alm_chart || !g_alm_ser) return;

    uint8_t ch = (uint8_t)(lv_dropdown_get_selected(g_alm_dd_channel) + 1);
    DataPoint_t pts[ALARM_CHART_POINTS];
    uint32_t cnt = DataLogger_QueryHistory(ch, 0, 0xFFFFFFFFUL,
                                           pts, ALARM_CHART_POINTS);

    lv_chart_set_point_count(g_alm_chart, ALARM_CHART_POINTS);
    lv_chart_refresh(g_alm_chart);
    for (uint32_t i = 0; i < cnt && i < ALARM_CHART_POINTS; i++)
        lv_chart_set_next_value(g_alm_chart, g_alm_ser,
                                (lv_coord_t)pts[i].signal_value);
    lv_chart_refresh(g_alm_chart);
    GUI_Toast("Curve updated", 1500);
}

static void alm_btn_export_csv_cb(lv_event_t *e)
{
    (void)e;
    uint8_t ch = (uint8_t)(lv_dropdown_get_selected(g_alm_dd_channel) + 1);
    GUI_ShowLoading(true);
    bool ok = DataLogger_ExportCSV(ch, 0, 0xFFFFFFFFUL, "history_export.csv");
    GUI_ShowLoading(false);
    GUI_Toast(ok ? "Exported: history_export.csv" : "Export failed!", 2000);
}

static void alm_btn_save_notes_cb(lv_event_t *e)
{
    (void)e;
    if (g_selected_row < 0 || (uint32_t)g_selected_row >= g_alm_record_count) {
        GUI_Toast("Select a record first", 1500);
        return;
    }
    AlarmRecord_t *r = &g_alm_records[g_selected_row];
    if (g_alm_ta_status) {
        const char *s = lv_textarea_get_text(g_alm_ta_status);
        strncpy(r->process_status, s, sizeof(r->process_status) - 1);
        r->process_status[sizeof(r->process_status) - 1] = '\0';
    }
    if (g_alm_ta_notes) {
        const char *n = lv_textarea_get_text(g_alm_ta_notes);
        strncpy(r->notes, n, sizeof(r->notes) - 1);
        r->notes[sizeof(r->notes) - 1] = '\0';
    }
    alm_refresh_table();
    GUI_Toast("Notes saved", 1500);
}

/* ==================== Public refresh ==================== */

void gui_alarm_records_refresh_list(void)
{
    alm_load_records();
}

/* ==================== Helper: make an "input-field–styled" container
 *  Returns the inner lv_label which can be updated with lv_label_set_text.
 *  Parent must support absolute positioning (scroll disabled). ============= */
static lv_obj_t *alm_make_date_field(lv_obj_t *parent,
                                     lv_coord_t x, lv_coord_t y,
                                     lv_coord_t w, lv_coord_t h,
                                     const char *placeholder)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_size(box, w, h);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_style_bg_color(box, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_color(box, lv_color_hex(0xBBBBBB), 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_radius(box, 3, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(box);
    lv_label_set_text(lbl, placeholder);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x999999), 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

    return lbl;
}

/* ==================== Public create function ==================== */

void gui_alarm_records_create(lv_obj_t *parent)
{
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xF8F9FA), 0);
    lv_obj_set_scroll_dir(parent, LV_DIR_NONE);

    /* ================================================================
     * PART A – TOP BUTTON BAR  (y = ALM_BTN_ROW_Y)
     * ================================================================ */
    {
        lv_obj_t *lbl = lv_label_create(parent);
        lv_label_set_text(lbl, "Alarm Records & Curve View");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x333333), 0);
        lv_obj_set_pos(lbl, ALM_LEFT_MARGIN, ALM_BTN_ROW_Y + 7);

        /* Refresh */
        lv_obj_t *btn_ref = lv_btn_create(parent);
        lv_obj_set_size(btn_ref, 84, ALM_BTN_H);
        lv_obj_set_pos(btn_ref, 292, ALM_BTN_ROW_Y);
        lv_obj_set_style_bg_color(btn_ref, lv_color_hex(0x007BFF), 0);
        lv_obj_set_style_radius(btn_ref, 4, 0);
        lv_obj_t *lbl_ref = lv_label_create(btn_ref);
        lv_label_set_text(lbl_ref, "Refresh");
        lv_obj_set_style_text_font(lbl_ref, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl_ref);
        lv_obj_add_event_cb(btn_ref, alm_btn_refresh_cb, LV_EVENT_CLICKED, NULL);

        /* Export Excel */
        lv_obj_t *btn_exp = lv_btn_create(parent);
        lv_obj_set_size(btn_exp, 116, ALM_BTN_H);
        lv_obj_set_pos(btn_exp, 384, ALM_BTN_ROW_Y);
        lv_obj_set_style_bg_color(btn_exp, lv_color_hex(0x007BFF), 0);
        lv_obj_set_style_radius(btn_exp, 4, 0);
        lv_obj_t *lbl_exp = lv_label_create(btn_exp);
        lv_label_set_text(lbl_exp, "Export Excel");
        lv_obj_set_style_text_font(lbl_exp, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl_exp);
        lv_obj_add_event_cb(btn_exp, alm_btn_export_cb, LV_EVENT_CLICKED, NULL);
    }

    /* ================================================================
     * PART A – FILTER BAR  (y = ALM_FILTER_ROW_Y)
     *
     *  Filter Type: [All▼]   Start Time: [2026-06-01][📅]
     *                         End Time:   [2026-06-30][📅]   [Search]
     * ================================================================ */
    {
        lv_coord_t fy = ALM_FILTER_ROW_Y;
        lv_coord_t fh = ALM_FILTER_H;

        /* ── Filter Type ── */
        lv_obj_t *lbl_ft = lv_label_create(parent);
        lv_label_set_text(lbl_ft, "Filter Type:");
        lv_obj_set_style_text_font(lbl_ft, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl_ft, lv_color_hex(0x333333), 0);
        lv_obj_set_pos(lbl_ft, ALM_LEFT_MARGIN, fy + (fh - 14) / 2);

        g_alm_dd_type = lv_dropdown_create(parent);
        lv_dropdown_set_options(g_alm_dd_type,
                                "All\nGeneral\nVibration\nDual");
        lv_dropdown_set_selected(g_alm_dd_type, 0);
        lv_obj_set_size(g_alm_dd_type, 108, fh);
        lv_obj_set_pos(g_alm_dd_type, 80, fy);
        lv_obj_set_style_text_font(g_alm_dd_type, &lv_font_montserrat_12, 0);
        /* Vertically center text inside dropdown */
        lv_obj_set_style_pad_top(g_alm_dd_type,
                                 (fh - 14) / 2, LV_PART_MAIN);
        lv_obj_add_event_cb(g_alm_dd_type, alm_filter_type_cb,
                            LV_EVENT_VALUE_CHANGED, NULL);

        /* ── Start Time ── */
        lv_obj_t *lbl_st = lv_label_create(parent);
        lv_label_set_text(lbl_st, "Start Time:");
        lv_obj_set_style_text_font(lbl_st, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl_st, lv_color_hex(0x333333), 0);
        lv_obj_set_pos(lbl_st, 196, fy + (fh - 14) / 2);

        /* Date display field (looks like input box, text centered) */
        g_alm_lbl_start = alm_make_date_field(parent,
                                              268, fy, 116, fh,
                                              "Select Date");

        /* Calendar button – blue, shows calendar icon */
        lv_obj_t *btn_cs = lv_btn_create(parent);
        lv_obj_set_size(btn_cs, fh, fh);
        lv_obj_set_pos(btn_cs, 388, fy);
        lv_obj_set_style_bg_color(btn_cs, lv_color_hex(0x007BFF), 0);
        lv_obj_set_style_radius(btn_cs, 3, 0);
        lv_obj_t *lbl_cs = lv_label_create(btn_cs);
        lv_label_set_text(lbl_cs, LV_SYMBOL_LIST);
        lv_obj_set_style_text_color(lbl_cs, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(lbl_cs);
        lv_obj_add_event_cb(btn_cs, alm_btn_cal_start_cb,
                            LV_EVENT_CLICKED, NULL);

        /* ── End Time ── */
        lv_obj_t *lbl_et = lv_label_create(parent);
        lv_label_set_text(lbl_et, "End Time:");
        lv_obj_set_style_text_font(lbl_et, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl_et, lv_color_hex(0x333333), 0);
        lv_obj_set_pos(lbl_et, 424, fy + (fh - 14) / 2);

        g_alm_lbl_end = alm_make_date_field(parent,
                                            490, fy, 116, fh,
                                            "Select Date");

        lv_obj_t *btn_ce = lv_btn_create(parent);
        lv_obj_set_size(btn_ce, fh, fh);
        lv_obj_set_pos(btn_ce, 610, fy);
        lv_obj_set_style_bg_color(btn_ce, lv_color_hex(0x007BFF), 0);
        lv_obj_set_style_radius(btn_ce, 3, 0);
        lv_obj_t *lbl_ce = lv_label_create(btn_ce);
        lv_label_set_text(lbl_ce, LV_SYMBOL_LIST);
        lv_obj_set_style_text_color(lbl_ce, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(lbl_ce);
        lv_obj_add_event_cb(btn_ce, alm_btn_cal_end_cb,
                            LV_EVENT_CLICKED, NULL);

        /* ── Search ── */
        lv_obj_t *btn_s = lv_btn_create(parent);
        lv_obj_set_size(btn_s, 80, fh);
        lv_obj_set_pos(btn_s, 646, fy);
        lv_obj_set_style_bg_color(btn_s, lv_color_hex(0x007BFF), 0);
        lv_obj_set_style_radius(btn_s, 4, 0);
        lv_obj_t *lbl_s = lv_label_create(btn_s);
        lv_label_set_text(lbl_s, "Search");
        lv_obj_set_style_text_font(lbl_s, &lv_font_montserrat_12, 0);
        lv_obj_center(lbl_s);
        lv_obj_add_event_cb(btn_s, alm_btn_search_cb,
                            LV_EVENT_CLICKED, NULL);
    }

    /* ================================================================
     * PART A – ALARM TABLE  (y = ALM_TABLE_Y)
     * ================================================================ */
    {
        /* Top separator */
        lv_obj_t *sep = lv_obj_create(parent);
        lv_obj_set_size(sep, LCD_WIDTH - 4, 1);
        lv_obj_set_pos(sep, 2, ALM_TABLE_Y - 2);
        lv_obj_set_style_bg_color(sep, lv_color_hex(0xDDDDDD), 0);
        lv_obj_set_style_border_width(sep, 0, 0);
        lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

        g_alm_table = lv_table_create(parent);
        lv_obj_set_pos(g_alm_table, ALM_LEFT_MARGIN, ALM_TABLE_Y);
        lv_obj_set_size(g_alm_table, ALM_CONTENT_W, ALM_TABLE_H);
        lv_table_set_row_cnt(g_alm_table, ALARM_TABLE_ROWS);
        lv_table_set_col_cnt(g_alm_table, ALARM_TABLE_COLS);

        for (uint16_t c = 0; c < ALARM_TABLE_COLS; c++)
            lv_table_set_col_width(g_alm_table, c, g_col_widths[c]);

        lv_obj_set_style_text_font(g_alm_table,
                                   &lv_font_montserrat_12, 0);
        /* Outer border */
        lv_obj_set_style_border_color(g_alm_table,
                                      lv_color_hex(0xCCCCCC), 0);
        lv_obj_set_style_border_width(g_alm_table, 1, 0);
        /* Table container background: white */
        lv_obj_set_style_bg_color(g_alm_table,
                                  lv_color_hex(0xFFFFFF), 0);

        /* Default item style: white; header/data row colors overridden
         * in alm_table_draw_cb via LV_EVENT_DRAW_PART_BEGIN             */
        lv_obj_set_style_bg_color(g_alm_table, lv_color_hex(0xFFFFFF),
                                  LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(g_alm_table, LV_OPA_COVER,
                                LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(g_alm_table, lv_color_hex(0x333333),
                                    LV_PART_ITEMS);
        /* Cell inner border (between cells): light gray */
        lv_obj_set_style_border_color(g_alm_table, lv_color_hex(0xDEDEDE),
                                      LV_PART_ITEMS);
        lv_obj_set_style_border_width(g_alm_table, 1, LV_PART_ITEMS);
        /* Vertically center cell text */
        lv_obj_set_style_pad_top(g_alm_table, 6, LV_PART_ITEMS);
        lv_obj_set_style_pad_bottom(g_alm_table, 6, LV_PART_ITEMS);
        lv_obj_set_style_pad_left(g_alm_table, 4, LV_PART_ITEMS);

        /* Touch-press highlight */
        lv_obj_set_style_bg_color(g_alm_table, lv_color_hex(0xCCE5FF),
                                  LV_PART_ITEMS | LV_STATE_PRESSED);

        /* No scrollbar – all 5 rows fit within ALM_TABLE_H */
        lv_obj_set_scroll_dir(g_alm_table, LV_DIR_NONE);
        lv_obj_set_scrollbar_mode(g_alm_table, LV_SCROLLBAR_MODE_OFF);

        /* Draw callback: header row gray, data rows white */
        lv_obj_add_event_cb(g_alm_table, alm_table_draw_cb,
                            LV_EVENT_DRAW_PART_BEGIN, NULL);
        lv_obj_add_event_cb(g_alm_table, alm_table_click_cb,
                            LV_EVENT_VALUE_CHANGED, NULL);
    }

    /* ================================================================
     * SEPARATOR
     * ================================================================ */
    {
        lv_obj_t *sep = lv_obj_create(parent);
        lv_obj_set_size(sep, LCD_WIDTH, 2);
        lv_obj_set_pos(sep, 0, ALM_SEP_Y);
        lv_obj_set_style_bg_color(sep, lv_color_hex(0xCCCCCC), 0);
        lv_obj_set_style_border_width(sep, 0, 0);
        lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
    }

    /* ================================================================
     * PART B – LEFT CONTROL PANEL
     * ================================================================ */
    {
        lv_obj_t *panel = lv_obj_create(parent);
        lv_obj_set_size(panel, ALM_PANEL_W, ALM_LOWER_H);
        lv_obj_set_pos(panel, 0, ALM_LOWER_Y);
        lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_color(panel, lv_color_hex(0xDDDDDD), 0);
        lv_obj_set_style_border_width(panel, 1, 0);
        lv_obj_set_style_radius(panel, 0, 0);
        lv_obj_set_style_pad_all(panel, 0, 0);
        lv_obj_set_scroll_dir(panel, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_AUTO);

        lv_coord_t py = 5;

        /* ── Curve Settings ── */
        lv_obj_t *lbl_cs = lv_label_create(panel);
        lv_label_set_text(lbl_cs, "Curve Settings");
        lv_obj_set_style_text_font(lbl_cs, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_cs, lv_color_hex(0x333333), 0);
        lv_obj_set_pos(lbl_cs, 6, py);
        py += 20;

        lv_obj_t *lbl_ch = lv_label_create(panel);
        lv_label_set_text(lbl_ch, "Channel Select:");
        lv_obj_set_style_text_font(lbl_ch, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl_ch, lv_color_hex(0x555555), 0);
        lv_obj_set_pos(lbl_ch, 6, py);
        py += 16;

        g_alm_dd_channel = lv_dropdown_create(panel);
        lv_dropdown_set_options(g_alm_dd_channel,
                                "CH1\nCH2\nCH3\nCH4\nCH5\nCH6\nCH7\nCH8");
        lv_dropdown_set_selected(g_alm_dd_channel, 0);
        lv_obj_set_size(g_alm_dd_channel, 184, 26);
        lv_obj_set_pos(g_alm_dd_channel, 6, py);
        lv_obj_set_style_text_font(g_alm_dd_channel,
                                   &lv_font_montserrat_12, 0);
        /* Center text vertically in 26px dropdown */
        lv_obj_set_style_pad_top(g_alm_dd_channel, (26-14)/2, LV_PART_MAIN);
        py += 30;

        /* View Curve */
        lv_obj_t *btn_vc = lv_btn_create(panel);
        lv_obj_set_size(btn_vc, 184, 28);
        lv_obj_set_pos(btn_vc, 6, py);
        lv_obj_set_style_bg_color(btn_vc, lv_color_hex(0x007BFF), 0);
        lv_obj_set_style_radius(btn_vc, 4, 0);
        lv_obj_t *lbl_vc = lv_label_create(btn_vc);
        lv_label_set_text(lbl_vc, "View Curve");
        lv_obj_set_style_text_font(lbl_vc, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl_vc);
        lv_obj_add_event_cb(btn_vc, alm_btn_view_curve_cb,
                            LV_EVENT_CLICKED, NULL);
        py += 32;

        /* Export Curve CSV */
        lv_obj_t *btn_csv = lv_btn_create(panel);
        lv_obj_set_size(btn_csv, 184, 28);
        lv_obj_set_pos(btn_csv, 6, py);
        lv_obj_set_style_bg_color(btn_csv, lv_color_hex(0x555555), 0);
        lv_obj_set_style_radius(btn_csv, 4, 0);
        lv_obj_t *lbl_csv = lv_label_create(btn_csv);
        lv_label_set_text(lbl_csv, "Export Curve CSV");
        lv_obj_set_style_text_font(lbl_csv, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl_csv);
        lv_obj_add_event_cb(btn_csv, alm_btn_export_csv_cb,
                            LV_EVENT_CLICKED, NULL);
        py += 36;

        /* Divider */
        lv_obj_t *sep = lv_obj_create(panel);
        lv_obj_set_size(sep, 184, 1);
        lv_obj_set_pos(sep, 6, py);
        lv_obj_set_style_bg_color(sep, lv_color_hex(0xDDDDDD), 0);
        lv_obj_set_style_border_width(sep, 0, 0);
        lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
        py += 6;

        /* ── Alarm Notes ── */
        lv_obj_t *lbl_an = lv_label_create(panel);
        lv_label_set_text(lbl_an, "Alarm Notes");
        lv_obj_set_style_text_font(lbl_an, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_an, lv_color_hex(0x333333), 0);
        lv_obj_set_pos(lbl_an, 6, py);
        py += 20;

        /* Status label + input (single line, text vertically centered) */
        lv_obj_t *lbl_st = lv_label_create(panel);
        lv_label_set_text(lbl_st, "Status:");
        lv_obj_set_style_text_font(lbl_st, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl_st, lv_color_hex(0x555555), 0);
        lv_obj_set_pos(lbl_st, 6, py + 4);

        g_alm_ta_status = lv_textarea_create(panel);
        lv_textarea_set_one_line(g_alm_ta_status, true);
        lv_textarea_set_max_length(g_alm_ta_status, 9);
        lv_textarea_set_placeholder_text(g_alm_ta_status, "Pending");
        lv_obj_set_size(g_alm_ta_status, 122, 26);
        lv_obj_set_pos(g_alm_ta_status, 58, py);
        lv_obj_set_style_text_font(g_alm_ta_status,
                                   &lv_font_montserrat_12, 0);
        lv_obj_set_style_border_color(g_alm_ta_status,
                                      lv_color_hex(0xBBBBBB), 0);
        /* Fix vertical centering: pad_top = (h - font_line_h) / 2 */
        lv_obj_set_style_pad_top(g_alm_ta_status, 4, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(g_alm_ta_status, 4, LV_PART_MAIN);
        py += 30;

        /* Notes label */
        lv_obj_t *lbl_nt = lv_label_create(panel);
        lv_label_set_text(lbl_nt, "Notes:");
        lv_obj_set_style_text_font(lbl_nt, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl_nt, lv_color_hex(0x555555), 0);
        lv_obj_set_pos(lbl_nt, 6, py);
        py += 16;

        /* Notes textarea */
        g_alm_ta_notes = lv_textarea_create(panel);
        lv_textarea_set_max_length(g_alm_ta_notes, ALARM_NOTES_MAX_LEN);
        lv_textarea_set_placeholder_text(g_alm_ta_notes, "Enter notes...");
        lv_obj_set_size(g_alm_ta_notes, 184, 40);
        lv_obj_set_pos(g_alm_ta_notes, 6, py);
        lv_obj_set_style_text_font(g_alm_ta_notes,
                                   &lv_font_montserrat_12, 0);
        lv_obj_set_style_border_color(g_alm_ta_notes,
                                      lv_color_hex(0xBBBBBB), 0);
        lv_obj_set_style_pad_top(g_alm_ta_notes, 4, LV_PART_MAIN);
        py += 44;

        /* Save Notes */
        lv_obj_t *btn_sn = lv_btn_create(panel);
        lv_obj_set_size(btn_sn, 184, 28);
        lv_obj_set_pos(btn_sn, 6, py);
        lv_obj_set_style_bg_color(btn_sn, lv_color_hex(0x007BFF), 0);
        lv_obj_set_style_radius(btn_sn, 4, 0);
        lv_obj_t *lbl_sn = lv_label_create(btn_sn);
        lv_label_set_text(lbl_sn, "Save Notes");
        lv_obj_set_style_text_font(lbl_sn, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl_sn);
        lv_obj_add_event_cb(btn_sn, alm_btn_save_notes_cb,
                            LV_EVENT_CLICKED, NULL);
    }

    /* ================================================================
     * PART B – RIGHT CHART AREA  (Historical Curve)
     * ================================================================ */
    {
        lv_obj_t *cpanel = lv_obj_create(parent);
        lv_obj_set_size(cpanel, ALM_CHART_W, ALM_LOWER_H);
        lv_obj_set_pos(cpanel, ALM_CHART_START_X, ALM_LOWER_Y);
        lv_obj_set_style_bg_color(cpanel, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_color(cpanel, lv_color_hex(0xDDDDDD), 0);
        lv_obj_set_style_border_width(cpanel, 1, 0);
        lv_obj_set_style_radius(cpanel, 0, 0);
        lv_obj_set_style_pad_all(cpanel, 0, 0);
        lv_obj_clear_flag(cpanel, LV_OBJ_FLAG_SCROLLABLE);

        /* Title */
        lv_obj_t *lbl_t = lv_label_create(cpanel);
        lv_label_set_text(lbl_t, "Historical Curve");
        lv_obj_set_style_text_font(lbl_t, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_t, lv_color_hex(0x333333), 0);
        lv_obj_align(lbl_t, LV_ALIGN_TOP_MID, 0, 5);

        /* Y-axis name (stacked text, left side) */
        lv_obj_t *lbl_y = lv_label_create(cpanel);
        lv_label_set_text(lbl_y, "Chan.\nVal.");
        lv_obj_set_style_text_font(lbl_y, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(lbl_y, lv_color_hex(0x666666), 0);
        lv_obj_set_style_text_align(lbl_y, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(lbl_y, 3, 68);

        /* Chart */
        g_alm_chart = lv_chart_create(cpanel);
        lv_obj_set_size(g_alm_chart, ALM_CHART_W - 46, ALM_LOWER_H - 38);
        lv_obj_set_pos(g_alm_chart, 36, 22);
        lv_chart_set_type(g_alm_chart, LV_CHART_TYPE_LINE);
        lv_chart_set_point_count(g_alm_chart, ALARM_CHART_POINTS);
        lv_chart_set_range(g_alm_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
        lv_chart_set_div_line_count(g_alm_chart, 5, 10);

        lv_obj_set_style_bg_color(g_alm_chart, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_color(g_alm_chart,
                                      lv_color_hex(0xCCCCCC), 0);
        lv_obj_set_style_border_width(g_alm_chart, 1, 0);
        lv_obj_set_style_line_color(g_alm_chart, lv_color_hex(0xE0E0E0),
                                    LV_PART_MAIN);

        /* Axis ticks */
        lv_chart_set_axis_tick(g_alm_chart, LV_CHART_AXIS_PRIMARY_Y,
                               4, 2, 6, 2, true, 36);
        lv_chart_set_axis_tick(g_alm_chart, LV_CHART_AXIS_PRIMARY_X,
                               4, 2, 6, 2, true, 16);
        lv_obj_set_style_text_font(g_alm_chart,
                                   &lv_font_montserrat_10, LV_PART_TICKS);
        lv_obj_set_style_text_color(g_alm_chart, lv_color_hex(0x666666),
                                    LV_PART_TICKS);

        /* Data series (blue line, no dots) */
        g_alm_ser = lv_chart_add_series(g_alm_chart,
                                        lv_color_hex(0x007BFF),
                                        LV_CHART_AXIS_PRIMARY_Y);
        lv_obj_set_style_size(g_alm_chart, 0, LV_PART_INDICATOR);

        /* X-axis label */
        lv_obj_t *lbl_x = lv_label_create(cpanel);
        lv_label_set_text(lbl_x, "Time");
        lv_obj_set_style_text_font(lbl_x, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(lbl_x, lv_color_hex(0x666666), 0);
        lv_obj_align(lbl_x, LV_ALIGN_BOTTOM_MID, 0, -3);
    }

    /* Initial load */
    alm_load_records();
}

void gui_alarm_records_destroy(lv_obj_t *parent)
{
    if (g_dp_popup != NULL) {
        lv_obj_del(g_dp_popup);
        g_dp_popup   = NULL;
        g_dp_lbl_sel = NULL;
    }

    if (parent != NULL) {
        lv_obj_clean(parent);
    }

    g_alm_dd_type    = NULL;
    g_alm_lbl_start  = NULL;
    g_alm_lbl_end    = NULL;
    g_alm_table      = NULL;
    g_alm_dd_channel = NULL;
    g_alm_ta_status  = NULL;
    g_alm_ta_notes   = NULL;
    g_alm_chart      = NULL;
    g_alm_ser        = NULL;
    g_selected_row   = -1;
}
