cmake_minimum_required(VERSION 3.15)
project(lvgl_pc_simulator C)

set(CMAKE_C_STANDARD 99)

# SDL2 路径
set(SDL2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SDL2/SDL2-2.30.3/x86_64-w64-mingw32")

# LVGL 路径
set(LVGL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/Third_Party/LVGL")

# 用户代码路径
set(USER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../user")

# SDL2 头文件和库文件
include_directories(${SDL2_DIR}/include/SDL2)
link_directories(${SDL2_DIR}/lib)

# LVGL 头文件 - 注意：pc_simulator 目录必须在 LVGL 目录之前
# 以确保使用 pc_simulator/lv_conf.h 而非 LVGL/lv_conf.h
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${LVGL_DIR})
include_directories(${LVGL_DIR}/src)

# 用户代码头文件（code_STM32 目录名为 inlcude，与 Keil/CubeIDE 一致）
include_directories(${USER_DIR}/app/inlcude)
include_directories(${USER_DIR}/app/inlcude/gui)

# 定义 PC_SIMULATOR 宏和 LV_CONF_INCLUDE_SIMPLE
add_definitions(-DPC_SIMULATOR)
add_definitions(-DLV_CONF_INCLUDE_SIMPLE)

# 收集 LVGL 源文件
file(GLOB_RECURSE LVGL_SOURCES "${LVGL_DIR}/src/*.c")

# GUI 源文件
set(GUI_SOURCES
    ${USER_DIR}/app/src/gui/gui_core.c
    ${USER_DIR}/app/src/gui/gui_calib.c
    ${USER_DIR}/app/src/gui/gui_widgets.c
    ${USER_DIR}/app/src/gui/screen_channel_setup.c
    ${USER_DIR}/app/src/gui/screen_acq_settings.c
    ${USER_DIR}/app/src/gui/screen_main_display.c
    ${USER_DIR}/app/src/gui/screen_alarm_records.c
    ${USER_DIR}/app/src/gui/screen_touch_test.c
)

# 共享数据与采集模块
set(SHARED_SOURCES
    ${USER_DIR}/app/src/channel_data.c
    ${USER_DIR}/app/src/touch_calib.c
    ${USER_DIR}/app/src/signal_algo.c
    ${USER_DIR}/app/src/acq_pipeline.c
    ${USER_DIR}/app/src/acq_task.c
    ${USER_DIR}/app/src/alarm_manager.c
)

# 桩模块源文件
set(STUB_SOURCES
    ${USER_DIR}/app/src/stubs/stub_config_store.c
    ${USER_DIR}/app/src/stubs/stub_data_logger.c
)

# 主程序
add_executable(pc_simulator
    main.c
    ${LVGL_SOURCES}
    ${GUI_SOURCES}
    ${SHARED_SOURCES}
    ${STUB_SOURCES}
)

# 链接 SDL2（注意顺序：mingw32 必须在 SDL2main 之前）
target_link_libraries(pc_simulator mingw32 SDL2main SDL2)

# 使用控制台程序入口（避免 WinMain 问题）
set_target_properties(pc_simulator PROPERTIES LINK_FLAGS "-mconsole")
