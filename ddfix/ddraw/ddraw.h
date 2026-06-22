#pragma once

#define INITGUID

// Phase 8.23: 解决 d3d.h (DX6) 在 DIRECT3D_VERSION=0x0900 时不定义 IDirect3D 问题。
//
// 背景:
//   1) d3d.h (DX6) 内部根据 DIRECT3D_VERSION 选择定义哪些接口:
//      * 0x0500 (DX5) -> IDirect3D, IDirect3DMaterial, IDirect3DTexture, IDirect3DViewport
//        等老接口
//      * 0x0600 (DX6) -> 在 0x0500 基础上加 IDirect3D2, IDirect3DTexture2
//      * 0x0700+ (DX7) -> 在 0x0600 基础上加 IDirect3D3, IDirect3D7 等
//      **任何 >=0x0700 都不再定义 IDirect3D** (DX5 老接口被淘汰)。
//   2) Phase 8.22 把 DIRECT3D_VERSION 强制设为 0x0900,导致 d3d.h 走 DX7+ 分支,
//      不再定义 IDirect3D。后续 IDirect3D.h 中 `class m_IDirect3D : public
//      dx6::IDirect3D` 编译失败 (CI #28: error C2039 'IDirect3D': is not a
//      member of 'dx6')。
//   3) Phase 8.22 同时还修复了 d3d9types.h 的 C3646 问题(必须 DIRECT3D_VERSION=
//      0x0900 才能让 d3d9types.h 完整展开,定义 D3DMATERIAL9/D3DCOLORVALUE)。
//
// 修复 (Phase 8.23):
//   1) 先强制 DIRECT3D_VERSION=0x0900 并 #include <d3d9types.h>,让 DX9 类型
//      在全局命名空间完整定义 (D3DMATERIAL9, D3DCOLORVALUE, D3DDEVTYPE 等)
//   2) 临时把 DIRECT3D_VERSION 改回 0x0600 (DX6),让 d3d.h 在 dx6 namespace 内
//      走 DX6 分支,定义 IDirect3D / IDirect3D2 / IDirect3DMaterial 等
//   3) 恢复 DIRECT3D_VERSION=0x0900,让后续 d3d9.h / d3dx9.h 走 DX9 分支
#if !defined(DIRECT3D_VERSION) || DIRECT3D_VERSION < 0x0900
#undef DIRECT3D_VERSION
#define DIRECT3D_VERSION 0x0900
#endif
#include <d3d9types.h>

#include <ddraw.h>
#include <ddrawex.h>

// Phase 8.19: 把 d3d.h (含 d3dtypes.h) 包裹到 dx6 命名空间,避开与 d3d9.h (含
// d3d9types.h) 的 enum type 重定义冲突 (C2011)。
//
// 根因：
//   d3dtypes.h 定义了 _D3DFILLMODE / _D3DBLEND / _D3DSHADEMODE / _D3DLIGHTTYPE /
//   _D3DTEXTUREADDRESS / _D3DCULL / _D3DCMPFUNC / _D3DSTENCILOP / _D3DFOGMODE 等
//   枚举类型，d3d9types.h 也定义了同名同值枚举。两个头文件无法在同一个 TU 的
//   全局命名空间共存（C2011 'enum' type redefinition）。
//
// 解决：
//   把 d3d.h (DX6/7 接口 + d3dtypes.h 的所有内容) 整个放进 dx6:: 命名空间。
//   ddraw.h / ddrawex.h 不含冲突枚举，仍在全局命名空间。
//   d3d9.h (含 d3d9types.h) 在全局/ ND3D9:: 命名空间，不再与 dx6:: 冲突。
//
// 代价：所有用到 IDirect3D* / LPDIRECT3D* / D3DVERTEX / D3DMATRIX 等 DX6/7
//       类型的代码必须加 dx6:: 前缀。d3dtypes.h 内的回调类型
//       (LPD3DENUMDEVICESCALLBACK 等) 和 struct (D3DFINDDEVICESEARCH 等) 也
//       全部进入 dx6::。
//
// Phase 8.23: 临时切到 0x0600,让 d3d.h 走 DX6 分支定义 IDirect3D (DX5 老接口)。
//   之后恢复 0x0900,让后续 d3d9.h 走 DX9 分支。
#undef DIRECT3D_VERSION
#define DIRECT3D_VERSION 0x0600
namespace dx6 {
#include <d3d.h>
}
#undef DIRECT3D_VERSION
#define DIRECT3D_VERSION 0x0900

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
