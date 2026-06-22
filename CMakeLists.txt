cmake_minimum_required(VERSION 3.11)

project(MeteorBladeEnhancer)

# -------------------- Phase 5.3: Build 选项 --------------------
# 默认配置类型：Debug + Release（之前是 Debug + RelWithDebInfo，没有纯 Release）
set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
# 提示：上面只影响 VS multi-config generator；单 config generator 仍按
#       CMAKE_BUILD_TYPE 选择，需要的话可在命令行 -DCMAKE_BUILD_TYPE=Release 切换。
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# -------------------- 业务参数（保持原 cache 变量名，向后兼容） --------------------
set(GameExe "" CACHE FILEPATH "游戏Exe路径")
set(GameStartupArgs "w" CACHE STRING "游戏启动参数")

# -------------------- 新增编译期选项（Phase 5.3） --------------------
# 单元测试开关：默认 ON（Phase 6 会实装 gtest + tests/CMakeLists.txt）
option(BUILD_TESTS "Build unit tests" ON)
# ddfix 静态库开关：默认 OFF（默认仍输出 ddfix.dll 共享库）
option(BUILD_DDFIX_LIB "Build ddfix as static lib" OFF)

add_definitions("/MP")

# -------------------- 输出目录 --------------------
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# -------------------- 第三方 SDK 路径 --------------------
set (EXTRADXSDK_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ExtraDxSDK/Include)
set (EXTRADXSDK_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ExtraDxSDK/Lib)

# Phase 8.9: 加 GameExe 空值检查
#   原 line 33 直接调用 get_filename_component(GameDir ${GameExe} DIRECTORY)
#   当 GameExe 为空字符串时（build.yml 默认 GAME_EXE: ""），cmake 4.x 报
#   "get_filename_component called with incorrect number of arguments"
#   这是因为 cmake 把空字符串当作未传参处理。
#   修复：if (GameExe) 守护，空时把 GameDir 也设为空字符串。
if (GameExe)
    get_filename_component(GameDir ${GameExe} DIRECTORY)
else()
    set(GameDir "")
endif()

# -------------------- Phase 5.3: 目标平台检测 --------------------
# CMake 不直接暴露 "x86/x64" 字符串，但 CMAKE_SIZEOF_VOID_P 可以区分 4/8 字节。
# 在 status 消息里打印一次，方便排错。
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(MBE_TARGET_ARCH "x64")
elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(MBE_TARGET_ARCH "x86")
else()
    set(MBE_TARGET_ARCH "unknown")
endif()
message(STATUS "MeteorBladeEnhancer target architecture: ${MBE_TARGET_ARCH} (sizeof(void*) = ${CMAKE_SIZEOF_VOID_P})")

include_directories(.)

add_subdirectory(ddfix)
add_subdirectory(minhook)

# -------------------- Phase 6.1: 单元测试 --------------------
# tests/ 在项目根目录。EXISTS 兜底防止 Phase 6 之前目录不存在时失败。
if (BUILD_TESTS)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    else()
        message(STATUS "BUILD_TESTS=ON but tests/CMakeLists.txt not found yet (expected in Phase 6.1); skipping tests subdir")
    endif()
endif()

set_property(TARGET ddfix PROPERTY VS_GLOBAL_GameExe "${GameExe}")
set_property(TARGET ddfix PROPERTY VS_GLOBAL_GameDir "${GameDir}")
set_property(TARGET ddfix PROPERTY VS_GLOBAL_GameStartupArgs "${GameStartupArgs}")

