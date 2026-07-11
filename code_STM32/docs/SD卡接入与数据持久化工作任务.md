# SD 卡接入与数据持久化工作任务

> **工程**：`amp-vibe-analyzer/code_STM32`  
> **依据**：`docs/产品需求文档PRD.md`（§3 功能模块、§6 数据存储、§8 硬件）  
> **文档版本**：v1.1 | 2026-07-10  
> **进度口径**：软件实现以 amp 工程为准；**板端插卡联调 / 冒烟验收尚未做**（按需求暂缓测试）。

### 进度总览（截至 v1.1）

| 标记 | 含义 |
|------|------|
| ✅ | 代码已完成并编进固件 |
| ⏳ | 代码部分完成 / 能力受限 |
| 🔬 | 代码有了，板测/验收未做 |
| ❌ | 未做 |
| ⏸ | 明确后置（P3） |

| 阶段 | 状态 | 说明 |
|------|------|------|
| A 驱动落地 | ✅ 为主，A5 🔬 | CubeMX+FatFs+SdFs_Init 已通；冒烟读写未测 |
| B 工具层 | ✅ | `sd_fs` / `path_layout` + PC 后端；GUI 已去 `ff.h` |
| C 配置持久化 | ✅ 代码 / 🔬 板测 | `config_store` + `channel_data` 单路径 JSON |
| D 包络 CSV | ✅ 为主，D4 ❌ | Browse/加载走 `envelope_csv`；样例卡未备 |
| E 采集存盘 | ✅ 为主，E5 ❌ / E6 ⏳ | Start/Stop 会话 + 1s 缓冲刷盘；包络另存未做 |
| F 报警历史 | ✅ 为主，F4 ⏳ | SaveAlarm/Query/备注已接；历史曲线仅会话文件 |
| G 导出清理 | ✅/⏸ | CSV 导出+Cleanup 有；Excel/PNG/ZIP 后置 |
| H 验收 | 🔬/❌ | 分层 H7 ✅；其余依赖板测 |

---

## 0. 架构决策：抽离 SD 工具层（✅ 已完成）

> 实现位置：`user/app/src/storage/{sd_fs,path_layout,json_kv,config_codec,envelope_csv}.c`  
> 业务：`config_store.c` / `data_logger.c`（已替换 stub），`channel_data` 单路径落盘  
> GUI 经 `EnvelopeCsv_*` / `SdFs_*`，不再直调 FatFs  
> 采集 Start/Stop → `DataLogger_BeginSession/EndSession`；报警边沿 → `DataLogger_SaveAlarm`

### 0.1 想法是否可行？

**可行，且已按此落地。**

嵌入式里这相当于服务端的 Repository / DAO：把「文件系统 CRUD」从业务里抽出来，GUI / 配置 / 采集存盘只调统一 API，不直接散落 `f_open` / `f_write`。

| 对比 | 散落在各业务里 | 独立工具层（推荐） |
|------|----------------|-------------------|
| 改路径、换挂载盘符 | 改很多处 | 只改工具层 |
| FreeRTOS 下 FatFs 互斥 | 容易漏加锁 | 工具层统一加锁 |
| PC 模拟器 | 每处 `#ifdef` | 工具层内 PC 用 POSIX、板端用 FatFs |
| 单测 / 冒烟 | 难 | 可单独测 mount/读写 |
| GUI 职责 | 界面里夹驱动细节 | 界面只关心「保存成功/失败」 |

### 0.2 目录与职责（✅ 已按此实现）

```
user/
├── app/
│   ├── inlcude/
│   │   ├── storage/
│   │   │   ├── sd_fs.h / path_layout.h
│   │   │   ├── json_kv.h / config_codec.h / envelope_csv.h
│   │   ├── config_store.h
│   │   └── data_logger.h
│   └── src/
│       ├── storage/     # sd_fs / path_layout / json_kv / config_codec / envelope_csv
│       ├── config_store.c
│       ├── data_logger.c
│       ├── channel_data.c   # RAM 模型，Save → Config_SaveChannels
│       └── gui/screen_*.c   # 不碰 ff.h
└── bsp/                 # CubeMX/HAL/SDIO
```

### 0.3 `sd_fs` API（✅ 已实现）

```c
bool     SdFs_Init / IsReady / Deinit;
int      SdFs_LastError(void);
bool     SdFs_EnsureDir / Exists / Remove / Rename;
int32_t  SdFs_ReadAll / WriteAll / Append;
bool     SdFs_Open; int32_t SdFs_Read / Write; bool SdFs_Close;
bool     SdFs_ListDir(...);
```

**约束落实情况：**

1. ✅ 全局互斥（板端 `osMutex`）  
2. ✅ 采集侧缓冲刷盘（约 1s / 满缓冲），非每 20ms 直写  
3. ✅ 业务相对路径，盘符在 `sd_fs` 内拼接  
4. ✅ `SdFs_LastError()`；串口详细日志仍可加强（B5 ⏳）

### 0.4 与现有模块的关系（✅）

```
screen_*  → Config_Store / channel_data / EnvelopeCsv → sd_fs → FatFs/SDIO
acq_task / alarm_mgr → DataLogger ─────────────────────┘
```

### 0.5 PRD 目录布局（✅ `path_layout` + Init 建树）

| 逻辑用途 | 路径 | 状态 |
|----------|------|------|
| 配置 | `Config/` | ✅ |
| 包络 CSV | `Envelope/` | ✅ |
| 实时数据 | `RealData/年/月/` | ✅ |
| 报警 | `AlarmData/alarms.csv` | ✅ |
| 导出 | `Export/` | ✅ |

---

## 1. 总体阶段与优先级

| 阶段 | 名称 | 目标 | 优先级 | 状态 |
|------|------|------|--------|------|
| A | 驱动落地与冒烟 | CubeMX 产物可用，`SdFs_Init` + 读写测试 | P0 | ✅/🔬 A5 未测 |
| B | 存储工具层 | `sd_fs` + 目录布局 + 互斥 | P0 | ✅ |
| C | 配置持久化 | 通道 / 采集断电可恢复 | P0 | ✅ 代码 / 🔬 板测 |
| D | 包络 CSV | Browse + 主监控包络 | P1 | ✅/❌ D4 |
| E | 采集自动存盘 | Start 后写 RealData CSV | P1 | ✅/❌ E5 |
| F | 报警与历史 | DataLogger + 报警屏 | P2 | ✅/⏳ F4 |
| G | 导出与清理 | Export、30 天清理 | P2/P3 | ✅/⏸ |
| H | 验收 | 对照 PRD §6.2 | P2 | 🔬 待板测 |

---

## 2. 阶段 A — 驱动落地与冒烟（P0）

| ID | 任务 | 状态 | 备注 |
|----|------|------|------|
| A1 | CubeMX 生成落盘 | ✅ | `MX_SDIO_Init` / `MX_FATFS_Init`、`FATFS/`、`HAL_SD` 已在 amp |
| A2 | 移除 FatFs 桩冲突 | ✅ | `stub_fatfs.c` → `.bak`；用 Middleware `ff.h` |
| A3 | 启动调用链 | ✅ | `main`：`MX_FATFS_Init`；`StartGUITask`：`SdFs_Init`（含 `f_mount`） |
| A4 | 卡检测 PE2 | ✅ | `SdFs_Init` 内 `BSP_SD_IsDetected`；无卡返回 false |
| A5 | 冒烟读写测试 | 🔬/❌ | **未做**（暂缓测试） |
| A6 | 工程同步 | ⏳ | 业务源已同步 `code`/`code_STM32`；Keil 侧 FatFs 工程未完整接入 |

**卡要求**：MicroSD，**FAT32**（板测时再用）。

---

## 3. 阶段 B — SD 工具层 `storage/`（P0）

| ID | 任务 | 状态 | 备注 |
|----|------|------|------|
| B1 | `sd_fs.h/.c` | ✅ | FatFs + mutex；固件已链接 |
| B2 | `path_layout.h/.c` | ✅ | Init 时 `PathLayout_EnsureAll` |
| B3 | PC 模拟器后端 | ✅ | `./sd_root`；`pc_simulator/main.c` 调 `SdFs_Init` |
| B4 | GUI 禁止直调 FatFs | ✅ | `screen_acq_settings` / `screen_main_display` 已去 `ff.h` |
| B5 | 错误与日志 | ⏳ | 有 `SdFs_LastError`；关键失败串口打印未系统化 |

---

## 4. 阶段 C — 配置持久化（P0）

### 4.1 通道设置

| ID | 任务 | 状态 | 备注 |
|----|------|------|------|
| C1 | 统一路径 `Config/channel_config.json` | ✅ | `PATH_FILE_CHANNEL_CONFIG` |
| C2 | 板端 JSON 写 | ✅ | `ChannelData_Save` → `Config_SaveChannels` → `SdFs_WriteAll` |
| C3 | 板端 JSON 读 | ✅ | `Config_Init` / `ChannelData_Init`；无文件用默认 |
| C4 | 与 Config_Store 单路径 | ✅ | channel_data 不再自写 fopen |
| C5 | 校验保持 | ✅ | 沿用原 GUI 校验逻辑 |
| C6 | GUI Toast 反馈 | ✅ | 原有 Save 成功/失败提示 |

### 4.2 采集设置

| ID | 任务 | 状态 | 备注 |
|----|------|------|------|
| C7 | Acq JSON 走 sd_fs | ✅ | `Config/acq_settings.json`；stub 已替换 |
| C8 | 启动加载顺序 | ✅ | `SdFs_Init` → `ChannelData_Init`/`Config_Init` → GUI |
| C9 | 包络相对路径 | ✅ | 如 `Envelope/xxx.csv`，无盘符 |

### 4.3 出厂复位

| ID | 任务 | 状态 | 备注 |
|----|------|------|------|
| C10 | `Config_RestoreDefaults` | ✅ | 默认值 + 覆盖写 SD |

> 🔬 断电恢复等验收项见阶段 H（H1），板测未做。

---

## 5. 阶段 D — 包络 CSV（P1）

| ID | 任务 | 状态 | 备注 |
|----|------|------|------|
| D1 | Browse → `EnvelopeCsv_List` / `SdFs_ListDir` | ✅ | 无卡提示 `(SD not ready)` |
| D2 | 选中写回路径框 | ✅ | `Envelope/文件名` |
| D3 | 主监控 `EnvelopeCsv_LoadBaseline` | ✅ | 替代原 FatFs/`fopen` |
| D4 | 测试卡样例 CSV | ❌ | 需人工备卡 |
| D5 | GUI 去掉 `ff.h` | ✅ | |

---

## 6. 阶段 E — 采集过程自动存盘（P1）

| ID | 任务 | 状态 | 备注 |
|----|------|------|------|
| E1 | `data_logger.c` 实时部分 | ✅ | 替换 `stub_data_logger` |
| E2 | `RealData/年/月/时间-通道名.csv` | ✅ | |
| E3 | 缓冲 + 周期刷盘 | ✅ | 64 点 / 约 1s `SdFs_Append` |
| E4 | Start/Stop 生命周期 | ✅ | `BeginSession` / `EndSession` |
| E5 | 包络会话另存文件 | ❌ | 未做并列 `envelope_*.csv` |
| E6 | 磁盘满/无卡策略 | ⏳ | 写失败不崩；停存盘+UI 提示未完善 |

---

## 7. 阶段 F — 报警记录与历史查询（P2）

| ID | 任务 | 状态 | 备注 |
|----|------|------|------|
| F1 | `DataLogger_SaveAlarm` → `AlarmData/alarms.csv` | ✅ | |
| F2 | `QueryAlarms` 供报警屏 | ✅ | |
| F3 | 备注/状态写回 | ✅ | `DataLogger_UpdateAlarm` |
| F4 | `QueryHistory` 历史曲线 | ⏳ | **仅当前会话文件/缓冲**；跨文件扫描未做 |
| F5 | `alarm_manager` 边沿调 SaveAlarm | ✅ | |

**P3 仍不做：** SQLite、JPG、≥10 万条优化。

---

## 8. 阶段 G — 导出与清理（P2/P3）

| ID | 任务 | 状态 | 备注 |
|----|------|------|------|
| G1 | `ExportCSV` → `Export/` | ✅ | |
| G2 | Excel | ⏸ | 策略：先 CSV 用 Excel 打开 |
| G3 | PNG | ⏸ | P3 |
| G4 | ZIP | ⏸ | P3 |
| G5 | `DataLogger_Cleanup(30)` | ✅ | `GUI_Init` 时调用（粗扫月份目录） |
| G6 | 报警 1 年策略说明 | ⏳ | 无独立维护菜单；仅文档层 |

---

## 9. 阶段 H — 验收清单

| ID | 项 | 状态 |
|----|----|------|
| H1 | 配置断电恢复 | 🔬 待板测 |
| H2 | 包络 Browse + 显示 | 🔬 待板测 |
| H3 | 采集存盘 PC 可读 | 🔬 待板测 |
| H4 | 报警闭环 + 备注 | 🔬 待板测 |
| H5 | 连续采集 ≥30 min | ❌ 未测 |
| H6 | 无卡/拔卡/写保护 | 🔬/⏳ 代码有兜底，未系统测 |
| H7 | 业务/GUI 不依赖 `ff.h` | ✅ |

---

## 10. 明确不在本专项（或后置）

| 项 | 说明 |
|----|------|
| SPI Flash (W25Q64) 环形缓冲 | PRD §8.1；单开专项 |
| I2C EEPROM | 勿与 SD 配置混写 |
| SQLite | P3 |
| 真 Excel / PNG / ZIP | P3 |
| 主监控纯 UI | 不归本任务 |
| 触摸校准 / LVGL 内存 | 并行，不阻塞 |

---

## 11. 实施顺序与完成勾选

```
[✅] A1~A4 驱动落地          [🔬/❌] A5 冒烟
[✅] B1~B4 工具层            [⏳] B5 日志
[✅] C1~C10 配置持久化（代码）
[✅] D1~D3,D5 包络           [❌] D4 样例卡
[✅] E1~E4 采集存盘          [❌] E5  [⏳] E6
[✅] F1~F3,F5 报警           [⏳] F4 历史跨文件
[✅] G1,G5 导出/清理         [⏸] G2~G4  [⏳] G6
[✅] H7 分层                 [🔬] H1~H6 板测验收
```

---

## 12. 关键文件索引（现状 → 目标）

| 原状 | 目标 | 状态 |
|------|------|------|
| `stubs/stub_fatfs.c` + 桩 `ff.h` | CubeMX FatFs + `sd_fs` | ✅ |
| `stubs/stub_config_store.c` | `config_store.c` | ✅（`.bak`） |
| `stubs/stub_data_logger.c` | `data_logger.c` | ✅（`.bak`） |
| `channel_data.c` 板端路径待适配 | `Config/channel_config.json` | ✅ |
| `screen_acq_settings.c` 直调 `f_opendir` | `EnvelopeCsv_List` | ✅ |
| `screen_main_display.c` 读包络 | `EnvelopeCsv_LoadBaseline` | ✅ |
| `screen_alarm_records.c` 调桩 | DataLogger 真实现 | ✅ |
| （无）`user/app/src/storage/` | 工具层 | ✅ |

---

## 13. 风险与注意

1. ~~生成代码未落盘~~ → ✅ 已落盘 amp。  
2. **RAM**：已压缩 JSON/报警缓存；继续注意任务栈与刷盘缓冲。  
3. **实时性**：已用缓冲刷盘；板测时再看帧率影响（H5）。  
4. **双项目同步**：业务源已拷到工作区 `code` / `code_STM32`；Keil 完整编 SD 需另接 FatFs。  
5. **无 RTC**：时间戳为软时钟，目录年月可能与真实日历偏差，板测前建议接 RTC 或校准。

---

## 14. 下一步建议（未完成项）

1. **A5**：插 FAT32 卡做 `SdFs` 读写冒烟  
2. **H1~H4**：配置 / 包络 / 采集 / 报警闭环板测  
3. **E5 / F4 / E6**：包络另存、跨文件历史、写失败 UI  
4. **D4**：准备 `Envelope/` 样例 CSV  

---

*v1.1：按代码落地情况标注 ✅/⏳/🔬/❌/⏸；板测项统一标 🔬。*
