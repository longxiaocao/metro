#pragma once

#define INITGUID

// Phase 8.24: 解决 d3d.h 在不同 DIRECT3D_VERSION 下定义不同接口的问题。
//
// 背景:
//   1) d3d.h (DX6) 内部根据 DIRECT3D_VERSION 选择定义哪些接口:
//      * 0x0600 (DX6) -> IDirect3D, IDirect3D2, IDirect3D3, IDirect3DMaterial*,
//        IDirect3DTexture*, IDirect3DDevice*, IDirect3DViewport*, IDirect3DLight,
//        IDirect3DExecuteBuffer 等
//      * 0x0700 (DX7) -> 在 DX6 基础上加 IDirect3D7, IDirect3DDevice7,
//        IDirect3DVertexBuffer, IDirect3DVertexBuffer7 等
//      * < 0x0500    -> IDirect3D, IDirect3DMaterial 等 DX5 基础接口
//      **0x0600 不定义 IDirect3D7**; **0x0700 不定义 IDirect3D**
//   2) 代码库需要同时使用 IDirect3D (DX5) 和 IDirect3D7 (DX7),
//      必须在 dx6 namespace 内分两次 include d3d.h,中间 #undef 头保护。
//   3) Phase 8.22 (单次 include, DIRECT3D_VERSION=0x0900) 失败原因:
//      d3d.h 走 DX7+ 分支,不定义 IDirect3D (CI #28)。
//   4) Phase 8.23 (单次 include, DIRECT3D_VERSION=0x0600) 失败原因:
//      d3d.h 走 DX6 分支,不定义 IDirect3D7 (CI #29)。
//
// 修复 (Phase 8.24):
//   1) 先强制 DIRECT3D_VERSION=0x0900 并 #include <d3d9types.h>,让 DX9 类型
//      在全局命名空间完整定义 (D3DMATERIAL9, D3DCOLORVALUE, D3DDEVTYPE 等)。
//   2) 临时 DIRECT3D_VERSION=0x0600 + #include d3d.h (dx6 namespace 内):
//      定义 IDirect3D, IDirect3D2, IDirect3D3, IDirect3DMaterial* 等 DX5/DX6 接口。
//   3) #undef d3d.h 头保护 + 临时 DIRECT3D_VERSION=0x0700 + 再次 #include d3d.h:
//      定义 IDirect3D7, IDirect3DDevice7, IDirect3DVertexBuffer, IDirect3DVertexBuffer7
//      等 DX7 接口。
//   4) 恢复 DIRECT3D_VERSION=0x0900,让后续 d3d9.h / d3dx9.h 走 DX9 分支。
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
// Phase 8.24: 分两次 include d3d.h,中间 #undef 头保护。两次都包在 dx6
//   namespace 内,避免枚举冲突 (C2011)。头保护名 _D3D_H_ 来自 Windows SDK
//   d3d.h;若保护名不同,#undef 是无害的(无操作)。
#pragma push_macro("_D3D_H_")
#pragma push_macro("__D3D_H__")
#pragma push_macro("_D3DH_")
#pragma push_macro("__D3DH__")
#undef DIRECT3D_VERSION
#define DIRECT3D_VERSION 0x0600
namespace dx6 {
#include <d3d.h>
}
#undef _D3D_H_
#undef __D3D_H__
#undef _D3DH_
#undef __D3DH__
#undef DIRECT3D_VERSION
#define DIRECT3D_VERSION 0x0700
namespace dx6 {
#include <d3d.h>
}
#pragma pop_macro("_D3D_H_")
#pragma pop_macro("__D3D_H__")
#pragma pop_macro("_D3DH_")
#pragma pop_macro("__D3DH__")
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
