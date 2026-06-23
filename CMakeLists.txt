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
# Phase 8.25.16: 把 link_directories 移到顶层, 让 ddfix + tests 都能继承 (CI #43 报告
#   LNK1181 cannot open input file 'd3dx9.lib' [D:\a\metro\metro\build\tests\ddfixtests.vcxproj]).
#   根因: link_directories() 是 CMake 目录级全局, 只在设置它的 CMakeLists.txt 文件作用域
#     (含 add_subdirectory 引入的子目录) 内有效。ddfix 子目录设置后, tests 子目录作为 sibling
#     目录无法继承, 导致 ddfixtests 链接时找不到 d3dx9.lib (虽然 ddfix-static 内部已经 link)。
#   修复: 提到顶层, ddfix 和 tests 都能继承。
#   注: ddfix/CMakeLists.txt 仍有 link_directories 兜底, 保证直接构建 ddfix 子目录时也能用。
link_directories(${EXTRADXSDK_LIB_DIR})
# Phase 8.25.16: d3dx9.lib 守护, 不存在时 CMake 全局变量 D3DX9_LIB 为空, 避免 ddfix + tests
#   双 LNK1181.
if (EXISTS "${EXTRADXSDK_LIB_DIR}/d3dx9.lib")
    message(STATUS "d3dx9.lib found at ${EXTRADXSDK_LIB_DIR}/d3dx9.lib (Phase 8.25.16 global)")
    set(D3DX9_LIB "d3dx9.lib")
else()
    message(WARNING "d3dx9.lib NOT found at ${EXTRADXSDK_LIB_DIR}; D3DX functions will fail to link")
    set(D3DX9_LIB "")
endif()

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

