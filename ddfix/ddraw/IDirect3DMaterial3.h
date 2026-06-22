#pragma once

// Phase 8.21: 修复 d3d9types.h 被 DIRECT3D_VERSION 保护跳过,导致
//   D3DMATERIAL9 / D3DCOLORVALUE / D3DDEVTYPE 等类型未定义的问题。
//
// 根因链:
//   1) ddraw.h 包含 <d3d.h> (在 dx6 namespace 内),d3d.h 自身会
//      #define DIRECT3D_VERSION 0x0600 (DX6),这个 #define 是全局的
//      (#define 不受 namespace 约束)。
//   2) <d3d9types.h> / <d3d9caps.h> / <d3d9.h> 头部都有
//      #if (DIRECT3D_VERSION >= 0x0900) 守护,版本不足时整个头文件被 #if/#endif 跳过,
//      所有 D3D9 类型/接口都不展开。
//   3) D3D9Context.h 通过 #undef+重新 #define 强制 DIRECT3D_VERSION=0x0900
//      解决了它自己 TU 的问题,但 IDirect3DMaterial3.h 在 D3D9Context.h 之前
//      被 ddraw.h 间接 include,DIRECT3D_VERSION 还是 0x0600。
//   4) 因此在 IDirect3DMaterial3.h 直接 #include <d3d9types.h> 时,
//      d3d9types.h 整个被跳过,D3DMATERIAL9 / D3DCOLORVALUE 都未定义
//      (CI #26 错误:d3d9types.h 跳过 → d3d9caps.h D3DDEVTYPE 未定义 →
//       d3d9.h D3DCOLOR 未定义 → IDirect3DMaterial3.h:39 报 C2061)。
//
// 修复:include d3d9types.h 之前强制覆盖 DIRECT3D_VERSION = 0x0900,
//      与 D3D9Context.h 保持一致。d3dtypes.h 的枚举 (DX6) 已在 ddraw.h 的
//      dx6 namespace 内,d3d9types.h 枚举 (DX9) 在全局,两个 namespace 互不冲突。
#if !defined(DIRECT3D_VERSION) || DIRECT3D_VERSION < 0x0900
#undef DIRECT3D_VERSION
#define DIRECT3D_VERSION 0x0900
#endif
#include <d3d9types.h>

class m_IDirect3DMaterial3 : public dx6::IDirect3DMaterial3, public AddressLookupTableObject
{
private:
	dx6::IDirect3DMaterial3 *ProxyInterface;
	ULONG Refs;
	dx6::D3DMATERIAL m_matDef;
public:
	m_IDirect3DMaterial3(dx6::IDirect3DMaterial3 *aOriginal, void *temp)
		: ProxyInterface(aOriginal)
		, Refs(1)
		, m_matDef({0})
	{
		ProxyAddressLookupTable.SaveAddress(this, ProxyInterface);
	}
	~m_IDirect3DMaterial3()
	{
		ProxyAddressLookupTable.DeleteAddress(this);
	}

	dx6::IDirect3DMaterial3 *GetProxyInterface() { return ProxyInterface; }
	void GetMaterial9(D3DMATERIAL9* mat9) const;

	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj);
	STDMETHOD_(ULONG, AddRef)(THIS);
	STDMETHOD_(ULONG, Release)(THIS);

	/*** dx6::IDirect3DMaterial3 methods ***/
	STDMETHOD(SetMaterial)(THIS_ dx6::LPD3DMATERIAL);
	STDMETHOD(GetMaterial)(THIS_ dx6::LPD3DMATERIAL);
	STDMETHOD(GetHandle)(THIS_ dx6::LPDIRECT3DDEVICE3, dx6::LPD3DMATERIALHANDLE);
};
