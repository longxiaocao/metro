# Phase 6.1: 单元测试 CMake 入口
# 位置：项目根目录 tests/CMakeLists.txt
# 由 顶层 CMakeLists.txt 通过 `add_subdirectory(tests)` 引入
#
# 设计：
#   - 零外部依赖（不引 gtest，Phase 6.1 决策）
#   - 全部测试 .cpp 链接到 ddfixtests 单二进制
#   - 注册到 CTest：ctest 即可跑
#
# 注意：
#   - Wrapper.h 需要 Windows COM 头（ddraw.h / objbase.h），所以测试要 WIN32
#   - ddfix 私有头不直接被测试 include（避免拖入整个 COM/D3D9 依赖），
#     test_wrapper.cpp 用 mock 化复现 AddressLookupTable 行为 + 引用计数
#   - test_ini_parser.cpp 通过相对路径 include 真实 IniParser.h（仅 STL）

# 收集所有 test_*.cpp
set(TEST_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_ini_parser.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_wrapper.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_perf_counter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_hlsl.cpp
)

# 公共 include 路径：项目根（让 #include "../ddfix/..." 能解析）
include_directories(${CMAKE_SOURCE_DIR})

# Win32 平台宏（让 Wrapper.h 内部 COM 类型能找到）
if (WIN32)
	add_definitions(-DWIN32 -D_WIN32 -D_CRT_SECURE_NO_WARNINGS)
endif()

# 单二进制：ddfixtests
add_executable(ddfixtests ${TEST_SOURCES})
target_include_directories(ddfixtests PRIVATE
	${CMAKE_SOURCE_DIR}                    # 项目根
	${CMAKE_CURRENT_SOURCE_DIR}            # tests/
	${CMAKE_SOURCE_DIR}/ddfix              # 真实头路径
)

# 把项目根路径注入编译期，让 test_*.cpp 知道从哪里找资源文件
target_compile_definitions(ddfixtests PRIVATE
	MBE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
	DDFIX_SOURCE_DIR="${CMAKE_SOURCE_DIR}/ddfix"
)

if (WIN32)
	# Wrapper.h / PerfCounter.h 需要 Windows COM 基础
	target_link_libraries(ddfixtests PRIVATE
		# Phase 8.15: link ddfix-static 拿到 IniParser/PerfCounter 等纯 STL 实现
		#   之前 link 列表为空，ddfixtests 报 LNK2019 unresolved NDDFIX::Config::IniParser::*
		#   ddfix-static 由 ddfix/CMakeLists.txt 无条件构建（Phase 8.15 起不依赖 BUILD_DDFIX_LIB）
		ddfix-static
		# 显式拉 COM 启动/卸载（initguid 用）
		# 不需要 d3d9.lib，测试不构造 D3D9 设备
	)
endif()

# 让 ctest 能发现
enable_testing()
add_test(
	NAME ddfixtests
	COMMAND ddfixtests
	# 在 tests/ 目录跑（这样 test_hlsl.cpp 的 "../ddfix/draw/ColorKey.hlsl" 能解析）
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# 编译警告：与主项目一致（/W4 + /permissive- 太严，单元测试用 /W3 + 无 warning-as-error）
if (MSVC)
	target_compile_options(ddfixtests PRIVATE /W3 /EHsc)
else()
	target_compile_options(ddfixtests PRIVATE -Wall -Wextra)
endif()

message(STATUS "Unit tests enabled (SingleTest). Sources: ${TEST_SOURCES}")
