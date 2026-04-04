cmake_minimum_required(VERSION 3.22)

# 1. 项目基础设置
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# 定义构建类型 (Debug/Release)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release") # 改为 Release 模式，自动开启 -O3 优化
endif()

add_compile_options(-O2)

set(CMAKE_PROJECT_NAME CMake_LCD_Driver0_1)
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# 启用 C 和 汇编语言支持
enable_language(C ASM)

# 启用编译命令导出 (方便 clangd 等插件索引)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# ====================================================================
# 2. 源码搜寻与定义
# ====================================================================

# 自动搜寻 LVGL 内部的所有 .c 源代码
file(GLOB_RECURSE LVGL_SOURCES
        "User_Modules/lvgl/src/*.c"
)
# 自动搜寻 SquareLine 生成的 UI 源代码 【新增这一段】
file(GLOB_RECURSE UI_SOURCES
        "User_Modules/GUI_App/*.c"
)
# 明确定义你的业务逻辑源码
# 建议包含 Core/Src 下的所有核心文件，确保链接完整性
set(USER_SOURCES
        Core/Src/main.c
        Core/Src/gpio.c
        Core/Src/dma.c
        Core/Src/i2c.c
        Core/Src/spi.c
        Core/Src/usart.c
        Core/Src/quadspi.c
        Core/Src/stm32h7xx_it.c
        Core/Src/stm32h7xx_hal_msp.c
        Core/Src/lcd.c
        Core/Src/ft6336u.c
        Core/Src/lv_port_disp.c
        Core/Src/lv_port_indev.c
        Core/Src/sysmem.c
        Core/Src/syscalls.c
        User_Modules/GUI_App/ui_events.c
)

# ====================================================================
# 3. 创建执行文件 (Target)
# ====================================================================

# 【关键修正】将 USER_SOURCES 和 LVGL_SOURCES 同时加入，解决 undefined reference 错误
add_executable(${CMAKE_PROJECT_NAME}
        ${USER_SOURCES}
        ${LVGL_SOURCES}
        ${UI_SOURCES}
)

# ====================================================================
# 4. 包含目录 (Include Paths)
# ====================================================================

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        Core/Inc
        User_Modules/lvgl
        User_Modules/lvgl/src
        User_Modules/GUI_App
)

# ====================================================================
# 5. STM32CubeMX 子项目集成
# ====================================================================

# 引入 CubeMX 生成的 CMake 脚本
add_subdirectory(cmake/stm32cubemx)

# 链接 CubeMX 生成的硬件库
target_link_libraries(${CMAKE_PROJECT_NAME}
        stm32cubemx
)

# ====================================================================
# 6. 其他配置
# ====================================================================

# 移除在使用 C++ 时可能出现的错误库依赖
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# (可选) 如果你需要打印内存占用情况，可以根据编译器添加链接参数
# target_link_options(${CMAKE_PROJECT_NAME} PRIVATE "-Wl,--print-memory-usage")