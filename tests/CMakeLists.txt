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
	# Phase 9.1.4: Pillarbox 居中 + D3D9Context 单元测试
	# 链接 ddfix-static (Phase 9.1.4 已在 ddfix/CMakeLists.txt 把 D3D9Context.cpp
	#   纳入 ddfix-static, 让 test 可调 SetRequestedMode / HasRequestedMode /
	#   CalcBackBufferSizeForTest 等 API + friend 访问私有字段).
	${CMAKE_CURRENT_SOURCE_DIR}/test_d3d9_context.cpp
	# Phase 9.1.5: IDirectDraw::SetDisplayMode 契约测试
	# 通过 D3D9Context 层验证"SetDisplayMode 等价于 SetRequestedMode"语义,
	#   避免引入 ddraw proxy 全局状态到 ddfixtests (CI #49 历史教训).
	${CMAKE_CURRENT_SOURCE_DIR}/test_idirectdraw.cpp
	# Phase 9.2: SMAA 抗锯齿后处理单元测试
	# 覆盖 PostProcess::Mode 枚举解析 + ConfigManager [SMAA] section
	#   + shader 源文件存在性. 不测 Initialize/Run（需要真 D3D9 设备）.
	${CMAKE_CURRENT_SOURCE_DIR}/test_smaa.cpp
	# Phase 9.3.1-9.3.7, 9.3.13: ExecuteBuffer 解析器单元测试
	# 覆盖 ExecuteBufferParser 11 个场景: 边界条件 + 5 个 D3DOP_* 解析 +
	#   EXIT 终止 + 未知 opcode 跳过 + ParseStats 计数 + 混合流.
	# 链接 ddfix-static 拿到 NDDFIX::Render::ExecuteBufferParser + ExtractedGeometry.
	${CMAKE_CURRENT_SOURCE_DIR}/test_execute_buffer.cpp
	# Phase 9.3.8-9.3.11: GBuffer MRT 单元测试
	# 覆盖 GBuffer.hlsl 源文件存在性 + 4 MRT 输出 + 公开文献引用 +
	#   GBufferRenderer 单例身份 + D3D9Context 薄包装 API (无设备时不崩).
	# 不测真实 RT 创建 (需要 D3D9 设备, 集成测试范围).
	${CMAKE_CURRENT_SOURCE_DIR}/test_gbuffer.cpp
	# Phase 9.3.10: Deferred Lighting 单元测试
	# 覆盖 Deferred.hlsl 源文件 + ps_main + 4 GBuffer 纹理采样 +
	#   Blinn-Phong 光照计算 + GPU Gems 2 §9.5 公开文献引用.
	${CMAKE_CURRENT_SOURCE_DIR}/test_deferred_lighting.cpp
	# Phase 9.4: Cascaded Shadow Map + PCF 单元测试
	# 覆盖 Shadow.hlsl + PCF 函数 (3x3/5x5/7x7) + ShadowConfig 解析 +
	#   ComputeCascadeSplits 静态函数 + GPU Gems 3 §8 公开文献引用.
	# 链接 ddfix-static 拿到 NDDFIX::Render::ShadowRenderer + ConfigManager.
	${CMAKE_CURRENT_SOURCE_DIR}/test_shadow.cpp
	# Phase 9.7: ScriptApi 单元测试
	# 覆盖 NDDFIX::Script::ScriptApi 单例 + 16 个 setter/getter round-trip +
	#   16 个 C-linkage __stdcall Ddfix_* 函数存在性 (链接器 unresolved-external
	#   验证) + RegisterAll/UnregisterAll 幂等性 + 多线程并发读写.
	# 链接 ddfix-static 拿到 ScriptApi 单例 + 16 个 Ddfix_* 导出.
	${CMAKE_CURRENT_SOURCE_DIR}/test_script_api.cpp
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
	# Phase 9.1.4: D3D9Context.h #include <d3dx9.h>, 测试编译需要 d3dx9 头路径.
	#   顶层 CMakeLists.txt 已 set EXTRADXSDK_INCLUDE_DIR, 这里补 include 进来.
	${EXTRADXSDK_INCLUDE_DIR}
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
		# Phase 8.25.16: 显式传递 d3dx9.lib + link_directories 兜底 (CI #43 报告
		#   LNK1181 cannot open input file 'd3dx9.lib' [ddfixtests.vcxproj]).
		#   根因: tests 子目录作为 ddfix 的兄弟, 不继承 ddfix/CMakeLists.txt 中的 link_directories.
		#     顶层 CMakeLists.txt 提到 link_directories 后, ddfixtests 仍可能在某些 VS generator
		#     配置下不传递 d3dx9.lib 路径. 显式 link 解决.
		#   注: D3DX9_LIB 顶层 set 为空时 (d3dx9.lib 不存在), 这里 link 空字符串无副作用.
		${D3DX9_LIB}
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
