#pragma once

#define INITGUID

#include <ddraw.h>
#include <ddrawex.h>

// Phase 8.19: 把 d3d.h (含 d3dtypes.h) 包裹到 dx6 命名空间，避开与 d3d9.h (含
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
namespace dx6 {
#include <d3d.h>
}

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
