# LVGL 内存与刷新优化工作计划

> **文档版本**：v1.2  
> **创建日期**：2026-07-03  
> **修订日期**：2026-07-07  
> **适用项目**：`code`（Keil MDK + PC 模拟器）、`code_STM32`（STM32CubeIDE，业务 GUI 代码需同步）  
> **前置背景**：800×480 RA8875 + LVGL v8.4 + 八通道采集（AcqTask 50Hz 推送）；板端交互时「点两下卡住」，初步判断 LVGL 堆 OOM。

> **说明**：与 `code/doc/LVGL内存与刷新优化工作计划.md` 内容一致；主维护路径为 `code/doc/`。

---

## 1. 硬性约束（审阅前请确认）

### 1.1 本计划允许修改的范围

| 类别 | 路径 | 说明 |
|------|------|------|
| LVGL 库配置 | `Middlewares/Third_Party/LVGL/lv_conf.h` | 内存、控件开关、主题、字体、日志/监控 |
| PC 模拟器 LVGL 配置 | `pc_simulator/lv_conf.h` | 控件开关与 STM32 一致（PC 堆策略可不同） |
| LVGL 源码裁剪 | `user/app/src/lvgl_wrappers/*.c` | 关闭控件后移除对应 `#include` |
| GUI 业务界面 | `user/app/src/gui/*.c` | 含 `gui_calib.c`、各 `screen_*.c` |
| GUI 头文件 | `user/app/include/gui/*.h`（STM32：`inlcude/gui/`） | 对外 API |
| GUI 相关宏 | `user/app/include/config.h`（STM32：`inlcude/config.h`） | **`GUI_*`、`LVGL_*`、`GUI_CHART_POINTS_MAX` 等** |

### 1.2 本计划禁止修改的范围

| 类别 | 路径 | 原因 |
|------|------|------|
| 显示/触摸 BSP | `user/bsp/**`、`disp_ra8875_lvgl.c` | 硬件/驱动层 |
| 采集管线 | `acq_task.c`、`acq_pipeline.c` | 非 GUI；推送周期由 `ACQ_UI_UPDATE_MS` 定义 |
| 触摸存储 | `touch_calib.c` | Flash/矩阵，非 LVGL |
| 报警/通道/参数 | `alarm_manager.c`、`channel_data.c`、`param.c` | 业务层 |
| MCU / RTOS | `Core/**`、`freertos.c` | 平台层（任务栈、`osDelay` 需另开审批） |
| 驱动/HAL | `Drivers/**` | 硬件层 |
| PC 桩 | `stubs/**` | 非板端 GUI |

**`config.h` 中 `ACQ_*` 宏**：本计划**默认只读**；若需改 `ACQ_UI_UPDATE_MS` 降低推送频率，须单独审阅（影响 PRD 实时性）。

**阶段 0 例外（只读测量）**：允许在 `freertos.c` / `FreeRTOSConfig.h` **临时**调用 `uxTaskGetStackHighWaterMark` 记录 GUI/Acq 栈水位，**不得**作为量产改动。

### 1.3 约束带来的预期边界

- **可改善**：LVGL 堆占用、无效重绘、50Hz 推送下的堆碎片、交互 CPU、Tab 懒加载后的峰值。
- **本计划内无法消除**：驱动条带 flush（约 24 行/条）、FSMC 逐像素写、LCD 与 AD7606 总线争用、主 SRAM 中 **~75KB** 条带缓冲占用。
- **验收区分**：「Mem/CPU/堆稳定达标」≠「全屏无条带刷新」。

---

## 2. 现状基线（2026-07-07 代码审查）

### 2.1 硬件与 RAM 布局（F407VGT6）

| 区域 | 大小 | 当前主要用途 |
|------|------|----------------|
| CCMRAM | 64 KB | `LV_MEM_SIZE` 池（`.ccmram`，`work_mem_int[64KB]` 占满） |
| 主 SRAM | 128 KB | 条带缓冲、FreeRTOS 静态栈/堆、Acq 振动环、`.bss/.data` |
| Flash | 1 MB | LVGL + GUI ≈6400 行 + Acq 模块 |

**主 SRAM 粗算（v1.2 修正）**

| 项 | 估算 | 位置 |
|----|------|------|
| 条带缓冲 `s_buf1` | **≈75 KB**（38400 px × 2 B，`800×480/20`） | `disp_ra8875_lvgl.c` |
| GUI 任务栈 | 16 KB（4096×4） | `main.c` 静态栈 |
| Acq 任务栈 | 2 KB | `acq_task.c` |
| FreeRTOS 堆 | 28 KB | `FreeRTOSConfig.h` |
| 振动环等 `.bss` | ≈2 KB+ | `acq_pipeline.c` 等 |
| **合计（已分配）** | **≈123 KB / 128 KB** | 链接后余量极小 |

> v1.1 写「条带缓冲 ≈38.4KB」系将 **像素个数**（38400）误当作 KB；实际为 **~75 KB**。主 SRAM 紧张程度被低估。

AD7606 与 RA8875 **共用 FSMC**；GUI 少刷可减轻总线压力，但本计划不改 BSP。

### 2.2 关键配置与规模

| 指标 | 当前值 | 位置 |
|------|--------|------|
| LVGL 堆 | 64 KB（CCMRAM） | `lv_conf.h` `LV_MEM_SIZE` |
| 条带缓冲 | **38400 px ≈75 KB**（主 SRAM） | `disp_ra8875_lvgl.c` `DISP_BUF_SIZE` |
| `config.h` 中 `LVGL_BUF_SIZE` | 40000 px（**未用于 BSP**） | `config.h`（与驱动不一致，仅文档/历史遗留） |
| 图表点数（STM32） | 100（≈2 s 滚动窗） | `GUI_CHART_POINTS_MAX` |
| UI 推送周期 | 20 ms（50Hz） | `ACQ_UI_UPDATE_MS` |
| GUI 源码 | **约 6420 行 / 8 文件** | `user/app/src/gui/` |
| GUI 任务 | 5 ms `lv_timer_handler` | `freertos.c` |
| `LV_USE_LOG` | **1（已开）** | `lv_conf.h`（量产应关） |
| `LV_USE_ASSERT_MALLOC` | **1** | 分配失败 → `while(1)` 卡死 |
| 默认主题 | `LV_THEME_DEFAULT_DARK=1`、`GROW=1` | `lv_conf.h`（`GUI_Init` 覆写为浅色，但库默认仍暗色+Grow） |

### 2.3 故障现象与根因假设（「点两下卡住」）

| 场景 | 第 1 次点击 | 第 2 次点击 | 可能结果 |
|------|-------------|-------------|----------|
| **A. Tab 切换** | 启动已在 Channel Setup（`gui_show_tab` 懒加载） | 切到 Acq Settings / Main Display / Alarm Records → `gui_load_tab` 首次 `*_create` | **Main Display 最重**（双 chart + 多 series + 定时器）；LVGL 堆分配失败 → `LV_ASSERT_MALLOC` → **死循环** |
| **B. 通道表编辑** | 点表格单元格 | 弹出 `lv_keyboard` + overlay | 键盘控件大，叠加 Channel Setup 已有 table/右栏 → OOM |
| **C. 报警页日历** | 进入 Alarm Records Tab | 点日历按钮 → `lv_calendar` + header dropdown | 日历+dropdown 峰值 |
| **D. 栈溢出** | 同上 | 重页 `*_create` 嵌套深 | HardFault 或随机卡死（需 0.5 栈水位排除） |

**当前最吻合 A**：懒加载只 **创建不销毁**，第 2 次进入新 Tab 时堆峰值叠加；若第 2 次点是 **Main Display**，峰值最高。

**与内存相关的卡死链**：

```
gui_load_tab / lv_keyboard_create / lv_mem_alloc
  → 失败 → LV_USE_ASSERT_MALLOC → LV_ASSERT_HANDLER while(1)
  → 表现为「界面完全卡住、触摸无响应」
```

### 2.4 已实现优化（v1.0 之后）

| 能力 | 位置 | 说明 |
|------|------|------|
| **Tab 懒加载** | `gui_core.c` → `gui_load_tab` | 首次进入 Tab 才 `*_create`，缓解**启动** OOM |
| **Tab 显隐** | `gui_show_tab` | 隐藏页 `LV_OBJ_FLAG_HIDDEN`（**不释放**已加载 Tab） |
| **非 Main 不写 chart** | `gui_apply_realtime_payload` | `g_current_screen != MAIN` 则 return |
| **线程安全推送** | `GUI_PostRealtimeData` | STM32：`lv_mem_alloc` + `lv_async_call` |
| **Acq 集成** | `GUI_Init` → `AcqTask_Init` | 50Hz 实时曲线 |

### 2.5 仍存在的优化缺口（v1.2 增补）

| 缺口 | 影响 |
|------|------|
| 非 Main Tab 时仍 **每 20ms alloc + async**（`GUI_PostRealtimeData`） | 堆抖动、GUI 线程无效唤醒 |
| **`GUI_PostAlarmStatus` / `GUI_PostAlarmPopup` 同样每帧堆分配** | 3.6 未覆盖的 alloc 路径 |
| **无 payload 静态池** | 50Hz × alloc 易碎片化 |
| **Tab 只增不毁** | 4 Tab 全进内存后堆占用单调涨，无回落 |
| `lv_conf.h` 启用大量未用控件 + **LV_USE_LOG=1** | Flash/RAM/CPU |
| 多档 Montserrat 字体同时启用（10/12/14/16） | Flash + 少量运行时 |
| 校准 `gui_calib` 50ms 定时器 | 生命周期待 5.4 验证 |
| **阶段 1~5 部分任务仅写「同 v1.0」** | 执行人缺少可操作细节（v1.2 已展开） |

---

## 3. 阶段总览

| 阶段 | 名称 | 任务数 | 主要目标 | 风险 |
|------|------|--------|----------|------|
| **0** | 基线测量与监控 | **5** | 量化基线、复现「点两下」 | 低 |
| **1** | LVGL 配置瘦身 | **6** | Flash/RAM/日志 | 中 |
| **2** | 主题与样式减负 | 3 | 降绘制成本 | 低 |
| **3** | 可见性与推送优化 | **7** | Post* 路径 + Tab 定时器 | 中 |
| **4** | 图表与表格绘制 | 4 | chart/table DRAW | 中高 |
| **5** | 生命周期与堆稳态 | **5** | 弹层/校准/Tab 卸载 | 中~高 |

**合计：6 阶段，30 项任务**（v1.2：+0.5/1.5/1.6/3.7/5.5；展开原「同 v1.0」条目；修正 RAM 基线）。

### 3.1 推荐实施顺序（相对 v1.1 调整）

| 优先级 | 任务 | 理由 |
|--------|------|------|
| **P0** | 0.1 + 0.5 | 先确认是否 LVGL Mem 触顶 / 哪次点击触顶 |
| **P0** | **3.6** | 低风险高收益，消除 50Hz 无效 alloc |
| **P0** | **3.7** | 与 3.6 同模式，覆盖报警 async |
| **P1** | 3.5、1.5、1.2 | 降 CPU、Release 日志、关未用控件 |
| **P1** | 5.1~5.3、5.5 | 弹层与 Tab 峰值（**5.5 对「点两下」最关键**） |
| **P2** | 1~2、4 全阶段 | 视觉/绘制细优化 |

> **说明**：不必等阶段 0 全部填表再动 P0；但 **0.1 应与新固件同烧**，否则无法验收。

---

## 4. 阶段 0：基线测量与监控接入

> **边界**：仅临时监控与记录；不改业务逻辑（0.5 栈测量除外，只读）。

### 任务 0.1 — 开启 LVGL Mem/Perf Monitor（临时）

| 项 | 内容 |
|----|------|
| **修改文件** | `lv_conf.h` |
| **工作** | `LV_USE_MEM_MONITOR=1`，`LV_USE_PERF_MONITOR=1` |
| **校验** | 业务界面可见 overlay；记录 **Mem used / Mem max / Frag%** |

### 任务 0.2 — 记录基线数据表（§15）

| 项 | 内容 |
|----|------|
| **工作** | 4 Tab 空闲；Main 上 Acq 50Hz；Tab 切换；弹键盘/日历/文件列表 |
| **校验** | 表填完整，含固件日期、`GUI_TOUCH_CALIB_MODE` |

### 任务 0.3 — 最坏弹层 + 首次 Tab 加载峰值

| 项 | 内容 |
|----|------|
| **工作** | 各 Tab **首次** `gui_load_tab` 后 Mem 峰值；Channel Setup 键盘 + Alarm 日历 |
| **校验** | 记录哪一 Tab/弹层导致 Mem>90%；是否触发卡死 |

### 任务 0.4 — Acq 推送路径基线

| 项 | 内容 |
|----|------|
| **工作** | ① Main + Acq ② 非 Main + Acq ③ 对比 Mem 波动 |
| **预期** | ② 在 3.6 前仍应看到 50Hz 级 alloc 开销 |
| **校验** | 记录非 Main 时 Mem 是否锯齿状波动 |

### 任务 0.5 — 「点两下」复现矩阵（v1.2 新增）

| 项 | 内容 |
|----|------|
| **工作** | 按 §2.3 表格逐项复现：A1~A4 各 Tab 首次切换；B 通道名编辑；C 报警日历；每项记录 **第几次点击、Mem%、是否卡死** |
| **可选** | `uxTaskGetStackHighWaterMark` 记录 GUI/Acq 栈剩余（阶段 0 临时） |
| **校验** | 明确 **最小复现路径**（写入 §15 备注列） |

---

## 5. 阶段 1：LVGL 配置瘦身

### 任务 1.1 — 控件使用清单

**已知使用**：btn、btnmatrix、label、table、chart、dropdown、textarea、spinbox、keyboard、list、calendar、msgbox、spinner、obj；`gui_calib` 仅用 btn/label/obj。

**已知未使用（可关 `LV_USE_*=0` 并裁 wrapper）**：meter、colorwheel、menu、tileview、win、animimg、arc、canvas、imgbtn、roller、switch、tabview、led、span、checkbox、slider、bar、line、img 等（以 grep `lv_xxx_create` 交叉验证为准）。

### 任务 1.2 — 关闭未用控件

| 项 | 内容 |
|----|------|
| **修改文件** | `lv_conf.h`、`lvgl_wrappers/` 全部 |
| **工作** | 关控件 → 删对应 `#include` 与注册，两项目同步编译 |
| **校验** | 全 Tab + 键盘/日历/文件列表功能正常 |

### 任务 1.3 — 关闭未用 Extra / Theme 分支

| 项 | 内容 |
|----|------|
| **工作** | 关 `LV_USE_*` 中 chart 以外的 extra（如 fragment、gridnav 等未用项） |
| **校验** | 链接无 unresolved symbol |

### 任务 1.4 — 双项目编译回归

| 项 | 内容 |
|----|------|
| **工作** | `code` Keil + `code_STM32` CubeIDE + PC `build_and_run.ps1` |
| **校验** | 三端通过 |

### 任务 1.5 — Release 关闭 LVGL 日志（v1.2 新增）

| 项 | 内容 |
|----|------|
| **修改文件** | `lv_conf.h` |
| **工作** | 板端量产配置：`LV_USE_LOG=0`；调试固件可单独开 |
| **预期** | 减少 printf 与字符串常量，降低交互 CPU |
| **校验** | 板端无 `[Error] lv_mem` 以外的大量串口刷屏 |

### 任务 1.6 — 字体裁剪（v1.2 新增）

| 项 | 内容 |
|----|------|
| **工作** | 统计 GUI 实际 `lv_font_montserrat_*` 引用；关闭未用字号（当前启 10/12/14/16） |
| **边界** | 不改中文/自定义字库路径 |
| **校验** | 视觉无缺字；Flash map 体积下降 |

---

## 6. 阶段 2：主题与样式减负

### 任务 2.1 — 关闭 GROW 动画

| 项 | 内容 |
|----|------|
| **修改文件** | `lv_conf.h` |
| **工作** | `LV_THEME_DEFAULT_GROW=0` |
| **校验** | 按钮按下无缩放动画；点击 CPU 略降 |

### 任务 2.2 — 扁平化默认主题

| 项 | 内容 |
|----|------|
| **工作** | `LV_THEME_DEFAULT_DARK=0` 与 `GUI_Init` 浅色一致；减少 shadow/outline（各 screen 局部样式已部分扁平） |
| **校验** | 产品 UI 仍可接受 |

### 任务 2.3 — 可选关闭复杂绘制

| 项 | 内容 |
|----|------|
| **工作** | 评估 `LV_DRAW_COMPLEX=0`（圆角/阴影/渐变失效） |
| **边界** | 若 chart/按钮圆角不可接受则保持 1 |
| **校验** | A/B 对比 CPU 与截图 |

---

## 7. 阶段 3：可见性与推送优化

> **边界**：主改 `gui_core.c`、`screen_main_display.c`；**不改** `acq_task.c`、**不修改** `ACQ_UI_UPDATE_MS`（除非审阅特批）。

### 任务 3.1 — 验证「非 Main 不写 chart」（已实现）

| 项 | 内容 |
|----|------|
| **工作** | 确认 `gui_apply_realtime_payload` 中 `g_current_screen` 判断 |
| **校验** | 非 Main 时 chart 冻结；切回 Main 曲线连续 |

### 任务 3.2 — 验证 Tab HIDDEN（已实现）

| 项 | 内容 |
|----|------|
| **校验** | Tab 切换无整屏闪烁异常 |

### 任务 3.3 — 合并 `lv_chart_refresh`

| 项 | 内容 |
|----|------|
| **修改文件** | `screen_main_display.c`、`screen_alarm_records.c` |
| **工作** | 同一帧内多次 `lv_chart_set_next_value` 后 **单次** refresh |
| **校验** | 曲线语义不变；DRAW 次数减少 |

### 任务 3.4 — 实时推送合并（修订）

| 项 | 内容 |
|----|------|
| **修改文件** | `gui_core.c` |
| **工作** | **不得**低于 50Hz。允许：同一 GUI 周期内合并多次 payload（保留最后一次） |
| **校验** | Main Tab 曲线仍 50Hz；CPU 略降或持平 |

### 任务 3.5 — Main 页 timer pause/resume

| 项 | 内容 |
|----|------|
| **修改文件** | `screen_main_display.c`、`gui_core.c`（切 Tab 时调用） |
| **工作** | 离开 Main 时 pause `s_time_timer`、`s_blink_timer`；回到 Main resume |
| **校验** | 非 Main 时 CPU 降；回 Main 时间/闪烁正常 |

### 任务 3.6 — `GUI_PostRealtimeData` 前置过滤 + 静态池（**P0**）

| 项 | 内容 |
|----|------|
| **修改文件** | `gui_core.c` |
| **工作** | ① `g_current_screen != MAIN` 或 chart 未创建 → **return** ② **静态双缓冲**替代 `lv_mem_alloc` ③ async 回调使用槽位交换，禁止在 Acq 线程 free LVGL 内存 |
| **校验** | 0.4 对比：非 Main+Acq Mem 平稳；30min 无单调上涨 |

### 任务 3.7 — 报警 Post 路径静态池（v1.2 新增）

| 项 | 内容 |
|----|------|
| **修改文件** | `gui_core.c` |
| **工作** | `GUI_PostAlarmStatus`、`GUI_PostAlarmPopup` 同 3.6：前置过滤 + 静态 payload 槽 |
| **边界** | 报警频率低，但 alloc 失败同样触发 assert |
| **校验** | 模拟报警弹窗 Mem 无尖峰 |

---

## 8. 阶段 4：图表与表格绘制优化

### 任务 4.1 — Chart `DRAW_PART_BEGIN` 轻量化

| 项 | 内容 |
|----|------|
| **修改文件** | `screen_main_display.c` |
| **工作** | 审查 `chart_draw_event_cb`：减少每点重复样式计算 |
| **边界** | 不改 PP/包络语义 |

### 任务 4.2 — Table 局部刷新

| 项 | 内容 |
|----|------|
| **修改文件** | `screen_channel_setup.c`、`screen_alarm_records.c` |
| **工作** | 数据变更时 `lv_table_set_cell_value` 替代整表重建 |
| **校验** | 编辑通道后表格正确 |

### 任务 4.3 — 轴刻度/标签减绘

| 项 | 内容 |
|----|------|
| **工作** | 评估减少 `lv_chart_set_axis_tick` 密度或隐藏次要刻度 |
| **校验** | 主监控仍可读 |

### 任务 4.4 — Alarm chart 按需创建

| 项 | 内容 |
|----|------|
| **修改文件** | `screen_alarm_records.c` |
| **工作** | 选中记录后再创建/刷新 chart，避免 Tab 加载即占峰值 |
| **校验** | 报警曲线查询功能不变 |

**边界**：STM32 `GUI_CHART_POINTS_MAX` 保持 **100**，除非阶段 0 堆余量 >30% 且特批。

---

## 9. 阶段 5：控件生命周期与内存稳态

### 任务 5.1 — 键盘/编辑弹层销毁

| 项 | 内容 |
|----|------|
| **修改文件** | `screen_channel_setup.c` |
| **工作** | 关闭 keyboard overlay 时 `lv_obj_del` + 指针置 NULL；`g_popup_group` 生命周期一致 |
| **校验** | 反复打开/关闭名称编辑，Mem 回落 |

### 任务 5.2 — 日历/文件列表弹层销毁

| 项 | 内容 |
|----|------|
| **修改文件** | `screen_alarm_records.c`、`screen_acq_settings.c` |
| **工作** | `g_dp_popup`、`g_fp_popup` 关闭后彻底删除 |
| **校验** | 5 次开闭日历 Mem 稳定 |

### 任务 5.3 — Spinner/Toast 轻量化

| 项 | 内容 |
|----|------|
| **修改文件** | `gui_core.c` |
| **工作** | 确认 `GUI_ShowLoading`/`GUI_Toast` 成对销毁（现有逻辑基本具备，补漏测） |

### 任务 5.4 — 校准界面定时器与对象销毁

| 项 | 内容 |
|----|------|
| **修改文件** | `gui_calib.c` |
| **工作** | `g_poll_timer` / `g_result_timer` 在阶段切换、退出时 `lv_timer_del` |
| **校验** | 5 点校准 + 验证 50 轮 Mem 回落 |

### 任务 5.5 — Tab 内存预算与卸载策略（v1.2 新增，**P1**）

| 项 | 内容 |
|----|------|
| **修改文件** | `gui_core.c`、各 `screen_*.c`（需新增 `*_destroy` 或 recreate 入口） |
| **方案 A（推荐）** | 切换 Tab 时 **销毁非当前 Tab 内容**（`lv_obj_clean(page)` + `g_tab_loaded[i]=false`），仅保留当前页 |
| **方案 B（保守）** | 仅允许 **2 个 Tab 同时在内存**（当前 + 上一个），其余卸载 |
| **方案 C（仅降压）** | Main Display 拆分：`chart`/legend 延迟到首次进入 Main 再创建 |
| **边界** | 销毁前 pause Acq 推送或确保 3.6 已前置过滤；Save 后状态需可重建 |
| **校验** | **连续切换 4 Tab 10 轮** Mem 峰值不单调上涨；§2.3 场景 A **不再卡死** |

---

## 10. 阶段验收总表

| 阶段 | 必过条件 | 建议指标 |
|------|----------|----------|
| 0 | 基线表含「点两下」最小复现 | Mem max、栈水位 |
| 1 | 全 Tab/弹层回归 | Flash↓；堆峰值↓ |
| 2 | 视觉可接受 | 点击 CPU↓5%+ |
| 3 | **3.6+3.7 必做** | 非 Main+Acq Mem 平稳；Main 50Hz 保持 |
| 4 | 曲线/表格语义不变 | chart DRAW CPU↓10%+ |
| 5 | **5.5 必过（板端）** | 4 Tab 往返无卡死；30min 堆稳定 |

**条带刷新仍明显** → 驱动专项，**不视为本计划失败**。

---

## 11. 双项目同步

`gui_core.c`、各 `screen_*.c`、`gui_calib.c`、`config.h`、`lv_conf.h`（控件开关一致）、`lvgl_wrappers/` 须 `code` ↔ `code_STM32` 同步。

---

## 12. 与《项目说明文档》关系

- 应用分层见 `docs/项目说明文档.md` §八。
- 本计划**不替代** `八通道采集与主监控显示工作任务.md`。

---

## 13. 审阅清单

- [ ] 同意硬件/BSP/Acq 不在本计划修改
- [ ] 同意 **3.6+3.7 静态池** 为 P0
- [ ] 同意 **5.5 Tab 卸载** 为解决「点两下」的关键项（选 A/B/C）
- [ ] 同意 **不做 100~200ms 降采样**（除非另审 `ACQ_UI_UPDATE_MS`）
- [ ] 阶段 2 扁平化程度：轻微 / 明显 / 仅关动画
- [ ] 接受 v1.2 RAM 修正（条带 **~75KB** 主 SRAM）
- [ ] 实施顺序：0.1/0.5 → 3.6/3.7 → 5.5 → 1/2/4

---

## 14. 合理性分析摘要（v1.2）

| 维度 | 评价 |
|------|------|
| **问题诊断** | 合理。懒加载 + 64KB LVGL 堆 + assert 卡死链与「点两下」高度吻合 |
| **3.6 静态池** | 必要但不充分：解决 50Hz 碎片，**不解决** 第 2 Tab 创建峰值 |
| **阶段顺序** | v1.1 的 0→1→…→5 偏「全面优化」；板端救急应 **3.6 → 5.5 → 1.x** |
| **禁止改 BSP** | 合理（硬件约束），但文档须明示 **主 SRAM 已近乎满载**，扩容 LVGL 堆只能挤占 CCMRAM（已满）或动 BSP |
| **禁止改 freertos** | 过严；**只读栈水位**应允许；若 0.5 显示 GUI 栈 <512B 余量，需单独立项 |
| **缺失项（已补）** | Tab 卸载、报警 Post、LOG/字体、RAM 单位错误、复现矩阵 |
| **风险项** | 5.5 方案 A 改动面大，需各 screen 支持 destroy/recreate；可先上 **5.5C + 3.6** 验证 |

---

## 15. 基线记录表

| 场景 | Tab/操作 | FPS | CPU% | LVGL Mem | Mem Max | 备注 |
|------|----------|-----|------|----------|---------|------|
| 启动完成 | Channel Setup | | | | | 默认首 Tab |
| **点两下复现** | （填最小路径） | | | | | §0.5 |
| 空闲 | 各 Tab 各一次 | | | | | 首次加载峰值 |
| Acq 运行 | Main Display | | | | | 50Hz |
| Acq 运行 | Channel Setup | | | | | 3.6 前后对比 |
| Tab 往返×10 | 4 Tab 循环 | | | | | 5.5 验收 |
| 弹键盘 | Channel Setup | | | | | |
| 弹日历 | Alarm Records | | | | | |
| 弹文件列表 | Acq Settings | | | | | |
| 校准向导 | Calib | | | | | MODE=1 |

---

## 16. 修订记录

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0 | 2026-07-03 | 初稿 22 任务 |
| v1.1 | 2026-07-04 | 对齐 Acq/Tab 懒加载；+0.4/3.6/5.4；25 任务 |
| v1.2 | 2026-07-07 | 修正条带 RAM；+§2.3 点两下根因；+0.5/1.5/1.6/3.7/5.5；展开 1~5 任务细节；+§14 合理性分析；30 任务 |
