#pragma once

#include "../D3D9Context.h"

class m_IDirect3DVertexBuffer7 : public IDirect3DVertexBuffer7, public AddressLookupTableObject
{
private:
	IDirect3DVertexBuffer7 *ProxyInterface;
	// P0 修复 (Task 1.2): 新增 D3D9 顶点缓冲后端。
	// 当 ProxyInterface 为 nullptr 时，Lock/Unlock 走 m_vertexBuffer9Handle。
	// m_vertexBuffer9Handle 来自 D3D9Context::CreateVertexBuffer9，0 表示未使用。
	ND3D9::Resource9Handle m_vertexBuffer9Handle;
	// P0 修复 (Task 1.2): 自管 Refs，仅在 ProxyInterface 为 nullptr 时使用。
	ULONG m_SelfRefs;

public:
	// 第二个参数 temp 复用作 Resource9Handle 槽位 (P0 fix)：
	//   - 旧用法：new m_IDirect3DVertexBuffer7(proxyVb7, nullptr)
	//   - 新用法：new m_IDirect3DVertexBuffer7(nullptr, (void*)handle)
	// Resource9Handle 是 int，void* 在 x86/x64 都能安全容纳。
	m_IDirect3DVertexBuffer7(IDirect3DVertexBuffer7 *aOriginal, void *temp)
		: ProxyInterface(aOriginal)
		, m_vertexBuffer9Handle(temp ? (ND3D9::Resource9Handle)(INT_PTR)temp : 0)
		, m_SelfRefs(1)
	{
		// 兼容旧路径：仅有 ProxyInterface 时仍 SaveAddress。
		if (ProxyInterface)
		{
			ProxyAddressLookupTable.SaveAddress(this, ProxyInterface);
		}
	}
	~m_IDirect3DVertexBuffer7()
	{
		if (ProxyInterface)
		{
			ProxyAddressLookupTable.DeleteAddress(this);
		}
		// m_vertexBuffer9Handle 由 D3D9Context 跟踪；Game 端的 Release() 走 ProxyInterface 路径时
		// 会在 m_IDirect3DDevice3::DrawPrimitiveVB 之类的地方访问，暂不主动释放 9 资源（保留至 D3D9Context 析构）。
	}

	IDirect3DVertexBuffer7 *GetProxyInterface() { return ProxyInterface; }

	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj);
	STDMETHOD_(ULONG, AddRef)(THIS);
	STDMETHOD_(ULONG, Release)(THIS);

	/*** IDirect3DVertexBuffer7 methods ***/
	STDMETHOD(Lock)(THIS_ DWORD, LPVOID*, LPDWORD);
	STDMETHOD(Unlock)(THIS);
	STDMETHOD(ProcessVertices)(THIS_ DWORD, DWORD, DWORD, LPDIRECT3DVERTEXBUFFER7, DWORD, LPDIRECT3DDEVICE7, DWORD);
	STDMETHOD(GetVertexBufferDesc)(THIS_ LPD3DVERTEXBUFFERDESC);
	STDMETHOD(Optimize)(THIS_ LPDIRECT3DDEVICE7, DWORD);
	STDMETHOD(ProcessVerticesStrided)(THIS_ DWORD, DWORD, DWORD, LPD3DDRAWPRIMITIVESTRIDEDDATA, DWORD, LPDIRECT3DDEVICE7, DWORD);
};
