#pragma once

#define INITGUID

// Phase 8.25.5: 必须在 d3d9types.h 之前**全局**先 include windows.h。
//
// 根因 (CI #33 新错误):
//   wine_d3dtypes.h 内部 line 28 有 `#include <windows.h>`, 把 windows.h 的所有
//   typedef (LONG, DWORD, HWND, BOOL, BYTE, WORD, INT, UINT 等) 在 namespace dx6
//   块内被 include, 导致这些基础类型进入 namespace dx6 (而非全局)。
//   之后 d3d9types.h (在 ddraw.h line 40 已全局 include) 在 typedef 块
//   (line 76 D3DRECT 中 `LONG x1, y1, x2, y2;`) 引用 LONG, 编译器找不到
//   全局 LONG, 报 `x1: unknown override specifier` (C3646) 和
//   `D3DCOLOR: missing ';'` (C2146) 错误。
//
// 解决: 在 ddraw.h 最顶部 include <windows.h>, 让 wine_d3dtypes.h 内部的
//   `<windows.h>` 被头保护跳过, windows.h 的所有 typedef 始终在全局可见。
#include <windows.h>

// Phase 8.25: 用项目内 Wine d3d.h / d3dtypes.h / d3dcaps.h 替代 Windows SDK d3d.h。
//
// 原因 (Phase 8.22 ~ 8.24 失败根因):
//   1) Windows SDK 的 d3d.h 根据 DIRECT3D_VERSION 提供不同接口子集:
//      * 0x0600 (DX6) -> IDirect3D, IDirect3D2, IDirect3D3, IDirect3DMaterial*,
//        IDirect3DTexture*, IDirect3DDevice*, IDirect3DViewport*, IDirect3DLight,
//        IDirect3DExecuteBuffer 等
//      * 0x0700 (DX7) -> 在 DX6 基础上加 IDirect3D7, IDirect3DDevice7,
//        IDirect3DVertexBuffer, IDirect3DVertexBuffer7 等
//      ** 0x0600 不定义 IDirect3D7; 0x0700 不定义 IDirect3D **
//      ** 0x0600 与 0x0700 都会定义 IDirect3D 等共有接口, 重复 #include 触发 C2011 **
//      ** 0x0600 模式 d3d.h 内部还会引用 LPD3DENUMDEVICESCALLBACK7 / LPD3DVIEWPORT7 等
//         DX7 独有类型, 触发 C2061 **
//   2) Windows SDK 的 d3dtypes.h 定义了与 d3d9types.h 同名 enum (D3DBLEND, D3DFILLMODE
//      等),在同 TU 全局命名空间触发 C2011。
//
// 解决 (Phase 8.25):
//   用 Wine 的 d3d.h / d3dtypes.h / d3dcaps.h 替代 Windows SDK 的对应头文件:
//     a) Wine d3d.h 在同文件中**无条件**定义 DX5/DX6/DX7 所有接口
//        (IDirect3D, IDirect3D2, IDirect3D3, IDirect3D7, IDirect3DDevice{,2,3,7},
//        IDirect3DMaterial{,2,3}, IDirect3DTexture{,2}, IDirect3DViewport{,2,3},
//        IDirect3DLight, IDirect3DExecuteBuffer, IDirect3DVertexBuffer{,7}),
//        完美解决 IDirect3D + IDirect3D7 矛盾。
//     b) Wine d3dtypes.h 不定义 enum 类型 (只 typedef struct + #define 常量),
//        包裹到 namespace dx6 后与 d3d9types.h (全局) 不冲突。
//     c) Wine d3dtypes.h 中 __MSABI_LONG(...) 已被替换为 ((LONG)(...)) 以适配 Windows SDK。
//     d) Wine d3d.h 头部的 <d3dtypes.h> / <d3dcaps.h> 已改为项目内相对路径。
//
// 全局命名空间先 include Windows SDK d3d9types.h,确保 D3DMATERIAL9 / D3DCOLORVALUE 等
// DX9 类型在全局定义(供 IDirect3DMaterial3 等使用)。然后在 namespace dx6 内 include
// Wine d3dtypes.h + Wine d3d.h,把 DX5/DX6/DX7 接口隔离到 dx6。
#if !defined(DIRECT3D_VERSION) || DIRECT3D_VERSION < 0x0900
#undef DIRECT3D_VERSION
#define DIRECT3D_VERSION 0x0900
#endif
#include <d3d9types.h>

#include <ddraw.h>
#include <ddrawex.h>

// Phase 8.25: 在 namespace dx6 内 include 项目内 Wine d3d.h, 同时定义 DX5/DX6/DX7 接口。
//
// 1) 必须先 include Wine d3dtypes.h 再 include Wine d3d.h (Wine d3d.h 内部 <wine_d3dtypes.h>
//    路径已改为项目内文件,等价)。
// 2) Wine d3d.h 内部还 #include <objbase.h> 提供 DECLARE_INTERFACE_ / STDMETHOD 等 COM 宏。
// 3) Wine d3dtypes.h 内部 #include <windows.h> <float.h> <ddraw.h>,但 windows.h / ddraw.h
//    头保护已定义,会被跳过,不会污染 namespace dx6。
// 4) Wine d3d.h 顶部 #define COM_NO_WINDOWS_H 避免 objbase.h 重复引入 windows.h。
// 5) Wine d3d.h 用 `__WINE_D3D_H` 头保护,与 Windows SDK d3d.h 的 `_D3D_H_` 头保护
//    互不冲突,即使 Windows SDK d3d.h 在同 TU 内被 include 也不会跳过。
// Phase 8.25.4: 修复 CI #31/32 仍未生效的 typedef 缺失错误。
//
// 根因: wine_d3dtypes.h 中 typedef 块有多层嵌套保护:
//   - 外层 #ifndef DX_SHARED_DEFINES        (line 82)
//   - 内层 #ifndef D3DCOLOR_DEFINED         (line 86, typedef D3DCOLOR)
//   - 内层 #ifndef D3DVECTOR_DEFINED        (line 91, typedef D3DVECTOR)
//   - typedef D3DVALUE 没有 D3DVALUE_DEFINED 保护, 但依赖外层 DX_SHARED_DEFINES
// Windows SDK 的 d3d9types.h 自身也用 DX_SHARED_DEFINES / D3DCOLOR_DEFINED
// / D3DVECTOR_DEFINED 作为保护宏, 在 ddraw.h line 40 include 时已定义。
// 我们已经 #undef DX_SHARED_DEFINES, 但 d3d9types.h 内部 typedef D3DCOLOR
// (DX9 中是 typedef DWORD D3DCOLOR) 也会 #define D3DCOLOR_DEFINED,
// D3DVECTOR9 也会 #define D3DVECTOR_DEFINED, 所以 wine_d3dtypes.h 的内层
// #ifndef 块仍不执行。
//
// 解决: 在 include wine_d3dtypes.h 之前 #undef 所有相关保护宏, 强制 wine
// 头文件重新定义所有 typedef 到 namespace dx6 内。
namespace dx6 {
#undef DX_SHARED_DEFINES
#undef D3DCOLOR_DEFINED
#undef D3DVECTOR_DEFINED
#include "wine_d3dtypes.h"
#include "wine_d3dcaps.h"
#include "wine_d3d.h"
}

// Phase 8.25.1: 补充 Wine d3dtypes.h 缺失的 DX6/7 宏。
//
// 缺失原因: Wine d3dtypes.h 是 D3D5/6/7 早期版本, 部分 DX6 后期引入的宏(D3DFVF_PSIZE)
// 未定义, 但项目代码(如 IDirect3DDevice3.cpp line 191)需要使用。
#ifndef D3DFVF_PSIZE
#define D3DFVF_PSIZE         0x020  // 顶点包含点大小
#endif

#include "..\Common\Wrapper.h"
#include "..\Common\Logging.h"

typedef void(WINAPI *AcquireDDThreadLockProc)();
typedef HRESULT(WINAPI *D3DParseUnknownCommandProc)(LPVOID lpCmd, LPVOID *lpRetCmd);
typedef HRESULT(WINAPI *DDrawCreateProc)(GUID FAR *lpGUID, LPDIRECTDRAW FAR *lplpDD, IUnknown FAR *pUnkOuter);
typedef HRESULT(WINAPI *DirectDrawCreateClipperProc)(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, LPUNKNOWN pUnkOuter);
typedef HRESULT(WINAPI *DDrawEnumerateAProc)(LPDDENUMCALLBACKA lpCallback, LPVOID lpContext);
typedef HRESULT(WINAPI *DDrawEnumerateExAProc)(LPDDENUMCALLBACKEXA lpCallback, LPVOID lpContext, DWORD dwFlags);
typedef HRESULT(WINAPI *DDrawEnumerateExWProc)(LPDDENUMCALLBACKEXW lpCallback, LPVOID lpContext, DWORD dwFlags);
typedef HRESULT(WINAPI *DDrawEnumerateWProc)(LPDDENUMCALLBACKW lpCallback, LPVOID lpContext);
typedef HRESULT(WINAPI *DDrawCreateExProc)(GUID FAR *lpGUID, LPVOID *lplpDD, REFIID riid, IUnknown FAR *pUnkOuter);
typedef HRESULT(WINAPI *DllCanUnloadNowProc)();
typedef HRESULT(WINAPI *DllGetClassObjectProc)(REFCLSID rclsid, REFIID riid, LPVOID *ppv);
typedef HRESULT(WINAPI *GetSurfaceFromDCProc)(HDC hdc, LPDIRECTDRAWSURFACE7 *lpDDS);
typedef void(WINAPI *ReleaseDDThreadLockProc)();
typedef HRESULT(WINAPI *SetAppCompatDataProc)(DWORD, DWORD);

void genericDdrawQueryInterface(REFIID CalledID, LPVOID * ppvObj);
//#define genericQueryInterface(a,b) genericDdrawQueryInterface(a,b)
#define genericQueryInterface(a,b)
extern AddressLookupTable<void> ProxyAddressLookupTable;

#include "IDirect3D.h"
#include "IDirect3D2.h"
#include "IDirect3D3.h"
#include "IDirect3D7.h"
#include "IDirect3DDevice.h"
#include "IDirect3DDevice2.h"
#include "IDirect3DDevice3.h"
#include "IDirect3DDevice7.h"
#include "IDirect3DExecuteBuffer.h"
#include "IDirect3DLight.h"
#include "IDirect3DMaterial.h"
#include "IDirect3DMaterial2.h"
#include "IDirect3DMaterial3.h"
#include "IDirect3DTexture.h"
#include "IDirect3DTexture2.h"
#include "IDirect3DVertexBuffer.h"
#include "IDirect3DVertexBuffer7.h"
#include "IDirect3DViewport.h"
#include "IDirect3DViewport2.h"
#include "IDirect3DViewport3.h"
#include "IDirectDraw.h"
#include "IDirectDraw2.h"
#include "IDirectDraw3.h"
#include "IDirectDraw4.h"
#include "IDirectDraw7.h"
#include "IDirectDrawClipper.h"
#include "IDirectDrawColorControl.h"
#include "IDirectDrawEnumSurface.h"
#include "IDirectDrawFactory.h"
#include "IDirectDrawGammaControl.h"
#include "IDirectDrawPalette.h"
#include "IDirectDrawSurface.h"
#include "IDirectDrawSurface2.h"
#include "IDirectDrawSurface3.h"
#include "IDirectDrawSurface4.h"
#include "IDirectDrawSurface7.h"
