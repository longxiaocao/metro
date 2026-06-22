set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")

SET(CMAKE_INCLUDE_CURRENT_DIR ON)

add_custom_command(OUTPUT ColorKeyHLSLC COMMAND fxc.exe /Eps_main /Vng_colorKeyHLSLC /Tps_2_b /Fh${CMAKE_CURRENT_BINARY_DIR}\\ColorKeyHLSLC.h /nologo ddraw\\ColorKey.hlsl WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

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
add_library(ddfix SHARED ${SOURCE_FILES} ${SRC_DINPUT} ${SRC_DDRAW} ${SRC_DSHOW} ${SRC_COMMON} ${SRC_CONFIG} ${SRC_DEBUG} ColorKeyHLSLC)
target_link_libraries(ddfix minhook dinput8.lib Strmiids.lib d3d9.lib d3dx9.lib legacy_stdio_definitions.lib "-SAFESEH:NO")

# 可选：ddfix 静态库（Phase 5.3，BUILD_DDFIX_LIB=ON 时额外产出 ddfix-static.lib）
if (BUILD_DDFIX_LIB)
    add_library(ddfix-static STATIC ${SOURCE_FILES} ${SRC_DINPUT} ${SRC_DDRAW} ${SRC_DSHOW} ${SRC_COMMON} ${SRC_CONFIG} ${SRC_DEBUG} ColorKeyHLSLC)
    target_link_libraries(ddfix-static minhook dinput8.lib Strmiids.lib d3d9.lib d3dx9.lib legacy_stdio_definitions.lib)
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
source_group("Config" FILES ${SRC_CONFIG})
source_group("Debug" FILES ${SRC_DEBUG})


configure_file(vcxproj.user.in ddfix.vcxproj.user @ONLY)


