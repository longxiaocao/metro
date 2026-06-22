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

HRESULT m_IDirect3DDevice::QueryInterface(REFIID riid, LPVOID * ppvObj)
{
	if ((riid == dx6::IID_IDirect3DDevice || riid == IID_IUnknown) && ppvObj)
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	HRESULT hr = ProxyInterface->QueryInterface(riid, ppvObj);

	if (SUCCEEDED(hr))
	{
		genericQueryInterface(riid, ppvObj);
	}

	return hr;
}

ULONG m_IDirect3DDevice::AddRef()
{
	return ProxyInterface->AddRef();
}

ULONG m_IDirect3DDevice::Release()
{
	ULONG x = ProxyInterface->Release();

	if (x == 0)
	{
		delete this;
	}

	return x;
}

HRESULT m_IDirect3DDevice::Initialize(dx6::LPDIRECT3D a, LPGUID b, dx6::LPD3DDEVICEDESC c)
{
	if (a)
	{
		a = static_cast<m_IDirect3D *>(a)->GetProxyInterface();
	}

	return ProxyInterface->Initialize(a, b, c);
}

HRESULT m_IDirect3DDevice::GetCaps(dx6::LPD3DDEVICEDESC a, dx6::LPD3DDEVICEDESC b)
{
	return ProxyInterface->GetCaps(a, b);
}

HRESULT m_IDirect3DDevice::SwapTextureHandles(dx6::LPDIRECT3DTEXTURE a, dx6::LPDIRECT3DTEXTURE b)
{
	if (a)
	{
		a = static_cast<m_IDirect3DTexture *>(a)->GetProxyInterface();
	}
	if (b)
	{
		b = static_cast<m_IDirect3DTexture *>(b)->GetProxyInterface();
	}

	return ProxyInterface->SwapTextureHandles(a, b);
}

HRESULT m_IDirect3DDevice::CreateExecuteBuffer(LPD3DEXECUTEBUFFERDESC a, dx6::LPDIRECT3DEXECUTEBUFFER * b, IUnknown * c)
{
	HRESULT hr = ProxyInterface->CreateExecuteBuffer(a, b, c);

	if (SUCCEEDED(hr))
	{
		*b = ProxyAddressLookupTable.FindAddress<m_IDirect3DExecuteBuffer>(*b);
	}

	return hr;
}

HRESULT m_IDirect3DDevice::GetStats(dx6::LPD3DSTATS a)
{
	return ProxyInterface->GetStats(a);
}

HRESULT m_IDirect3DDevice::Execute(dx6::LPDIRECT3DEXECUTEBUFFER a, dx6::LPDIRECT3DVIEWPORT b, DWORD c)
{
	if (a)
	{
		a = static_cast<m_IDirect3DExecuteBuffer *>(a)->GetProxyInterface();
	}
	if (b)
	{
		b = static_cast<m_IDirect3DViewport *>(b)->GetProxyInterface();
	}

	return ProxyInterface->Execute(a, b, c);
}

HRESULT m_IDirect3DDevice::AddViewport(dx6::LPDIRECT3DVIEWPORT a)
{
	if (a)
	{
		a = static_cast<m_IDirect3DViewport *>(a)->GetProxyInterface();
	}

	return ProxyInterface->AddViewport(a);
}

HRESULT m_IDirect3DDevice::DeleteViewport(dx6::LPDIRECT3DVIEWPORT a)
{
	if (a)
	{
		a = static_cast<m_IDirect3DViewport *>(a)->GetProxyInterface();
	}

	return ProxyInterface->DeleteViewport(a);
}

HRESULT m_IDirect3DDevice::NextViewport(dx6::LPDIRECT3DVIEWPORT a, dx6::LPDIRECT3DVIEWPORT * b, DWORD c)
{
	if (a)
	{
		a = static_cast<m_IDirect3DViewport *>(a)->GetProxyInterface();
	}

	HRESULT hr = ProxyInterface->NextViewport(a, b, c);

	if (SUCCEEDED(hr))
	{
		*b = ProxyAddressLookupTable.FindAddress<m_IDirect3DViewport>(*b);
	}

	return hr;
}

HRESULT m_IDirect3DDevice::Pick(dx6::LPDIRECT3DEXECUTEBUFFER a, dx6::LPDIRECT3DVIEWPORT b, DWORD c, LPD3DRECT d)
{
	if (a)
	{
		a = static_cast<m_IDirect3DExecuteBuffer *>(a)->GetProxyInterface();
	}
	if (b)
	{
		b = static_cast<m_IDirect3DViewport *>(b)->GetProxyInterface();
	}

	return ProxyInterface->Pick(a, b, c, d);
}

HRESULT m_IDirect3DDevice::GetPickRecords(LPDWORD a, LPD3DPICKRECORD b)
{
	return ProxyInterface->GetPickRecords(a, b);
}

HRESULT m_IDirect3DDevice::EnumTextureFormats(LPD3DENUMTEXTUREFORMATSCALLBACK a, LPVOID b)
{
	return ProxyInterface->EnumTextureFormats(a, b);
}

HRESULT m_IDirect3DDevice::CreateMatrix(LPD3DMATRIXHANDLE a)
{
	return ProxyInterface->CreateMatrix(a);
}

HRESULT m_IDirect3DDevice::SetMatrix(D3DMATRIXHANDLE a, const dx6::LPD3DMATRIX b)
{
	return ProxyInterface->SetMatrix(a, b);
}

HRESULT m_IDirect3DDevice::GetMatrix(D3DMATRIXHANDLE a, dx6::LPD3DMATRIX b)
{
	return ProxyInterface->GetMatrix(a, b);
}

HRESULT m_IDirect3DDevice::DeleteMatrix(D3DMATRIXHANDLE a)
{
	return ProxyInterface->DeleteMatrix(a);
}

HRESULT m_IDirect3DDevice::BeginScene()
{
	return ProxyInterface->BeginScene();
}

HRESULT m_IDirect3DDevice::EndScene()
{
	return ProxyInterface->EndScene();
}

HRESULT m_IDirect3DDevice::GetDirect3D(dx6::LPDIRECT3D * a)
{
	HRESULT hr = ProxyInterface->GetDirect3D(a);

	if (SUCCEEDED(hr))
	{
		*a = ProxyAddressLookupTable.FindAddress<m_IDirect3D>(*a);
	}

	return hr;
}
