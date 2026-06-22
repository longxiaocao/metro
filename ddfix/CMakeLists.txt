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

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ColorKeyHLSLC.h
    COMMAND ${FXC_EXECUTABLE} /Eps_main /Vng_colorKeyHLSLC /Tps_2_b /Fh${CMAKE_CURRENT_BINARY_DIR}/ColorKeyHLSLC.h /nologo ddraw/ColorKey.hlsl
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ddraw/ColorKey.hlsl
    COMMENT "Compiling ColorKey.hlsl via fxc.exe")

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
link_directories(${EXTRADXSDK_LIB_DIR})

# 主目标：ddfix 共享库（始终构建，保持向后兼容）
add_library(ddfix SHARED ${SOURCE_FILES} ${SRC_DINPUT} ${SRC_DDRAW} ${SRC_DSHOW} ${SRC_COMMON} ${SRC_CONFIG} ${SRC_DEBUG} ${CMAKE_CURRENT_BINARY_DIR}/ColorKeyHLSLC.h)
# Phase 7: 增加 dxguid.lib（IID 宏定义需要）+ winmm.lib（DInput 8 升级需要）
target_link_libraries(ddfix minhook dinput8.lib Strmiids.lib d3d9.lib d3dx9.lib dxguid.lib winmm.lib legacy_stdio_definitions.lib "-SAFESEH:NO")

# 可选：ddfix 静态库（Phase 5.3，BUILD_DDFIX_LIB=ON 时额外产出 ddfix-static.lib）
if (BUILD_DDFIX_LIB)
    add_library(ddfix-static STATIC ${SOURCE_FILES} ${SRC_DINPUT} ${SRC_DDRAW} ${SRC_DSHOW} ${SRC_COMMON} ${SRC_CONFIG} ${SRC_DEBUG} ${CMAKE_CURRENT_BINARY_DIR}/ColorKeyHLSLC.h)
    target_link_libraries(ddfix-static minhook dinput8.lib Strmiids.lib d3d9.lib d3dx9.lib dxguid.lib winmm.lib legacy_stdio_definitions.lib)
    set_target_properties(ddfix-static PROPERTIES OUTPUT_NAME "ddfix-static")
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


