set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")

SET(CMAKE_INCLUDE_CURRENT_DIR ON)

# Phase 8: fxc.exe 路径探测（多路径 + 版本子目录兜底）
# 优先级：
#   1) 环境变量 %WindowsSdkVerBinPath% / $ENV{WindowsSdkDir}（MSVC Dev Cmd 已设置）
#   2) PATHS_SUFFIXES 列出所有可能的 SDK 版本子目录
#   3) PATH（最后兜底）
find_program(FXC_EXECUTABLE
    NAMES fxc.exe fxc
    HINTS
        "$ENV{WindowsSdkDir}/bin"
        "$ENV{WindowsSdkVerBinPath}"
    PATHS
        "C:/Program Files (x86)/Windows Kits/10/bin"
        "C:/Program Files/Windows Kits/10/bin"
    PATH_SUFFIXES
        "10.0.26100.0/x64" "10.0.26100.0"
        "10.0.22621.0/x64" "10.0.22621.0"
        "10.0.22000.0/x64" "10.0.22000.0"
        "10.0.20348.0/x64" "10.0.20348.0"
        "10.0.19041.0/x64" "10.0.19041.0"
        "10.0.18362.0/x64" "10.0.18362.0"
        "10.0.17763.0/x64" "10.0.17763.0"
    DOC "HLSL shader compiler (fxc.exe)"
)
if (NOT FXC_EXECUTABLE)
    message(FATAL_ERROR
        "fxc.exe not found.\n"
        "  Tried: PATH, WindowsSdkDir env, WindowsSdkVerBinPath env,\n"
        "         C:/Program Files (x86)/Windows Kits/10/bin/<version>/x64,\n"
        "         C:/Program Files/Windows Kits/10/bin/<version>/x64\n"
        "  Fix:   install Windows 10/11 SDK (any 19041+), or put fxc.exe in PATH.")
endif()
message(STATUS "Found fxc.exe: ${FXC_EXECUTABLE}")

# Phase 9.4 build fix: MSBuild x86 host process 调 x64 fxc.exe 报 268435659
#   (STATUS_DLL_NOT_FOUND). 解决: 通过 PowerShell 包装脚本转发 fxc.exe 调用.
#   fxc_wrapper.ps1 优先用最新 SDK 10.0.26100.0, 回退到 10.0.19041.0.
#   包装脚本的 exit code 原样透传给 MSBuild, 不影响 fxc 错误诊断.
#   PowerShell.exe 进程环境与 MSBuild 进程隔离, DLL 加载问题消失.
set(FXC_WRAPPER "${CMAKE_CURRENT_SOURCE_DIR}/fxc_wrapper.ps1")
if (NOT EXISTS "${FXC_WRAPPER}")
    message(FATAL_ERROR "fxc_wrapper.ps1 not found at ${FXC_WRAPPER}")
endif()

add_custom_command(OUTPUT ColorKeyHLSLC.h
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${FXC_WRAPPER}" /Eps_main /Vng_colorKeyHLSLC /Tps_2_b /Fh${CMAKE_CURRENT_BINARY_DIR}/ColorKeyHLSLC.h /nologo ddraw/ColorKey.hlsl
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ddraw/ColorKey.hlsl
    COMMENT "Compiling ColorKey.hlsl via fxc.exe (PowerShell wrapper)")
# Phase 8.25.15: 用 add_custom_target 包装 fxc 命令, 让 ddfix / ddfix-static 通过
#   add_dependencies 显式声明依赖, 避免 VS multi-config generator 在多 target 并行构建时
#   ColorKeyHLSLC.h 还没生成就开始编译 cpp。
add_custom_target(CompileColorKeyHLSLC DEPENDS ColorKeyHLSLC.h)

# Phase 9.3.8: GBuffer VS 编译 (vs_3_0)
#   Entry: vs_main
#   Source: shaders/GBuffer.hlsl (第 1 个 vs_main 函数, fxc 取第 1 个 VS 函数)
#   Output: GBufferVSHLSLC.h (C 数组)
#   公开文献: GPU Gems 2 第 9 章 "Deferred Shading in S.T.A.L.K.E.R." (Clean Room)
add_custom_command(OUTPUT GBufferVSHLSLC.h
    COMMAND ${FXC_EXECUTABLE} /Evs_main /Vng_gBufferVSHLSLC /Tvs_3_0 /Fh${CMAKE_CURRENT_BINARY_DIR}/GBufferVSHLSLC.h /nologo shaders/GBuffer.hlsl
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS shaders/GBuffer.hlsl
    COMMENT "Compiling GBuffer.hlsl (VS) via fxc.exe")
add_custom_target(CompileGBufferVSHLSLC DEPENDS GBufferVSHLSLC.h)

# Phase 9.3.8: GBuffer PS 编译 (ps_3_0, MRT 4 RT)
#   Entry: ps_main
#   Source: shaders/GBuffer.hlsl (第 2 个 ps_main 函数)
#   Output: GBufferPSHLSLC.h
#   公开文献: GPU Gems 2 第 9 章 (Clean Room)
add_custom_command(OUTPUT GBufferPSHLSLC.h
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${FXC_WRAPPER}" /Eps_main /Vng_gBufferPSHLSLC /Tps_3_0 /Fh${CMAKE_CURRENT_BINARY_DIR}/GBufferPSHLSLC.h /nologo shaders/GBuffer.hlsl
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS shaders/GBuffer.hlsl
    COMMENT "Compiling GBuffer.hlsl (PS) via fxc.exe (PowerShell wrapper)")
add_custom_target(CompileGBufferPSHLSLC DEPENDS GBufferPSHLSLC.h)

# Phase 9.3.10: Deferred PS 编译 (ps_3_0, 单 RT 输出 back buffer)
#   Entry: ps_main
#   Source: shaders/Deferred.hlsl
#   Output: DeferredPSHLSLC.h
#   公开文献: GPU Gems 2 第 9.5 节 (Clean Room)
add_custom_command(OUTPUT DeferredPSHLSLC.h
    COMMAND ${FXC_EXECUTABLE} /Eps_main /Vng_deferredPSHLSLC /Tps_3_0 /Fh${CMAKE_CURRENT_BINARY_DIR}/DeferredPSHLSLC.h /nologo shaders/Deferred.hlsl
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS shaders/Deferred.hlsl
    COMMENT "Compiling Deferred.hlsl (PS) via fxc.exe")
add_custom_target(CompileDeferredPSHLSLC DEPENDS DeferredPSHLSLC.h)

# Phase 9.4: Shadow VS 编译 (vs_3_0, depth-only pass)
#   Entry: vs_main
#   Source: shaders/Shadow.hlsl (第 1 个 vs_main 函数)
#   Output: ShadowVSHLSLC.h
#   公开文献: GPU Gems 3 第 8 章 "Parallel-Split Shadow Maps" (Clean Room)
add_custom_command(OUTPUT ShadowVSHLSLC.h
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${FXC_WRAPPER}" /Evs_main /Vng_shadowVSHLSLC /Tvs_3_0 /Fh${CMAKE_CURRENT_BINARY_DIR}/ShadowVSHLSLC.h /nologo shaders/Shadow.hlsl
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS shaders/Shadow.hlsl
    COMMENT "Compiling Shadow.hlsl (VS) via fxc.exe (PowerShell wrapper)")
add_custom_target(CompileShadowVSHLSLC DEPENDS ShadowVSHLSLC.h)

# Phase 9.4: Shadow PS 编译 (ps_3_0, depth-only pass 占位)
#   Entry: ps_main
#   Source: shaders/Shadow.hlsl (第 2 个 ps_main 函数)
#   Output: ShadowPSHLSLC.h
#   公开文献: GPU Gems 3 第 8 章 (Clean Room)
add_custom_command(OUTPUT ShadowPSHLSLC.h
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${FXC_WRAPPER}" /Eps_main /Vng_shadowPSHLSLC /Tps_3_0 /Fh${CMAKE_CURRENT_BINARY_DIR}/ShadowPSHLSLC.h /nologo shaders/Shadow.hlsl
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS shaders/Shadow.hlsl
    COMMENT "Compiling Shadow.hlsl (PS) via fxc.exe (PowerShell wrapper)")
add_custom_target(CompileShadowPSHLSLC DEPENDS ShadowPSHLSLC.h)

FILE(GLOB SRC_DINPUT dinput/*.h dinput/*.cpp)
FILE(GLOB SRC_DDRAW ddraw/*.h ddraw/*.cpp)
FILE(GLOB SRC_DSHOW dshow/*.h dshow/*.cpp)
FILE(GLOB SRC_COMMON Common/*.h Common/*.cpp)
FILE(GLOB SRC_CONFIG Config/*.h Config/*.cpp)
FILE(GLOB SRC_DEBUG Debug/*.h Debug/*.cpp)

# ----------------------------------------------------------------------
# Phase 5.5: 20+ 壳包装类标注（仅注释，不删类，避免破坏 QueryInterface）
# ----------------------------------------------------------------------
# TODO(Phase 5.5): 删壳需要先验证 m_IDirectDraw4::QueryInterface 能处理所有 IID
#   当前 ddraw/ 目录下约 20 个壳包装（IDirectDraw2/3/7、IDirectDrawSurface2/3/7、
#   IDirect3D/2/7、IDirect3DDevice/2/7、IDirect3DVertexBuffer、IDirect3DExecuteBuffer、
#   IDirect3DMaterial/2/3 之外的 stub、IDirect3DViewport/2 等）都还保留，因为
#   它们的 QueryInterface 仍然被 AddressLookupTable 调用，删除前必须做
#   端到端 IID 映射表回归测试。
#   详见 PROJECT_ANALYSIS.md §21.5（本 Phase 5 的评估段）。
# ----------------------------------------------------------------------

FILE(GLOB SOURCE_FILES *.h *.cpp *.def)
include_directories(${EXTRADXSDK_INCLUDE_DIR})
# Phase 8.25.16: link_directories 已在顶层 CMakeLists.txt 设置, 这里注释掉避免冗余.
#   见顶层 CMakeLists.txt Phase 8.25.16 注释. 此处保留 link_directories 兜底, 防止有人
#   只构建 ddfix 子目录时找不到 lib.
# link_directories(${EXTRADXSDK_LIB_DIR})
# Phase 8.25.16: D3DX9_LIB 全局变量由顶层 CMakeLists.txt 设置 (Phase 8.25.16), 不再
#   重复 set. 这里直接用.
message(STATUS "ddfix target using D3DX9_LIB: '${D3DX9_LIB}' (set by top-level CMakeLists.txt)")

# 主目标：ddfix 共享库（始终构建，保持向后兼容）
add_library(ddfix SHARED ${SOURCE_FILES} ${SRC_DINPUT} ${SRC_DDRAW} ${SRC_DSHOW} ${SRC_COMMON} ${SRC_CONFIG} ${SRC_DEBUG}
    ${CMAKE_CURRENT_BINARY_DIR}/ColorKeyHLSLC.h
    ${CMAKE_CURRENT_BINARY_DIR}/GBufferVSHLSLC.h
    ${CMAKE_CURRENT_BINARY_DIR}/GBufferPSHLSLC.h
    ${CMAKE_CURRENT_BINARY_DIR}/DeferredPSHLSLC.h
    ${CMAKE_CURRENT_BINARY_DIR}/ShadowVSHLSLC.h
    ${CMAKE_CURRENT_BINARY_DIR}/ShadowPSHLSLC.h)
# Phase 7: 增加 dxguid.lib（IID 宏定义需要）+ winmm.lib（DInput 8 升级需要）
target_link_libraries(ddfix minhook dinput8.lib Strmiids.lib d3d9.lib ${D3DX9_LIB} dxguid.lib winmm.lib legacy_stdio_definitions.lib "-SAFESEH:NO")
# Phase 8.25.15: 显式声明 ddfix 依赖 CompileColorKeyHLSLC, 保证 fxc 先跑
# Phase 9.3.8-9.3.10: 新增 GBuffer VS/PS + Deferred PS shader 依赖
# Phase 9.4: 新增 Shadow VS/PS shader 依赖
add_dependencies(ddfix CompileColorKeyHLSLC CompileGBufferVSHLSLC CompileGBufferPSHLSLC CompileDeferredPSHLSLC CompileShadowVSHLSLC CompileShadowPSHLSLC)

# 静态库：总是构建（tests/CMakeLists.txt 链接 ddfix-static 拿 IniParser/PerfCounter 等
#   纯 STL 实现，避免 ddfixtests 通过 ddraw/com 调用 ddfix.dll 的全部依赖）
#   Phase 8.15: 之前用 `if (BUILD_DDFIX_LIB)` 包了，BUILD_DDFIX_LIB=OFF 时 tests link 失败
#   （LNK2019 unresolved external NDDFIX::Config::IniParser::*）。改为无条件构建。
#   Phase 8.25.15: 改用 ${CMAKE_CURRENT_BINARY_DIR}/ColorKeyHLSLC.h 全路径, 与 ddfix 一致,
#     确保 ddfix-static 也触发 fxc 生成 ColorKeyHLSLC.h, 避免 ddfix 阶段找不到。
#   Phase 8.25.22: 限制 ddfix-static 源到 Config + Debug + Common 目录, 排除
#     ddraw/dinput/dshow 全部 cpp. 原因: ddfixtests 不需要 ddraw proxy 包装, 而
#     ddraw/*.cpp 引入了全局 ProxyAddressLookupTable + ddraw.h 头链, 在 .exe
#     启动时构造触发 STATUS_HEAP_CORRUPTION (CI #49 exit code -1073740940).
#     只保留 ddfix-static 纯 STL 部分 (IniParser / PerfCounter / HudRenderer /
#     ImGuiBackend) + Common (Logging / SmartPointer / Wrapper), 足够 test 链接.
set(STATIC_SRC_DRAW_SRC ${SRC_CONFIG} ${SRC_DEBUG} ${SRC_COMMON})
# ddraw/ 头文件仍然需要 include (Logging.h 等可能引用), 但只编 Config/Debug/Common 的 cpp.
# 注意: ddraw 头依赖 d3d9types.h 等, 但我们不编译 ddraw cpp, 所以不会触发 dx6:: 冲突.
# Phase 8.25.23: ddfix-static 不再依赖 ColorKeyHLSLC.h. 该头只被 D3D9Context.cpp +
#   IDirectDrawSurface4.cpp 引用, 都不在 ddfix-static 源中. 移除依赖避免 ddfix-static
#   触发 fxc 生成 (ddfix-static 不需要).
# Phase 9.1.4: 把 D3D9Context.cpp 纳入 ddfix-static, 以便 test_d3d9_context.cpp
#   通过链接 ddfix-static 拿到 D3D9Context 类符号 (Phase 9.1.4 单元测试需要调
#   SetRequestedMode / HasRequestedMode / GetRequestedMode / CalcBackBufferSizeForTest
#   等公开 API). D3D9Context.cpp 依赖 ColorKeyHLSLC.h, 所以 ddfix-static 现在需要
#   触发 fxc 生成, 改用 add_dependencies 显式声明.
# Phase 9.2: 把 PostProcess.cpp 纳入 ddfix-static, 让 test_smaa.cpp 能
#   链接 ddfix-static 拿到 NDDFIX::PostProcess::PostProcess 类 + Mode 解析
#   函数. PostProcess.cpp 依赖 D3DX9 + d3d9 (ColorKeyHLSLC.h 之外不依赖
#   ColorKey 头), 与 ddfix-static 现有 d3d9 + d3dx9 链接一致.
# Phase 9.3.1-9.3.7: 把 ExecuteBufferParser.cpp 纳入 ddfix-static, 让
#   test_execute_buffer.cpp 能直接 link 到 NDDFIX::Render::ExecuteBufferParser
#   与 ExtractedGeometry. ExecuteBufferParser 故意不 include ddraw.h (用
#   LocalD3D* 镜像类型), 纯 STL, 不触发 ddraw/ 全局污染. 是 ddfix-static
#   安全的纯逻辑模块.
# Phase 9.3.8-9.3.11: 把 GBufferRenderer.cpp 纳入 ddfix-static, 让
#   test_gbuffer.cpp / test_deferred_lighting.cpp 能 link 到 GBufferRenderer
#   单例. GBufferRenderer.cpp include "../D3D9Context.h" + fxc 生成的 GBuffer
#   shader .h, 与 ddfix-static 现有依赖 (d3d9 + d3dx9) 一致. 注意: GBufferRenderer
#   内部的 RT/shader 创建需要真 D3D9 设备, 单元测试只验证单例身份 + 配置 API
#   (不实际创 RT), 真实初始化由游戏运行时调 D3D9Context::Initialize 触发.
# Phase 9.7: 把 ScriptApi.cpp 纳入 ddfix-static, 让 test_script_api.cpp 能
#   link 到 NDDFIX::Script::ScriptApi 单例 + 16 个 Ddfix_* 导出函数 (后者靠
#   链接器 unresolved-external 验证存在性). ScriptApi.cpp 纯 STL (mutex / string),
#   不依赖 d3d9 / d3dx9 / fxc 头, 与 ddfix-static 其他源完全解耦.
set(STATIC_SRC_DRAW_SRC ${STATIC_SRC_DRAW_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/D3D9Context.cpp ${CMAKE_CURRENT_SOURCE_DIR}/ddraw/PostProcess.cpp ${CMAKE_CURRENT_SOURCE_DIR}/ddraw/ExecuteBufferParser.cpp ${CMAKE_CURRENT_SOURCE_DIR}/ddraw/GBufferRenderer.cpp ${CMAKE_CURRENT_SOURCE_DIR}/ddraw/ShadowRenderer.cpp ${CMAKE_CURRENT_SOURCE_DIR}/ddraw/ScriptApi.cpp)
add_library(ddfix-static STATIC ${STATIC_SRC_DRAW_SRC})
target_link_libraries(ddfix-static minhook d3d9.lib ${D3DX9_LIB} dxguid.lib winmm.lib legacy_stdio_definitions.lib)
# Phase 9.1.4: ddfix-static 现在编译 D3D9Context.cpp, 后者 #include "ColorKeyHLSLC.h",
#   因此需要先跑 fxc 生成该头. 显式 add_dependencies 触发 CompileColorKeyHLSLC target.
# Phase 9.3.8-9.3.10: ddfix-static 现在也编译 GBufferRenderer.cpp, 后者
#   #include GBufferVSHLSLC.h / GBufferPSHLSLC.h / DeferredPSHLSLC.h, 因此
#   也需要触发这 3 个 fxc compile target. 否则 ddfix-static 阶段编译
#   GBufferRenderer.cpp 时 .h 还没生成 → fatal error.
# Phase 9.4: ddfix-static 现在也编译 ShadowRenderer.cpp, 后者 #include
#   ShadowVSHLSLC.h / ShadowPSHLSLC.h, 因此需要触发这 2 个 fxc compile target.
#   否则 ddfix-static 阶段编译 ShadowRenderer.cpp 时 .h 还没生成 → fatal error.
add_dependencies(ddfix-static CompileColorKeyHLSLC CompileGBufferVSHLSLC CompileGBufferPSHLSLC CompileDeferredPSHLSLC CompileShadowVSHLSLC CompileShadowPSHLSLC)
set_target_properties(ddfix-static PROPERTIES OUTPUT_NAME "ddfix-static")

# 向后兼容：BUILD_DDFIX_LIB 选项保留但变为 no-op（永远 ON）
#   Phase 8.15: 保留宏定义以防外部脚本依赖，但不再控制构建
if (BUILD_DDFIX_LIB)
    message(STATUS "BUILD_DDFIX_LIB: 始终 ON（Phase 8.15 起 ddfix-static 总是构建）")
endif()

# 可选：单元测试（Phase 6.1+）
# 注：tests/ 目录在项目根，与 ddfix 平级，由顶层 CMakeLists.txt 的
#     `if (BUILD_TESTS) add_subdirectory(tests)` 引入。
#     这里不再需要单独处理 —— ddfix 只负责核心 ddfix.dll 构建。

source_group("dinput" FILES ${SRC_DINPUT})
source_group("ddraw" FILES ${SRC_DDRAW})
source_group("dshow" FILES ${SRC_DSHOW})
source_group("Common" FILES ${SRC_COMMON})
source_group("config" FILES ${SRC_CONFIG})
source_group("debug" FILES ${SRC_DEBUG})


configure_file(vcxproj.user.in ddfix.vcxproj.user @ONLY)


