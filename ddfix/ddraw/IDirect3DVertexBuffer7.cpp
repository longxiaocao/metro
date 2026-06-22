/**
* Copyright (C) 2017 Elisha Riedlinger
*
* This software is  provided 'as-is', without any express  or implied  warranty. In no event will the
* authors be held liable for any damages arising from the use of this software.
* Permission  is granted  to anyone  to use  this software  for  any  purpose,  including  commercial
* applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*   1. The origin of this software must not be misrepresented; you must not claim that you  wrote the
*      original  software. If you use this  software  in a product, an  acknowledgment in the product
*      documentation would be appreciated but is not required.
*   2. Altered source versions must  be plainly  marked as such, and  must not be  misrepresented  as
*      being the original software.
*   3. This notice may not be removed or altered from any source distribution.
*/

#include "ddraw.h"

HRESULT m_IDirect3DVertexBuffer7::QueryInterface(REFIID riid, LPVOID * ppvObj)
{
	if ((riid == dx6::IID_IDirect3DVertexBuffer7 || riid == IID_IUnknown) && ppvObj)
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	// P0 修复 (Task 1.2): ProxyInterface 可能为 nullptr。代理空时不再 forward QueryInterface。
	if (!ProxyInterface)
	{
		return E_NOINTERFACE;
	}

	HRESULT hr = ProxyInterface->QueryInterface(riid, ppvObj);

	if (SUCCEEDED(hr))
	{
		genericQueryInterface(riid, ppvObj);
	}

	return hr;
}

ULONG m_IDirect3DVertexBuffer7::AddRef()
{
	// P0 修复 (Task 1.2): 当 ProxyInterface 为 nullptr 时不能用 ProxyInterface->AddRef()，自管 Refs。
	if (ProxyInterface)
	{
		return ProxyInterface->AddRef();
	}
	return ++m_SelfRefs;
}

ULONG m_IDirect3DVertexBuffer7::Release()
{
	// P0 修复 (Task 1.2): 当 ProxyInterface 为 nullptr 时不能用 ProxyInterface->Release()，自管 Refs。
	if (ProxyInterface)
	{
		ULONG x = ProxyInterface->Release();
		if (x == 0)
		{
			delete this;
		}
		return x;
	}

	ULONG x = --m_SelfRefs;
	if (x == 0)
	{
		delete this;
	}
	return x;
}

HRESULT m_IDirect3DVertexBuffer7::Lock(DWORD a, LPVOID * b, LPDWORD c)
{
	// P0 修复 (Task 1.2): 有 D3D9 缓冲时优先用 D3D9 Lock。
	// DX6 Lock(DWORD dwFlags, LPVOID *lplpVoid, LPDWORD lpdwSize) 语义：拿数据指针 + 大小。
	// D3D9 IDirect3DVertexBuffer9::Lock(UINT Offset, UINT Size, VOID **ppData, DWORD Flags) 不同。
	// 这里 0,0 锁整段，把 size 通过 GetDesc 取出来回填。
	if (m_vertexBuffer9Handle)
	{
		auto vb9 = ND3D9::D3D9Context::Instance()->GetResource9<IDirect3DVertexBuffer9>(m_vertexBuffer9Handle, nullptr);
		if (vb9)
		{
			HRESULT hr = vb9->Lock(0, 0, (VOID**)b, a);
			if (SUCCEEDED(hr) && c)
			{
				D3DVERTEXBUFFER_DESC desc;
				if (SUCCEEDED(vb9->GetDesc(&desc)))
				{
					*c = desc.Size;
				}
				else
				{
					*c = 0;
				}
			}
			return hr;
		}
	}
	return ProxyInterface->Lock(a, b, c);
}

HRESULT m_IDirect3DVertexBuffer7::Unlock()
{
	// P0 修复 (Task 1.2): 有 D3D9 缓冲时优先用 D3D9 Unlock。
	if (m_vertexBuffer9Handle)
	{
		auto vb9 = ND3D9::D3D9Context::Instance()->GetResource9<IDirect3DVertexBuffer9>(m_vertexBuffer9Handle, nullptr);
		if (vb9)
		{
			return vb9->Unlock();
		}
	}
	return ProxyInterface->Unlock();
}

HRESULT m_IDirect3DVertexBuffer7::ProcessVertices(DWORD a, DWORD b, DWORD c, dx6::LPDIRECT3DVERTEXBUFFER7 d, DWORD e, dx6::LPDIRECT3DDEVICE7 f, DWORD g)
{
	if (d)
	{
		d = static_cast<m_IDirect3DVertexBuffer7 *>(d)->GetProxyInterface();
	}
	if (f)
	{
		f = static_cast<m_IDirect3DDevice7 *>(f)->GetProxyInterface();
	}

	return ProxyInterface->ProcessVertices(a, b, c, d, e, f, g);
}

HRESULT m_IDirect3DVertexBuffer7::GetVertexBufferDesc(dx6::LPD3DVERTEXBUFFERDESC a)
{
	return ProxyInterface->GetVertexBufferDesc(a);
}

HRESULT m_IDirect3DVertexBuffer7::Optimize(dx6::LPDIRECT3DDEVICE7 a, DWORD b)
{
	if (a)
	{
		a = static_cast<m_IDirect3DDevice7 *>(a)->GetProxyInterface();
	}

	return ProxyInterface->Optimize(a, b);
}

HRESULT m_IDirect3DVertexBuffer7::ProcessVerticesStrided(DWORD a, DWORD b, DWORD c, dx6::LPD3DDRAWPRIMITIVESTRIDEDDATA d, DWORD e, dx6::LPDIRECT3DDEVICE7 f, DWORD g)
{
	if (f)
	{
		f = static_cast<m_IDirect3DDevice7 *>(f)->GetProxyInterface();
	}

	return ProxyInterface->ProcessVerticesStrided(a, b, c, d, e, f, g);
}
