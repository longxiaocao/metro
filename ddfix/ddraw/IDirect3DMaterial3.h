#pragma once

// Phase 8.22: ddraw.h 顶部已经先 include d3d9types.h (DIRECT3D_VERSION=0x0900),
//   全局 D3DMATERIAL9 / D3DCOLORVALUE 已完整定义,本头文件不再需要重复 include。
//
// 历史:
//   Phase 8.20: 移除 ND3D9 前向声明,改 #include <d3d9types.h> → 失败
//     (CI #26): 当时 d3d.h (dx6 namespace 内) 已 define DIRECT3D_VERSION=0x0600,
//     d3d9types.h 头部 #if (DIRECT3D_VERSION >= 0x0900) 把整个头文件跳过。
//   Phase 8.21: include d3d9types.h 前 #undef+重新 #define 0x0900 → 失败
//     (CI #27): 行号与 CI #25 一致(173-189),_D3DMATERIAL9 仍被识别为 class,
//     推测是 d3d9types.h 内部 _D3DMATERIAL9 定义时,本 TU 已存在其他形式的
//     _D3DMATERIAL9 声明(class 或 typedef)导致重复定义冲突。
//   Phase 8.22 (本版): 不在 h 文件 include d3d9types.h,改在 ddraw.h 顶部统一处理
//     (见 ddraw.h 的 Phase 8.22 注释),确保 d3d.h 包裹到 dx6 namespace 之前,
//     d3d9types.h 已在全局命名空间以 struct 形式完整定义 _D3DMATERIAL9。

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
