/**
 * @file    screen_acq_settings.c
 * @brief   SignalX GUI - 采集设置界面 (Tab 2)
 *
 *  左右分栏布局:
 *   - 左侧: 周期与通道 / 滤波设置 / 阈值设置 / 包络线设置 + 应用参数按钮
 *   - 右侧: 参数说明 + 系统状态指示
 */

/* ==================== Includes ==================== */
#include "lvgl.h"
#include "gui_core.h"
#include "gui_widgets.h"
#include "config.h"
#include "config_store.h"
#include "channel_data.h"
#include "acq_pipeline.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 平台头文件: STM32 用 FatFs, PC模拟器用 dirent */
#if defined(PC_SIMULATOR)
  #include <dirent.h>
#else
  #include "ff.h"       /* FatFs - SD 卡文件系统 */
#endif

/* ==================== Layout constants ==================== */
#define ACQ_LEFT_W          460
#define ACQ_RIGHT_W         330
#define ACQ_GROUP_PAD        8
#define ACQ_ROW_H            32
#define ACQ_LABEL_W         185
#define ACQ_CTRL_W          180
#define ACQ_UNIT_W           60

/* ==================== Static GUI objects ==================== */
/* Group 1: Cycle & Channel */
static lv_obj_t *g_acq_spin_cycle     = NULL;
static lv_obj_t *g_acq_dd_vib_ch      = NULL;
static lv_obj_t *g_acq_dd_gen_ch      = NULL;

/* Group 2: Filter Settings */
static lv_obj_t *g_acq_dd_filter      = NULL;
static lv_obj_t *g_acq_spin_rpm       = NULL;
static lv_obj_t *g_acq_label_rpm      = NULL;
static lv_obj_t *g_acq_row_rpm        = NULL;
static lv_obj_t *g_acq_spin_cycle_mul = NULL;
static lv_obj_t *g_acq_spin_mov_avg   = NULL;

/* Group 3: Threshold */
static lv_obj_t *g_acq_spin_gen_thr   = NULL;
static lv_obj_t *g_acq_spin_thr_time  = NULL;
static lv_obj_t *g_acq_spin_alarm_pct = NULL;

/* Group 4: Envelope */
static lv_obj_t *g_acq_spin_env_up    = NULL;
static lv_obj_t *g_acq_spin_env_low   = NULL;
static lv_obj_t *g_acq_ta_env_path    = NULL;

/* Status */
static lv_obj_t *g_acq_status_led     = NULL;
static lv_obj_t *g_acq_status_label   = NULL;

/* ==================== 文件选择器弹窗 ==================== */
static lv_obj_t     *g_fp_popup       = NULL;   /* 弹窗根对象 (全屏背景 + 内容面板) */
static lv_obj_t     *g_fp_list        = NULL;   /* 文件列表 (lv_list) */
static lv_group_t   *g_fp_group       = NULL;   /* 弹窗专用焦点组 */
static lv_group_t   *g_fp_main_group  = NULL;   /* 保存的主焦点组 */

/* 包络线文件扫描目录 (STM32: SD卡路径, PC: 当前目录下的 Envelope 文件夹) */
#if defined(PC_SIMULATOR)
  #define ENVELOPE_DIR  "Envelope"
#else
  #define ENVELOPE_DIR  "/Envelope"
#endif

/* 最大文件数 (防止列表过长) */
#define FP_MAX_FILES    32

/* Channel ID mapping: dropdown index → channel_id */
static uint8_t g_acq_vib_ids[CHANNEL_DATA_MAX];
static uint8_t g_acq_gen_ids[CHANNEL_DATA_MAX];
static uint8_t g_acq_vib_cnt = 0;
static uint8_t g_acq_gen_cnt = 0;

/* Current params in memory */
static AcqParams_t g_acq_params;

/* ==================== Forward declarations ==================== */
static void acq_filter_mode_cb(lv_event_t *e);
static void acq_btn_apply_cb(lv_event_t *e);
static void acq_btn_browse_cb(lv_event_t *e);
static void acq_load_params(void);
static void acq_apply_to_ui(void);
static void acq_refresh_channel_dropdowns(void);

/* ==================== Helper: create a row (label + control + unit) ==================== */

/**
 * @brief  Create a horizontal row inside a group: [label | control | unit_hint]
 * @return The control object (textarea used as number input)
 */
static lv_obj_t *acq_add_spinbox_row(lv_obj_t *parent, const char *label_text,
                                      const char *unit_text,
                                      int32_t min_val, int32_t max_val,
                                      uint8_t digits, uint8_t dec_pos,
                                      uint32_t step)
{
    (void)min_val; (void)max_val; (void)digits; (void)dec_pos; (void)step;

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), ACQ_ROW_H);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_gap(row, 4, 0);

    /* Label */
    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_width(lbl, ACQ_LABEL_W);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x333333), 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);

    /* Textarea (number input, no leading zeros) */
    lv_obj_t *ta = lv_textarea_create(row);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, 8);
    lv_textarea_set_text(ta, "0");
    lv_obj_set_width(ta, ACQ_CTRL_W);
    lv_obj_set_height(ta, 28);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_12, 0);
    lv_obj_set_style_pad_top(ta, 4, 0);
    lv_obj_set_style_pad_bottom(ta, 4, 0);
    lv_obj_clear_flag(ta, LV_OBJ_FLAG_SCROLLABLE);

    /* Unit hint */
    if (unit_text != NULL && unit_text[0] != '\0') {
        lv_obj_t *unit = lv_label_create(row);
        lv_label_set_text(unit, unit_text);
        lv_obj_set_style_text_font(unit, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(unit, lv_color_hex(0x999999), 0);
    }

    return ta;
}

/** @brief Read integer value from a number textarea */
static int32_t acq_ta_get_int(lv_obj_t *ta)
{
    const char *t = lv_textarea_get_text(ta);
    return (t && t[0]) ? (int32_t)atoi(t) : 0;
}

/** @brief Set integer value to a number textarea (no leading zeros) */
static void acq_ta_set_int(lv_obj_t *ta, int32_t val)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", (int)val);
    lv_textarea_set_text(ta, buf);
}

static lv_obj_t *acq_add_dropdown_row(lv_obj_t *parent, const char *label_text,
                                       const char *options)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), ACQ_ROW_H);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_gap(row, 4, 0);

    /* Label */
    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_width(lbl, ACQ_LABEL_W);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x333333), 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);

    /* Dropdown */
    lv_obj_t *dd = lv_dropdown_create(row);
    lv_dropdown_set_options(dd, options);
    lv_obj_set_width(dd, ACQ_CTRL_W);
    lv_obj_set_height(dd, 28);
    lv_obj_set_style_text_font(dd, &lv_font_montserrat_12, 0);
    lv_obj_set_style_pad_top(dd, 4, 0);
    lv_obj_set_style_pad_bottom(dd, 2, 0);

    return dd;
}

/* ==================== Helper: create a bordered group ==================== */

static lv_obj_t *acq_create_group(lv_obj_t *parent, const char *title)
{
    GUI_LegendBoxCfg_t cfg;
    GUI_LegendBoxCfgInit(&cfg);
    cfg.box_transparent = true;
    cfg.flex_column     = true;
    cfg.inner_pad       = ACQ_GROUP_PAD;
    cfg.title_color     = 0x2979FF;
    return GUI_CreateLegendBox(parent, title, &cfg);
}

/* ==================== Filter mode change callback ==================== */

static void acq_filter_mode_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *dd = lv_event_get_target(e);
        uint16_t sel = lv_dropdown_get_selected(dd);
        /* sel: 0=None, 1=Smoothing, 2=SpeedFilter */
        if (sel == 2) {
            if (g_acq_row_rpm != NULL)
                lv_obj_clear_flag(g_acq_row_rpm, LV_OBJ_FLAG_HIDDEN);
        } else {
            if (g_acq_row_rpm != NULL)
                lv_obj_add_flag(g_acq_row_rpm, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/* ==================== Refresh channel dropdown options ==================== */

static void acq_refresh_channel_dropdowns(void)
{
    static char vib_opts[256];
    static char gen_opts[256];
    vib_opts[0] = '\0';
    gen_opts[0] = '\0';
    g_acq_vib_cnt = 0;
    g_acq_gen_cnt = 0;

    uint8_t count = ChannelData_GetCount();
    for (uint8_t i = 0; i < count; i++) {
        ChannelConfig_t *ch = ChannelData_GetByIndex(i);
        if (!ch || !ch->enabled) continue;

        if (ch->ch_type == CH_TYPE_VIBRATION) {
            g_acq_vib_ids[g_acq_vib_cnt++] = ch->channel_id;
            if (vib_opts[0] != '\0') strcat(vib_opts, "\n");
            strcat(vib_opts, ch->channel_name);
        } else {
            g_acq_gen_ids[g_acq_gen_cnt++] = ch->channel_id;
            if (gen_opts[0] != '\0') strcat(gen_opts, "\n");
            strcat(gen_opts, ch->channel_name);
        }
    }

    if (g_acq_vib_cnt == 0) { strcpy(vib_opts, "(none)"); g_acq_vib_cnt = 0; }
    if (g_acq_gen_cnt == 0) { strcpy(gen_opts, "(none)"); g_acq_gen_cnt = 0; }

    if (g_acq_dd_vib_ch) lv_dropdown_set_options(g_acq_dd_vib_ch, vib_opts);
    if (g_acq_dd_gen_ch) lv_dropdown_set_options(g_acq_dd_gen_ch, gen_opts);
}

/* ==================== Apply button callback ==================== */

static void acq_btn_apply_cb(lv_event_t *e)
{
    (void)e;

    /* Read all values from UI */
    g_acq_params.min_cycle_duration_s = (uint16_t)acq_ta_get_int(g_acq_spin_cycle);
    g_acq_params.vibration_ch_num     = (g_acq_vib_cnt > 0) ? g_acq_vib_ids[lv_dropdown_get_selected(g_acq_dd_vib_ch)] : 0;
    g_acq_params.general_ch_num       = (g_acq_gen_cnt > 0) ? g_acq_gen_ids[lv_dropdown_get_selected(g_acq_dd_gen_ch)] : 0;
    g_acq_params.filter_mode          = (uint8_t)lv_dropdown_get_selected(g_acq_dd_filter);
    g_acq_params.rpm                  = (uint16_t)acq_ta_get_int(g_acq_spin_rpm);
    g_acq_params.cycle_multiplier     = (uint8_t)acq_ta_get_int(g_acq_spin_cycle_mul);
    g_acq_params.moving_average       = (uint8_t)acq_ta_get_int(g_acq_spin_mov_avg);
    g_acq_params.general_threshold    = (float)acq_ta_get_int(g_acq_spin_gen_thr);
    g_acq_params.threshold_time_s     = (float)acq_ta_get_int(g_acq_spin_thr_time) / 10.0f;
    g_acq_params.alarm_threshold_pct  = (uint8_t)acq_ta_get_int(g_acq_spin_alarm_pct);
    g_acq_params.envelope_upper_pct   = (uint8_t)acq_ta_get_int(g_acq_spin_env_up);
    g_acq_params.envelope_lower_pct   = (uint8_t)acq_ta_get_int(g_acq_spin_env_low);

    const char *path = lv_textarea_get_text(g_acq_ta_env_path);
    snprintf(g_acq_params.envelope_file_path,
             sizeof(g_acq_params.envelope_file_path), "%s", path);

    /* ====== Validate per PRD 3.2.2 ====== */

    /* 分组一: 周期与通道 */
    if (g_acq_params.min_cycle_duration_s < 1 ||
        g_acq_params.min_cycle_duration_s > 3600) {
        GUI_Toast("Cycle duration must be 1~3600s", 2000);
        return;
    }

    /* 分组二: 滤波设置 */
    if (g_acq_params.filter_mode == 2) {
        /* 转速滤波: RPM 100~100000 */
        if (g_acq_params.rpm < 100 || g_acq_params.rpm > 100000) {
            GUI_Toast("RPM must be 100~100000", 2000);
            return;
        }
    }
    /* 周期倍数 0~100 (平滑/转速均适用) */
    if (g_acq_params.cycle_multiplier > 100) {
        GUI_Toast("Cycle multiplier must be 0~100", 2000);
        return;
    }
    /* 滑动平均 0~10 (平滑/转速均适用) */
    if (g_acq_params.moving_average > 10) {
        GUI_Toast("Moving average must be 0~10", 2000);
        return;
    }

    /* 分组三: 阈值设置 */
    if (g_acq_params.threshold_time_s < 0.1f ||
        g_acq_params.threshold_time_s > 10.0f) {
        GUI_Toast("Threshold time must be 0.1~10s", 2000);
        return;
    }
    if (g_acq_params.alarm_threshold_pct > 100) {
        GUI_Toast("Alarm threshold must be 0%~100%", 2000);
        return;
    }

    /* 分组四: 包络线设置 (整数百分比) */
    if (g_acq_params.envelope_upper_pct > 200) {
        GUI_Toast("Envelope upper must be 0~200%", 2000);
        return;
    }
    if (g_acq_params.envelope_lower_pct > 200) {
        GUI_Toast("Envelope lower must be 0~200%", 2000);
        return;
    }

    /* Save */
    if (Config_SaveAcqParams(&g_acq_params)) {
        AcqPipeline_RebuildRoute();
        GUI_Toast("Acq settings saved!", 2000);
    } else {
        GUI_Toast("Save failed!", 2000);
    }
}

/* ==================== 文件选择器弹窗 ==================== */

/**
 * @brief  扫描目录中的 .csv 文件，填充到 lv_list
 *
 * 平台隔离:
 *   - STM32: 使用 FatFs (f_opendir / f_readdir) 扫描 SD 卡目录
 *   - PC模拟器: 使用 dirent.h 扫描本地目录，目录不存在时提供 mock 文件
 */
static void fp_scan_files(lv_obj_t *list)
{
    lv_list_add_text(list, "  " ENVELOPE_DIR);

#if defined(PC_SIMULATOR)
    /* ---------- PC 模拟器: 用 dirent 扫描本地目录 ---------- */
    DIR *dir = opendir(ENVELOPE_DIR);
    if (dir) {
        struct dirent *entry;
        uint8_t count = 0;
        while ((entry = readdir(dir)) != NULL && count < FP_MAX_FILES) {
            /* 过滤: 只要 .csv 文件 */
            const char *name = entry->d_name;
            size_t len = strlen(name);
            if (len > 4 && strcmp(name + len - 4, ".csv") == 0) {
                lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_FILE, name);
                lv_obj_set_style_text_font(btn, &lv_font_montserrat_12, 0);
                count++;
            }
        }
        closedir(dir);
        if (count == 0) {
            lv_list_add_text(list, "  (no .csv files found)");
        }
    } else {
        /* 目录不存在时提供 mock 文件，方便测试 */
        lv_obj_t *b1 = lv_list_add_btn(list, LV_SYMBOL_FILE, "envelope_sample1.csv");
        lv_obj_t *b2 = lv_list_add_btn(list, LV_SYMBOL_FILE, "envelope_sample2.csv");
        lv_obj_t *b3 = lv_list_add_btn(list, LV_SYMBOL_FILE, "motor_baseline.csv");
        lv_obj_set_style_text_font(b1, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_font(b2, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_font(b3, &lv_font_montserrat_12, 0);
    }
#else
    /* ---------- STM32: 用 FatFs 扫描 SD 卡目录 ---------- */
    DIR fatfs_dir;
    FILINFO fno;
    FRESULT res = f_opendir(&fatfs_dir, ENVELOPE_DIR);
    if (res == FR_OK) {
        uint8_t count = 0;
        while (count < FP_MAX_FILES) {
            res = f_readdir(&fatfs_dir, &fno);
            if (res != FR_OK || fno.fname[0] == '\0') break;  /* 目录结束 */

            /* 过滤: 只要 .csv 文件 (忽略目录和隐藏文件) */
            size_t len = strlen(fno.fname);
            if (len > 4 && strcmp(fno.fname + len - 4, ".csv") == 0
                && !(fno.fattrib & AM_DIR)) {
                lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_FILE, fno.fname);
                lv_obj_set_style_text_font(btn, &lv_font_montserrat_12, 0);
                count++;
            }
        }
        f_closedir(&fatfs_dir);
        if (count == 0) {
            lv_list_add_text(list, "  (no .csv files found)");
        }
    } else {
        lv_list_add_text(list, "  (SD card not available)");
    }
#endif
}

/**
 * @brief  关闭文件选择器弹窗
 *
 * 关闭流程 (遵循焦点隔离规范):
 *   1. 恢复主焦点组为 default
 *   2. 删除弹窗对象
 *   3. 删除弹窗焦点组
 */
static void fp_close_popup(void)
{
    /* 1. 恢复主焦点组 */
    if (g_fp_main_group) {
        lv_group_set_default(g_fp_main_group);
    }
    /* 2. 删除弹窗对象 */
    if (g_fp_popup) {
        lv_obj_del(g_fp_popup);
        g_fp_popup = NULL;
        g_fp_list  = NULL;
    }
    /* 3. 删除弹窗焦点组 */
    if (g_fp_group) {
        lv_group_del(g_fp_group);
        g_fp_group = NULL;
    }
}

/**
 * @brief  文件列表项点击回调
 *
 * 获取被点击按钮的文本 (文件名)，拼接完整路径后回填到包络线文件输入框
 */
static void fp_list_item_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    const char *filename = lv_list_get_btn_text(g_fp_list, btn);
    if (filename && filename[0] != '\0') {
        /* 拼接完整路径并回填到 textarea */
        char full_path[32];
        snprintf(full_path, sizeof(full_path), "%s/%s", ENVELOPE_DIR, filename);
        if (g_acq_ta_env_path) {
            lv_textarea_set_text(g_acq_ta_env_path, full_path);
        }
    }
    fp_close_popup();
}

/**
 * @brief  取消按钮回调 - 关闭弹窗不做任何操作
 */
static void fp_cancel_cb(lv_event_t *e)
{
    (void)e;
    fp_close_popup();
}

/**
 * @brief  打开文件选择器弹窗
 *
 * 弹窗结构:
 *   ┌──────────── 全屏半透明背景 ────────────┐
 *   │                                       │
 *   │   ┌──── Select Envelope File ─────┐   │
 *   │   │  📂 /Envelope                 │   │
 *   │   │  📄 file1.csv                 │   │
 *   │   │  📄 file2.csv                 │   │
 *   │   │  📄 file3.csv                 │   │
 *   │   │                               │   │
 *   │   │        [ Cancel ]             │   │
 *   │   └───────────────────────────────┘   │
 *   └───────────────────────────────────────┘
 */
static void fp_open_popup(void)
{
    /* ---- 焦点隔离: 保存主 group, 创建弹窗专用 group ---- */
    g_fp_main_group = lv_group_get_default();
    if (g_fp_group) { lv_group_del(g_fp_group); g_fp_group = NULL; }
    g_fp_group = lv_group_create();
    lv_group_set_default(g_fp_group);

    /* ---- 全屏半透明背景 ---- */
    g_fp_popup = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_fp_popup, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(g_fp_popup, 0, 0);
    lv_obj_set_style_bg_color(g_fp_popup, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(g_fp_popup, LV_OPA_40, 0);
    lv_obj_set_style_border_width(g_fp_popup, 0, 0);
    lv_obj_set_style_radius(g_fp_popup, 0, 0);
    lv_obj_set_style_pad_all(g_fp_popup, 0, 0);
    lv_obj_clear_flag(g_fp_popup, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- 居中白色面板 ---- */
    lv_obj_t *panel = lv_obj_create(g_fp_popup);
    lv_obj_set_size(panel, 320, 300);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_radius(panel, 6, 0);
    lv_obj_set_style_pad_all(panel, 10, 0);
    lv_obj_set_style_pad_gap(panel, 6, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);

    /* ---- 标题 ---- */
    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, "Select Envelope File");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x333333), 0);
    lv_obj_set_style_pad_bottom(title, 4, 0);

    /* ---- 文件列表 (可滚动) ---- */
    g_fp_list = lv_list_create(panel);
    lv_obj_set_width(g_fp_list, LV_PCT(100));
    lv_obj_set_flex_grow(g_fp_list, 1);  /* 占满剩余空间 */
    lv_obj_set_style_text_font(g_fp_list, &lv_font_montserrat_12, 0);

    /* 扫描目录并填充文件列表 */
    fp_scan_files(g_fp_list);

    /* 为列表中的每个按钮注册点击回调 */
    uint32_t child_cnt = lv_obj_get_child_cnt(g_fp_list);
    for (uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(g_fp_list, i);
        /* 只给按钮 (文件项) 注册回调，跳过文本标签 (目录名/空提示) */
        if (lv_obj_check_type(child, &lv_btn_class)) {
            lv_obj_add_event_cb(child, fp_list_item_cb,
                                LV_EVENT_CLICKED, NULL);
        }
    }

    /* ---- 取消按钮 ---- */
    lv_obj_t *btn_cancel = lv_btn_create(panel);
    lv_obj_set_size(btn_cancel, 80, 30);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_bg_opa(btn_cancel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_cancel, 1, 0);
    lv_obj_set_style_border_color(btn_cancel, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_radius(btn_cancel, 4, 0);
    lv_obj_align(btn_cancel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_t *lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, "Cancel");
    lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_cancel, lv_color_hex(0x333333), 0);
    lv_obj_center(lbl_cancel);
    lv_obj_add_event_cb(btn_cancel, fp_cancel_cb, LV_EVENT_CLICKED, NULL);
}

/* ==================== Browse button callback ==================== */

/**
 * @brief  包络线文件浏览按钮回调
 *
 * 点击 "..." 按钮时打开文件选择器弹窗，
 * 用户从弹窗中选择 SD 卡上的 .csv 文件，
 * 选中后文件名自动回填到包络线文件路径输入框。
 */
static void acq_btn_browse_cb(lv_event_t *e)
{
    (void)e;
    fp_open_popup();
}

/* ==================== Load params from flash ==================== */

static void acq_load_params(void)
{
    g_acq_params = Config_LoadAcqParams();
}

/* ==================== Apply UI from params ==================== */

static void acq_apply_to_ui(void)
{
    acq_ta_set_int(g_acq_spin_cycle,     g_acq_params.min_cycle_duration_s);
    lv_dropdown_set_selected(g_acq_dd_vib_ch, 0);  /* default to first */
    for (uint8_t i = 0; i < g_acq_vib_cnt; i++) {
        if (g_acq_vib_ids[i] == g_acq_params.vibration_ch_num) {
            lv_dropdown_set_selected(g_acq_dd_vib_ch, i); break;
        }
    }
    lv_dropdown_set_selected(g_acq_dd_gen_ch, 0);
    for (uint8_t i = 0; i < g_acq_gen_cnt; i++) {
        if (g_acq_gen_ids[i] == g_acq_params.general_ch_num) {
            lv_dropdown_set_selected(g_acq_dd_gen_ch, i); break;
        }
    }
    lv_dropdown_set_selected(g_acq_dd_filter,  g_acq_params.filter_mode);
    acq_ta_set_int(g_acq_spin_rpm,       g_acq_params.rpm);
    acq_ta_set_int(g_acq_spin_cycle_mul, g_acq_params.cycle_multiplier);
    acq_ta_set_int(g_acq_spin_mov_avg,   g_acq_params.moving_average);
    acq_ta_set_int(g_acq_spin_gen_thr,   (int32_t)g_acq_params.general_threshold);
    acq_ta_set_int(g_acq_spin_thr_time,  (int32_t)(g_acq_params.threshold_time_s * 10.0f));
    acq_ta_set_int(g_acq_spin_alarm_pct, g_acq_params.alarm_threshold_pct);
    if (g_acq_spin_env_up != NULL)
        acq_ta_set_int(g_acq_spin_env_up,  g_acq_params.envelope_upper_pct);
    if (g_acq_spin_env_low != NULL)
        acq_ta_set_int(g_acq_spin_env_low, g_acq_params.envelope_lower_pct);
    if (g_acq_ta_env_path != NULL)
        lv_textarea_set_text(g_acq_ta_env_path, g_acq_params.envelope_file_path);

    /* Update RPM row visibility */
    if (g_acq_params.filter_mode == 2) {
        if (g_acq_row_rpm != NULL)
            lv_obj_clear_flag(g_acq_row_rpm, LV_OBJ_FLAG_HIDDEN);
    } else {
        if (g_acq_row_rpm != NULL)
            lv_obj_add_flag(g_acq_row_rpm, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ==================== Public create function ==================== */

void gui_acq_settings_create(lv_obj_t *parent)
{
    /* Remove tab page default padding */
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_pad_gap(parent, 0, 0);
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    /* ===== Header bar (full-width gray background, flush top-left) ===== */
    lv_obj_t *header_bar = lv_obj_create(parent);
    lv_obj_set_size(header_bar, LCD_WIDTH, 46);
    lv_obj_set_pos(header_bar, 0, 0);
    lv_obj_set_style_bg_color(header_bar, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_bg_opa(header_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header_bar, 0, 0);
    lv_obj_set_style_radius(header_bar, 0, 0);
    lv_obj_set_style_pad_all(header_bar, 0, 0);
    lv_obj_clear_flag(header_bar, LV_OBJ_FLAG_SCROLLABLE);

    /* Title label */
    lv_obj_t *title = lv_label_create(header_bar);
    lv_label_set_text(title, "Acq Settings");
    lv_obj_set_style_text_color(title, lv_color_hex(0x333333), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);

    /* Apply button in header bar */
    lv_obj_t *btn_apply = lv_btn_create(header_bar);
    lv_obj_set_size(btn_apply, 110, 28);
    lv_obj_align(btn_apply, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_bg_color(btn_apply, lv_color_hex(0x1E88E5), 0);
    lv_obj_set_style_bg_opa(btn_apply, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_apply, 0, 0);
    lv_obj_set_style_radius(btn_apply, 3, 0);
    lv_obj_t *lbl_apply = lv_label_create(btn_apply);
    lv_label_set_text(lbl_apply, "Apply");
    lv_obj_set_style_text_color(lbl_apply, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl_apply);
    lv_obj_add_event_cb(btn_apply, acq_btn_apply_cb, LV_EVENT_CLICKED, NULL);

    /* ===== Content area below header ===== */
    lv_obj_t *content = lv_obj_create(parent);
    lv_obj_set_size(content, LCD_WIDTH, LCD_HEIGHT - 46 - 36);
    lv_obj_set_pos(content, 0, 46);
    lv_obj_set_style_bg_color(content, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_style_pad_gap(content, 0, 0);
    lv_obj_set_style_radius(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* ===== Left panel ===== */
    lv_obj_t *left = lv_obj_create(content);
    lv_obj_set_size(left, ACQ_LEFT_W, LV_PCT(100));
    lv_obj_align(left, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left, 0, 0);
    lv_obj_set_style_pad_all(left, 6, 0);
    lv_obj_set_style_pad_gap(left, 6, 0);
    lv_obj_set_scroll_dir(left, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(left, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_flag(left, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(left, LV_OBJ_FLAG_SCROLL_MOMENTUM);

    /* ===== Group 1: 周期与通道 ===== */
    {
        lv_obj_t *g1 = acq_create_group(left, "Cycle & Channel");

        g_acq_spin_cycle = acq_add_spinbox_row(g1,
            "Min Cycle Duration(s)", "(1~3600)",
            1, 3600, 3, 0, 1);

        g_acq_dd_vib_ch = acq_add_dropdown_row(g1,
            "Vibration Channel", "(loading)");

        g_acq_dd_gen_ch = acq_add_dropdown_row(g1,
            "General Channel", "(loading)");
    }

    /* ===== Group 2: 滤波设置 ===== */
    {
        lv_obj_t *g2 = acq_create_group(left, "Filter Settings");

        g_acq_dd_filter = acq_add_dropdown_row(g2,
            "Filter Mode", "None\nSmoothing\nSpeedFilter");
        lv_obj_add_event_cb(g_acq_dd_filter, acq_filter_mode_cb,
                            LV_EVENT_VALUE_CHANGED, NULL);

        /* RPM row (hidden by default, shown only in SpeedFilter mode) */
        g_acq_row_rpm = lv_obj_create(g2);
        lv_obj_set_size(g_acq_row_rpm, LV_PCT(100), ACQ_ROW_H);
        lv_obj_set_flex_flow(g_acq_row_rpm, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(g_acq_row_rpm, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_clear_flag(g_acq_row_rpm, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_opa(g_acq_row_rpm, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(g_acq_row_rpm, 0, 0);
        lv_obj_set_style_pad_all(g_acq_row_rpm, 0, 0);
        lv_obj_set_style_pad_gap(g_acq_row_rpm, 4, 0);
        lv_obj_add_flag(g_acq_row_rpm, LV_OBJ_FLAG_HIDDEN);
        {
            lv_obj_t *lbl = lv_label_create(g_acq_row_rpm);
            lv_label_set_text(lbl, "Speed (RPM)");
            lv_obj_set_width(lbl, ACQ_LABEL_W);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0x333333), 0);

            g_acq_spin_rpm = lv_textarea_create(g_acq_row_rpm);
            lv_textarea_set_one_line(g_acq_spin_rpm, true);
            lv_textarea_set_max_length(g_acq_spin_rpm, 8);
            lv_textarea_set_text(g_acq_spin_rpm, "0");
            lv_obj_set_width(g_acq_spin_rpm, ACQ_CTRL_W);
            lv_obj_set_height(g_acq_spin_rpm, 28);
            lv_obj_set_style_text_font(g_acq_spin_rpm, &lv_font_montserrat_12, 0);
            lv_obj_set_style_pad_top(g_acq_spin_rpm, 4, 0);
            lv_obj_set_style_pad_bottom(g_acq_spin_rpm, 4, 0);
            lv_obj_clear_flag(g_acq_spin_rpm, LV_OBJ_FLAG_SCROLLABLE);
        }

        g_acq_spin_cycle_mul = acq_add_spinbox_row(g2,
            "Cycle Multiplier", "(0~100)",
            0, 100, 2, 0, 1);

        g_acq_spin_mov_avg = acq_add_spinbox_row(g2,
            "Moving Average", "(0~10)",
            0, 10, 2, 0, 1);
    }

    /* ===== Group 3: 阈值设置 ===== */
    {
        lv_obj_t *g3 = acq_create_group(left, "Threshold");

        g_acq_spin_gen_thr = acq_add_spinbox_row(g3,
            "General Threshold", NULL,
            0, 99999, 5, 0, 1);

        g_acq_spin_thr_time = acq_add_spinbox_row(g3,
            "Threshold Time(s)", "(0.1~10)",
            1, 100, 2, 1, 1);

        g_acq_spin_alarm_pct = acq_add_spinbox_row(g3,
            "Alarm Threshold(%)", "(0%~100%)",
            0, 100, 2, 0, 5);
    }

    /* ===== Group 4: 包络线设置 ===== */
    {
        lv_obj_t *g4 = acq_create_group(left, "Envelope");

        g_acq_spin_env_up = acq_add_spinbox_row(g4,
            "Envelope Upper(%)", NULL,
            0, 200, 3, 0, 1);

        g_acq_spin_env_low = acq_add_spinbox_row(g4,
            "Envelope Lower(%)", NULL,
            0, 200, 3, 0, 1);

        /* File path row: label + textarea + browse button */
        lv_obj_t *row_file = lv_obj_create(g4);
        lv_obj_set_size(row_file, LV_PCT(100), ACQ_ROW_H);
        lv_obj_set_flex_flow(row_file, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row_file, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_clear_flag(row_file, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_opa(row_file, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row_file, 0, 0);
        lv_obj_set_style_pad_all(row_file, 0, 0);
        lv_obj_set_style_pad_gap(row_file, 4, 0);

        lv_obj_t *lbl_file = lv_label_create(row_file);
        lv_label_set_text(lbl_file, "Envelope File");
        lv_obj_set_width(lbl_file, ACQ_LABEL_W);
        lv_obj_set_style_text_font(lbl_file, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl_file, lv_color_hex(0x333333), 0);

        g_acq_ta_env_path = lv_textarea_create(row_file);
        lv_textarea_set_one_line(g_acq_ta_env_path, true);
        lv_textarea_set_max_length(g_acq_ta_env_path, 31);
        lv_obj_set_width(g_acq_ta_env_path, 110);
        lv_obj_set_height(g_acq_ta_env_path, 28);
        lv_obj_set_style_text_font(g_acq_ta_env_path, &lv_font_montserrat_12, 0);
        lv_obj_set_style_pad_top(g_acq_ta_env_path, 4, 0);
        lv_obj_set_style_pad_bottom(g_acq_ta_env_path, 4, 0);
        lv_obj_clear_flag(g_acq_ta_env_path, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *btn_browse = lv_btn_create(row_file);
        lv_obj_set_size(btn_browse, 40, 28);
        lv_obj_set_style_bg_color(btn_browse, lv_color_hex(0xE0E0E0), 0);
        lv_obj_set_style_bg_opa(btn_browse, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn_browse, 1, 0);
        lv_obj_set_style_border_color(btn_browse, lv_color_hex(0xCCCCCC), 0);
        lv_obj_set_style_radius(btn_browse, 3, 0);
        lv_obj_t *lbl_br = lv_label_create(btn_browse);
        lv_label_set_text(lbl_br, "...");
        lv_obj_set_style_text_font(lbl_br, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl_br, lv_color_hex(0x333333), 0);
        lv_obj_center(lbl_br);
        lv_obj_add_event_cb(btn_browse, acq_btn_browse_cb, LV_EVENT_CLICKED, NULL);
    }

    /* ===== Right panel: 参数说明 + 系统状态 ===== */
    lv_obj_t *right = lv_obj_create(content);
    lv_obj_set_size(right, ACQ_RIGHT_W, LV_PCT(100));
    lv_obj_align(right, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(right, lv_color_hex(0xF5F5F5), 0);
    lv_obj_set_style_bg_opa(right, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(right, 1, 0);
    lv_obj_set_style_border_side(right, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_color(right, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_radius(right, 0, 0);
    lv_obj_set_style_pad_all(right, 12, 0);
    lv_obj_set_style_pad_gap(right, 6, 0);
    lv_obj_set_scroll_dir(right, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(right, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_flag(right, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(right, LV_OBJ_FLAG_SCROLL_MOMENTUM);

    /* Title: 参数说明 */
    lv_obj_t *lbl_title = lv_label_create(right);
    lv_label_set_text(lbl_title, "Parameter Description");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x333333), 0);
    lv_obj_set_style_pad_bottom(lbl_title, 6, 0);

    /* Helper: section header */
    #define ACQ_SECTION(txt) do { \
        lv_obj_t *_s = lv_label_create(right); \
        lv_label_set_text(_s, txt); \
        lv_obj_set_style_text_font(_s, &lv_font_montserrat_14, 0); \
        lv_obj_set_style_text_color(_s, lv_color_hex(0x2979FF), 0); \
        lv_obj_set_style_pad_top(_s, 8, 0); \
        lv_obj_set_style_pad_bottom(_s, 2, 0); \
    } while(0)

    /* Helper: description text */
    #define ACQ_DESC(txt) do { \
        lv_obj_t *_d = lv_label_create(right); \
        lv_label_set_text(_d, txt); \
        lv_label_set_long_mode(_d, LV_LABEL_LONG_WRAP); \
        lv_obj_set_width(_d, LV_PCT(100)); \
        lv_obj_set_style_text_font(_d, &lv_font_montserrat_12, 0); \
        lv_obj_set_style_text_color(_d, lv_color_hex(0x555555), 0); \
        lv_obj_set_style_pad_bottom(_d, 2, 0); \
    } while(0)

    ACQ_SECTION("Cycle & Channel");
    ACQ_DESC("* Min cycle duration controls envelope time length and calculation cycle");

    ACQ_SECTION("Filter Mode");
    ACQ_DESC("* None: Output raw signal directly");
    ACQ_DESC("* Smoothing: Moving average algorithm, averages data in window. Higher value = smoother curve but more distortion risk");
    ACQ_DESC("* SpeedFilter: Calculates bandpass filter center frequency based on RPM, retains signals within bandpass range");

    ACQ_SECTION("Threshold");
    ACQ_DESC("* General Threshold: Signal amount exceeding envelope bounds to trigger alarm (no fixed range, same unit as general channel)");
    ACQ_DESC("* Threshold Time: Delay time for triggering envelope after threshold is reached");
    ACQ_DESC("* Alarm Threshold: Alarm when peak-to-peak exceeds envelope for sufficient time ratio");

    ACQ_SECTION("Envelope");
    ACQ_DESC("* Upper/Lower: Percentage applied to baseline Y values to form envelope boundaries");
    ACQ_DESC("* Envelope File (optional): CSV with N data points forming a baseline polyline");
    ACQ_DESC("* If file provided: main display shows upper/lower envelope boundaries over [0, N] range");
    ACQ_DESC("* If no file: main display shows peak-to-peak curve only, no envelope bounds");

    /* ===== System Status ===== */
    lv_obj_t *status_sep = lv_obj_create(right);
    lv_obj_set_size(status_sep, LV_PCT(100), 1);
    lv_obj_set_style_bg_color(status_sep, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_bg_opa(status_sep, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(status_sep, 0, 0);
    lv_obj_set_style_pad_all(status_sep, 0, 0);
    lv_obj_set_style_pad_top(status_sep, 10, 0);
    lv_obj_clear_flag(status_sep, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_status_title = lv_label_create(right);
    lv_label_set_text(lbl_status_title, "System Status");
    lv_obj_set_style_text_font(lbl_status_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_status_title, lv_color_hex(0x333333), 0);
    lv_obj_set_style_pad_top(lbl_status_title, 6, 0);
    lv_obj_set_style_pad_bottom(lbl_status_title, 4, 0);

    /* Status row: LED + label */
    lv_obj_t *status_row = lv_obj_create(right);
    lv_obj_set_size(status_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(status_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(status_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(status_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(status_row, 0, 0);
    lv_obj_set_style_pad_all(status_row, 0, 0);
    lv_obj_set_style_pad_gap(status_row, 8, 0);

    g_acq_status_led = lv_obj_create(status_row);
    lv_obj_set_size(g_acq_status_led, 14, 14);
    lv_obj_set_style_radius(g_acq_status_led, 7, 0);
    lv_obj_set_style_bg_color(g_acq_status_led,
                              lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_style_border_width(g_acq_status_led, 0, 0);

    g_acq_status_label = lv_label_create(status_row);
    lv_label_set_text(g_acq_status_label, "Ready");
    lv_obj_set_style_text_font(g_acq_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_acq_status_label,
                                lv_palette_main(LV_PALETTE_GREEN), 0);

    /* Load saved parameters and populate UI */
    acq_load_params();
    acq_refresh_channel_dropdowns();
    acq_apply_to_ui();

    lv_obj_update_layout(left);
    lv_obj_update_layout(right);
}

void gui_acq_settings_destroy(lv_obj_t *parent)
{
    fp_close_popup();

    if (parent != NULL) {
        lv_obj_clean(parent);
    }

    g_acq_spin_cycle     = NULL;
    g_acq_dd_vib_ch      = NULL;
    g_acq_dd_gen_ch      = NULL;
    g_acq_dd_filter      = NULL;
    g_acq_spin_rpm       = NULL;
    g_acq_label_rpm      = NULL;
    g_acq_row_rpm        = NULL;
    g_acq_spin_cycle_mul = NULL;
    g_acq_spin_mov_avg   = NULL;
    g_acq_spin_gen_thr   = NULL;
    g_acq_spin_thr_time  = NULL;
    g_acq_spin_alarm_pct = NULL;
    g_acq_spin_env_up    = NULL;
    g_acq_spin_env_low   = NULL;
    g_acq_ta_env_path    = NULL;
    g_acq_status_led     = NULL;
    g_acq_status_label   = NULL;
    g_fp_main_group      = NULL;
}
