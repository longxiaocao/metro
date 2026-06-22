#pragma once

// Phase 8.20: 修复 D3DMATERIAL9 结构在 d3d9types.h 编译失败（C3646 / C4430）问题。
//
// 根因：原代码用 `namespace ND3D9 { typedef struct _D3DMATERIAL9 D3DMATERIAL9; }`
//       做前向声明，意图给 `ND3D9::D3DMATERIAL9*` 提供类型签名。但这会在
//       d3d9types.h 编译 _D3DMATERIAL9 结构体之前，让编译器认为 `_D3DMATERIAL9`
//       已经是 class 类型（C++ 前向声明默认为 class），导致 d3d9types.h 后续的
//       `struct _D3DMATERIAL9 { D3DCOLORVALUE Diffuse; ... }` 把 Diffuse 等
//       字段当成 class override 成员（C3646 'Diffuse': unknown override specifier）。
//
// 修复：移除前向声明，直接 include d3d9types.h（d3dtypes.h 已被 ddraw.h 包裹到
//       dx6 命名空间，避免了 _D3DFILLMODE / _D3DBLEND 等枚举重定义），让 d3d9types.h
//       在全局命名空间完整定义 D3DMATERIAL9 / D3DCOLORVALUE 等类型。
//       调用方（IDirect3DDevice3.cpp）原本用 `ND3D9::D3DMATERIAL9 mat9` 局部变量，
//       改为直接 `D3DMATERIAL9 mat9`（d3d9types.h 在全局命名空间）。
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
