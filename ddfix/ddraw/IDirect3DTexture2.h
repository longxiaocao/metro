#pragma once

// Phase 8.25.7: include d3d9.h 提供 IDirect3DTexture9 完整类型定义。
//
// 背景: IDirect3DTexture2.h 被 ddraw.h (line 140) include, 但 ddraw.h 不 include
//   d3d9.h (只在 D3D9Context.h 中 include), 所以在 IDirect3DTexture2.h 编译时,
//   全局命名空间没有 IDirect3DTexture9 完整定义, 只有 namespace ND3D9 内的
//   前向声明 `struct IDirect3DTexture9;`。
//   SmartPtr<ND3D9::IDirect3DTexture9> 需要完整类型 (operator->/析构 调用
//   AddRef/Release), 编译报 C2923/C2065。
//
// 修复: 在 IDirect3DTexture2.h 顶部直接 include <d3d9.h>。
//   - d3d9.h 有头保护 #pragma once, 多次 include 安全;
//   - d3d9.h 头部会 #include <d3d9types.h>, 全局 IDirect3DTexture9 可用;
//   - DIRECT3D_VERSION 已经在 ddraw.h (line 53) 全局设为 0x0900, d3d9.h 内容
//     不会被跳过。
//   - namespace ND3D9 内的前向声明 `struct IDirect3DTexture9;` 仍然存在,
//     但因为 d3d9.h 也在全局定义了 IDirect3DTexture9, 全局查找优先。
#include <d3d9.h>

namespace ND3D9
{
	using Resource9Handle = int;
}

class m_IDirect3DTexture2 : public dx6::IDirect3DTexture2, public AddressLookupTableObject
{
private:
	dx6::IDirect3DTexture2 *ProxyInterface;
	ULONG Refs;
	m_IDirectDrawSurface4* m_surface;
	ND3D9::Resource9Handle m_tex9Handle;

public:
	m_IDirect3DTexture2(dx6::IDirect3DTexture2 *aOriginal, m_IDirectDrawSurface4 *temp);
	~m_IDirect3DTexture2();

	dx6::IDirect3DTexture2 *GetProxyInterface() { return ProxyInterface; }
	SmartPtr<IDirect3DTexture9> GetTexture9() const;

	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj);
	STDMETHOD_(ULONG, AddRef)(THIS);
	STDMETHOD_(ULONG, Release)(THIS);

	/*** dx6::IDirect3DTexture2 methods ***/
	STDMETHOD(GetHandle)(THIS_ dx6::LPDIRECT3DDEVICE2, dx6::LPD3DTEXTUREHANDLE);
	STDMETHOD(PaletteChanged)(THIS_ DWORD, DWORD);
	STDMETHOD(Load)(THIS_ dx6::LPDIRECT3DTEXTURE2);
};
